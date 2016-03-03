/*
*   2016.2.25
*
*   Copyright arCookie. All rights reserved.
*
*   The license under which the Mob source code is released is the GPLv2 (or later) from the Free Software Foundation.
*
*   A copy of the license is included with every copy of Mob source code, but you can also read the text of the license here(http://www.arcookie.com/?page_id=414).
*
****************************************************************************************
*
*   Copyright AllSeen Alliance. All rights reserved.
*
*   Permission to use, copy, modify, and/or distribute this software for any
*   purpose with or without fee is hereby granted, provided that the above
*   copyright notice and this permission notice appear in all copies.
*
*   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
*   WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
*   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
*   ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
*   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
*   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
*   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <time.h>
#include <string>
#include <qcc/StringUtil.h>

#include "mob.h"
#include "Sender.h"
#include "AlljoynMob.h"

struct find_id : std::unary_function<RECEIVE, bool> {
	int snum;
	qcc::String uid;
	find_id(qcc::String & u, int sn) :uid(u), snum(sn) { }
	bool operator()(RECEIVE const& m) const {
		return (m.uid == uid && m.sn_s <= snum && m.sn_e >= snum);
	}
};

struct find_id_applies : std::unary_function<RECEIVE, bool> {
	int snum_p;
	qcc::String uid_p;
	find_id_applies(const char * u, int sn) :uid_p(u), snum_p(sn) { }
	bool operator()(APPLIES const& m) const {
		return (m.uid_p == uid_p && m.snum_p == snum_p);
	}
};

CSender::CSender(CAlljoynMob * pMob, BusAttachment& bus, const char* path) : m_pMob(pMob), BusObject(path), m_pMobSignalMember(NULL)
{
	QStatus status;

	/* Add the mob interface to this object */
	const InterfaceDescription* mobIntf = bus.GetInterface(MOB_SERVICE_INTERFACE_NAME);
	assert(mobIntf);
	AddInterface(*mobIntf);

	/* Store the mob signal member away so it can be quickly looked up when signals are sent */
	m_pMobSignalMember = mobIntf->GetMember("Mob");
	assert(m_pMobSignalMember);

	/* Register signal handler */
	status = bus.RegisterSignalHandler(this,
		static_cast<MessageReceiver::SignalHandler>(&CSender::OnRecvData),
		m_pMobSignalMember,
		NULL);

	if (ER_OK != status) {
		printf("Failed to register signal handler for CSender::Mob (%s)\n", QCC_StatusText(status));
	}
}

CSender::~CSender()
{
	mReceives::iterator miter;
	vReceives::iterator viter;

	for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {
		for (viter = miter->second.begin(); viter != miter->second.end(); viter++) {
			delete (*viter).data;
		}
	}
}

void CSender::Save(SessionId nSID, const char * sText, int nLength, const char * pExtra, int nExtLen)
{
	RECEIVE rcv;
	mReceives::iterator miter;
	vReceives::iterator viter;
	SYNC_DATA * pSD = (SYNC_DATA *)pExtra;

	rcv.uid_p = pSD->uid_p;
	rcv.uid = pSD->uid;
	rcv.snum_p = pSD->snum_p;
	rcv.snum = pSD->snum;
	rcv.sn_s = pSD->sn;
	rcv.sn_e = pSD->sn;
	rcv.data = sText;

	m_mReceives[pSD->base].push_back(rcv);

	for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {
		for (viter = miter->second.begin(); viter != miter->second.end(); viter++) {
			if ((*viter).sn_s != (*viter).sn_e && (*viter).sn_e > 1 && std::find_if(miter->second.begin(), miter->second.end(), find_id((*viter).uid, (*viter).sn_e - 1)) == miter->second.end()) return;
		}
	}

	Apply(nSID);
}

BOOL CSender::PushApply(vApplies & applies, const char * base, const char * uid_p, int snum_p, BOOL bFirst)
{
	APPLY apply;
	vApplies::iterator viter;

	if ((viter = std::find_if(applies.begin(), applies.end(), find_id_applies(uid_p, snum_p))) != applies.end()) (*viter).applies.insert(apply);
	else if (bFirst) {
		APPLIES appl;

		appl.applies.insert(apply);
		applies.push_back(appl);
		bFirst = FALSE;
	}
	return TRUE;
}

void CSender::Apply(SessionId nSID)
{
	BOOL bFirst;
	int num, n;
	BOOL bWorked;
	APPLY apply;
	SYNC_DATA sd;
	sApplies::iterator siter;
	mApplies::iterator miter;
	vApplies::iterator viter;
	mReceives::iterator mRcvIt;
	vReceives::iterator vRcvIt;

	for (mRcvIt = m_mReceives.begin(); mRcvIt != m_mReceives.end(); mRcvIt++) {
		vApplies applies;

		for (vRcvIt = mRcvIt->second.begin(); vRcvIt != mRcvIt->second.end();) {
			if ((*vRcvIt).data && (n = mob_find_parent_db(nSID, (*vRcvIt).uid_p.data(), (*vRcvIt).snum_p, mRcvIt->first.data())) > num) num = n;
		}

		while ((num = mob_get_db(nSID, num, mRcvIt->first.data(), &sd)) > 0) PushApply(applies, mRcvIt->first.data(), sd.uid_p, sd.snum_p, TRUE);

		bFirst = TRUE;

		do {
			bWorked = FALSE;
			for (vRcvIt = mRcvIt->second.begin(); vRcvIt != mRcvIt->second.end();) {
				if ((*vRcvIt).data && PushApply(applies, mRcvIt->first.data(), (*vRcvIt).uid_p.data(), (*vRcvIt).snum_p, bFirst)) {
					bFirst = FALSE;
					(*vRcvIt).data = NULL;

					vReceives::iterator it = std::find_if(mRcvIt->second.begin(), mRcvIt->second.end(), find_id((*vRcvIt).uid, (*vRcvIt).sn_e - 1));

					if (it != mRcvIt->second.end()) {
						(*it).sn_e = (*vRcvIt).sn_e;
						vRcvIt = mRcvIt->second.erase(vRcvIt);
					}
					else vRcvIt++;

					bWorked = TRUE;
				}
				else vRcvIt++;
			}
		} while (bWorked);

		vApplies::iterator viter = applies.begin();

		if (viter != applies.end()) {
			mob_undo_db(nSID, (*viter).uid_p.data(), (*viter).snum_p, mRcvIt->first.data());

			for (; viter != applies.end(); viter++) {
				for (siter = (*viter).applies.begin(); siter != (*viter).applies.end(); siter++) {
					mob_apply_db(nSID, (*siter).uid.data(), (*siter).sn, (*siter).snum, mRcvIt->first.data(), (*siter).data);
					delete (*siter).data;
				}
			}
		}
	}
}

void CSender::MissingCheck()
{
	std::map<qcc::String, qcc::String> miss;
	{
		mReceives::iterator miter;
		vReceives::iterator viter;

		for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {
			for (viter = miter->second.begin(); viter != miter->second.end(); viter++) {
				if ((*viter).sn_s != (*viter).sn_e && (*viter).sn_e > 1 && std::find_if(miter->second.begin(), miter->second.end(), find_id((*viter).uid, (*viter).sn_e - 1)) == miter->second.end()) {
					miss[(*viter).uid] += qcc::I32ToString((*viter).sn_e - 1) + "|";
				}
			}
		}
	}
	{
		std::map<qcc::String, qcc::String>::iterator iter;

		for (iter = miss.begin(); iter != miss.end(); iter++) {
			if (!iter->second.empty()) m_pMob->SendData(iter->first.data(), time(NULL), ACT_MISSING, m_pMob->GetSessionID(), iter->second.data(), iter->second.size());
		}
	}
}

/** Receive a signal from another mob client */
void CSender::OnRecvData(const InterfaceDescription::Member* member, const char* srcPath, Message& msg)
{
	QCC_UNUSED(member);
	QCC_UNUSED(srcPath);

	uint8_t * data;
	size_t size;
	std::map<int, TRAIN>::iterator iter;

	msg->GetArg(0)->Get("ay", &size, &data);

	if (IS_TRAIN_HEADER(data) && size == sizeof(TRAIN_HEADER)) {
		TRAIN_HEADER * pTH = (TRAIN_HEADER *)data;

		m_mTrain[pTH->chain].action = pTH->action;
		m_mTrain[pTH->chain].aid = pTH->aid;
		m_mTrain[pTH->chain].wid = pTH->wid;
		memcpy(m_mTrain[pTH->chain].extra, pTH->extra, TRAIN_EXTRA_LEN);
	}
	else if ((iter = m_mTrain.find(((int*)data)[0])) != m_mTrain.end()){
		if (size == sizeof(int) * 2) {
			switch (iter->second.action) {
			case ACT_MISSING:
				// undo DB 에서 uid, snum 을 찾아 타킷 발송.
				//SendData(NULL/*iter->second.uid*/, iter->second.aid, ACT_FLIST_REQ, iter->second.wid, data, len); // special target most be assigned.

				break;
			case ACT_DATA:
				printf("%s:(%d) %ssqlite>", msg->GetSender(), iter->second.length, iter->second.body);
				m_mHangar[iter->second.aid].action = iter->second.action;
				m_mHangar[iter->second.aid].wid = iter->second.wid;
				memcpy(m_mHangar[iter->second.aid].body.z, iter->second.body.z, iter->second.length);
				m_mHangar[iter->second.aid].length = iter->second.length;
				memcpy(m_mHangar[iter->second.aid].extra, iter->second.extra, TRAIN_EXTRA_LEN);
				// 
				break;
			case ACT_FLIST:
				if (sizeof(FILE_SEND_ITEM) % iter->second.length == 0) {
					int n = 0;
					Block data;
					FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.body.z;

					strInit(&data);

					while (n < iter->second.length) {
						//								find pFSI->uri and cpy

						strCat(&data, (char *)pFSI, sizeof(FILE_SEND_ITEM));

						n += sizeof(FILE_SEND_ITEM);
						pFSI++;
					}
					if (data.nUsed > 0) SendData(NULL/*iter->second.uid*/, iter->second.aid, ACT_FLIST_REQ, iter->second.wid, data.z, data.nUsed); // special target most be assigned.

					strFree(&data);
				}
				break;
			case ACT_FLIST_REQ:
				if (sizeof(FILE_SEND_ITEM) % iter->second.length == 0) {
					int n = 0;
					FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.body.z;

					while (n < iter->second.length) {
						// load pFSI->uri as mem
						char * fmem = 0;
						int fsize = 0;
						SendData(NULL/*iter->second.uid*/, iter->second.aid, ACT_FILE, iter->second.wid, fmem, fsize);// special target most be assigned.
						n += sizeof(FILE_SEND_ITEM);
						pFSI++;
					}
					SendData(NULL/*iter->second.uid*/, iter->second.aid, ACT_END, iter->second.wid, 0, 0);// special target most be assigned.
				}
				break;
			case ACT_FILE:
				// save as file of data and register in file list table(or map).
				break;
			case ACT_END:
			{
				std::map<int, TRAIN>::iterator _iter;

				if ((_iter = m_mHangar.find(iter->second.aid)) != m_mHangar.end()){

					// file:// 를 테이블을 이용하여 변환하여 DB 에 반영
					// 충돌이 존재하면 롤백

					Save(_iter->second.wid, _iter->second.body.z, _iter->second.length, _iter->second.extra, TRAIN_EXTRA_LEN);

					m_mHangar.erase(_iter);
				}
			}
				break;
			}
			m_mTrain.erase(iter);
		}
		else {
			strCat(&(iter->second.body), (char *)(((int*)data) + 1), size - sizeof(int));
			iter->second.length += size - sizeof(int);
		}
	}
}

QStatus CSender::_Send(SessionId nSID, const char * sJoinName, int nChain, const char * pData, int nLength)
{
	uint8_t flags = 0;
	QStatus status = ER_FAIL;
	int l = (nLength > 0 ? nLength : 0) + sizeof(int);
	char * pBuf = new char[l];

	if (pBuf) {
		((int*)pBuf)[0] = nChain;

		if (nLength > 0) memcpy(pBuf + sizeof(int), pData, nLength);

		MsgArg mobArg("ay", l, pBuf);

		status = Signal(sJoinName, nSID, *m_pMobSignalMember, &mobArg, 1, 0, flags);

		delete[] pBuf;
	}

	return status;
}

QStatus CSender::SendData(const char * sJoinName, int nAID, int nAction, SessionId wid, const char * msg, int nLength, const char * pExtra, int nExtLen)
{
	TRAIN_HEADER th;
	uint8_t flags = 0;

	TRAIN_HEADER(th.marks);

	th.aid = nAID;
	th.wid = wid;
	th.action = nAction;
	th.chain = time(NULL);
	//	th.chain |= s_user_id; // indivisualize using by user id

	if (pExtra) memcpy(th.extra, pExtra, nExtLen);

	QStatus status;
	MsgArg mobArg("ay", sizeof(TRAIN_HEADER), &th);

	if ((status = Signal(sJoinName, wid, *m_pMobSignalMember, &mobArg, 1, 0, flags)) == ER_OK && nLength > 0) {
		int l = nLength > SEND_BUF ? SEND_BUF : nLength;
		const char * p = msg;

		while ((status = _Send(wid, sJoinName, th.chain, p, l)) == ER_OK) {
			if (nLength > SEND_BUF) {
				nLength -= SEND_BUF;
				p += SEND_BUF;
				l = nLength > SEND_BUF ? SEND_BUF : nLength;
			}
			else break;
		}
		_Send(wid, sJoinName, th.chain, 0, -1);
	}

	return status;
}

int get_file_mtime(const char * path)
{
	HANDLE fh;

	if ((fh = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE) {
		FILETIME modtime;
		SYSTEMTIME stUTC;
		char buf[32];

		GetFileTime(fh, NULL, NULL, &modtime);

		CloseHandle(fh);

		FileTimeToSystemTime(&modtime, &stUTC);

		sprintf(buf, "%02d%02d%02d%02d%02d", stUTC.wMonth, stUTC.wDay, stUTC.wHour, stUTC.wMinute, stUTC.wSecond);

		return atoi(buf);
	}

	return 0;
}

long get_file_length(const char * path)
{
	FILE *fp;
	long sz;

	if ((fp = fopen(path, "rb")) != NULL) {
		fseek(fp, 0, SEEK_END);
		sz = ftell(fp);
		fclose(fp);
		return sz;
	}
	return 0L;
}

int alljoyn_send(SessionId nSID, int nAction, char * sText, int nLength, const char * pExtra, int nExtLen)
{
	time_t aid = time(NULL);
	int ret = gpMob->SendData(NULL, aid, nAction, nSID, sText, nLength, pExtra, nExtLen);

	if (sText && ER_OK == ret) {
		int l;
		char * p = sText;
		Block data;
		char * p2;
		FILE_SEND_ITEM fsi;

		strInit(&data);

		while ((p = strstr(p, "file://")) != NULL) {
			p += 7;
			if ((p2 = strchr(p, '\'')) != NULL && (l = (p2 - p)) > 0) {
				if (l < MAX_URI) {
					memcpy(fsi.uri, p, l);
					fsi.uri[l] = 0;
					fsi.mtime = get_file_mtime(fsi.uri);
					fsi.fsize = get_file_length(fsi.uri);

					strCat(&data, (char *)&fsi, sizeof(FILE_SEND_ITEM));
				}

				p = p2 + 1;
			}
			else p++;
		}
		if (data.nUsed > 0) ret = gpMob->SendData(NULL, aid, ACT_FLIST, nSID, data.z, data.nUsed);

		strFree(&data);
	}

	return ret;
}
