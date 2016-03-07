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

#define TM_MISSING_CHECK	1
#define INT_MISSING_CHECK	2000

extern qcc::String gWPath;

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

struct find_uri : std::unary_function<FILE_RECV_ITEM, bool> {
	int wid;
	qcc::String uid;
	qcc::String uri;

	find_uri(int nDocID, const char * pJoiner, const char * pURI){
		wid = nDocID;
		uid = pJoiner;
		uri = pURI;
	}
	bool operator()(FILE_RECV_ITEM const& m) const {
		return (m.wid == wid && m.uid == uid && m.uri == uri);
	}
};

vRecvFiles gRecvFiles;

extern int get_file_mtime(const char * path);
extern long get_file_length(const char * path);

QStatus CSender::SendFile(const char * sJoinName, int nAID, int nAction, SessionId wid, LPCSTR sPath)
{
	FILE *fp;
	QStatus status = ER_OK;

	if ((fp = fopen(sPath, "rb")) != NULL) {
		int l;
		BYTE Buf[SEND_BUF];
		TRAIN_HEADER th;
		uint8_t flags = 0;
		FILE_SEND_ITEM fsi;

		TRAIN_HEADER(th.marks);

		th.aid = nAID;
		th.wid = wid;
		th.action = nAction;
		th.chain = time(NULL);

		fsi.fsize = get_file_length(sPath);
		fsi.mtime = get_file_mtime(sPath);
		memcpy(fsi.uri, sPath, strlen(sPath));

		memcpy(th.extra, (const char *)&fsi, sizeof(FILE_SEND_ITEM));

		MsgArg mobArg("ay", sizeof(TRAIN_HEADER), &th);

		if ((status = Signal(sJoinName, wid, *m_pMobSignalMember, &mobArg, 1, 0, flags)) == ER_OK) {
			while ((l = fread(Buf, sizeof(BYTE), SEND_BUF, fp)) > 0) {
				if ((status = _Send(wid, sJoinName, th.chain, (const char *)Buf, l)) != ER_OK) break;
			}
			_Send(wid, sJoinName, th.chain, 0, -1);
		}

		fclose(fp);
	}
	return status;
}

void CALLBACK fnMissingCheck(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR idEvent, DWORD /*dwTime*/)
{
	KillTimer(NULL, idEvent);

	gpMob->MissingCheck();
}

void CSender::Save(SessionId nSID, const char * pJoiner, char * sText, int nLength, const char * pExtra, int nExtLen)
{
	RECEIVE rcv;
	std::string p;
	size_t start_pos;
	size_t end_pos;
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

	while ((start_pos = rcv.data.find("file://", start_pos)) != std::string::npos) {
		if ((end_pos = rcv.data.find("\'", start_pos)) != std::string::npos) {
			p.assign(GetLocalPath(nSID, pJoiner, rcv.data.substr(start_pos, end_pos - start_pos).data()));
			rcv.data.replace(start_pos, end_pos - start_pos, p);
			start_pos += p.length();
		}
		else start_pos += 7;
	}

	m_mReceives[pSD->base].push_back(rcv);

	for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {
		for (viter = miter->second.begin(); viter != miter->second.end(); viter++) {
			if ((*viter).sn_s != (*viter).sn_e && (*viter).sn_e > 1 && std::find_if(miter->second.begin(), miter->second.end(), find_id((*viter).uid, (*viter).sn_e - 1)) == miter->second.end()) {
				SetTimer(NULL, TM_MISSING_CHECK, INT_MISSING_CHECK, &fnMissingCheck);
				return;
			}
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
			if (!(*vRcvIt).data.empty() && (n = mob_find_parent_db(nSID, (*vRcvIt).uid_p.data(), (*vRcvIt).snum_p, mRcvIt->first.data())) > num) num = n;
		}

		while ((num = mob_get_db(nSID, num, mRcvIt->first.data(), &sd)) > 0) PushApply(applies, mRcvIt->first.data(), sd.uid_p, sd.snum_p, TRUE);

		bFirst = TRUE;

		do {
			bWorked = FALSE;
			for (vRcvIt = mRcvIt->second.begin(); vRcvIt != mRcvIt->second.end();) {
				if (!(*vRcvIt).data.empty() && PushApply(applies, mRcvIt->first.data(), (*vRcvIt).uid_p.data(), (*vRcvIt).snum_p, bFirst)) {
					bFirst = FALSE;
					(*vRcvIt).data.clear();

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
		qcc::String s;
		mReceives::iterator miter;
		vReceives::iterator viter;

		for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {
			s = "";
			for (viter = miter->second.begin(); viter != miter->second.end(); viter++) {
				if ((*viter).sn_s != (*viter).sn_e && (*viter).sn_e > 1 && std::find_if(miter->second.begin(), miter->second.end(), find_id((*viter).uid, (*viter).sn_e - 1)) == miter->second.end()) {
					if (s != "") s += ",";
					s += qcc::I32ToString((*viter).sn_e - 1);
				}
			}
			if (!s.empty()) miss[(*viter).uid] = s;
		}
	}
	{
		std::map<qcc::String, qcc::String>::iterator iter;

		for (iter = miss.begin(); iter != miss.end(); iter++) {
			if (!iter->second.empty()) m_pMob->SendData(iter->first.data(), time(NULL), ACT_MISSING, m_pMob->GetSessionID(), iter->second.data(), iter->second.size());
		}
	}
}

const char * CSender::GetLocalPath(SessionId nSID, const char * pJoiner, const char * sURI)
{
	vRecvFiles::iterator itFiles;

	if ((itFiles = std::find_if(gRecvFiles.begin(), gRecvFiles.end(), find_uri(nSID, pJoiner, sURI))) != gRecvFiles.end())
		return (*itFiles).path.data();
	else return NULL;
}

const qcc::String mem2tmpfile(const char * data, int length, const qcc::String ext)
{
	FILE *fp;
	qcc::String sPath;
	static ULONGLONG ullCount = 0;

	do {
		sPath = gWPath + qcc::I32ToString(++ullCount) + ext;
	} while (GetFileAttributes(sPath.data()) != INVALID_FILE_ATTRIBUTES);

	if ((fp = fopen(sPath.data(), "wb")) != NULL) {
		fwrite(data, sizeof(char), length, fp);
		fclose(fp);
		return sPath;
	}
	return "";
}

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
				mob_send_missed_db(iter->second.wid, msg->GetSender(), iter->second.body.z);
				break;
			case ACT_DATA:
				printf("%s:(%d) %ssqlite>", msg->GetSender(), iter->second.length, iter->second.body);
				m_mHangar[iter->second.aid].action = iter->second.action;
				m_mHangar[iter->second.aid].wid = iter->second.wid;
				memcpy(m_mHangar[iter->second.aid].body.z, iter->second.body.z, iter->second.length);
				m_mHangar[iter->second.aid].length = iter->second.length;
				memcpy(m_mHangar[iter->second.aid].extra, iter->second.extra, TRAIN_EXTRA_LEN);
				break;
			case ACT_FLIST:
				if (sizeof(FILE_SEND_ITEM) % iter->second.length == 0) {
					int n = 0;
					Block data;
					vRecvFiles::iterator itFiles;
					FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.body.z;

					blkInit(&data);

					while (n < iter->second.length) {
						if ((itFiles = std::find_if(gRecvFiles.begin(), gRecvFiles.end(), find_uri(iter->second.wid, msg->GetSender(), pFSI->uri))) != gRecvFiles.end()) {
							if ((*itFiles).mtime == pFSI->mtime && (*itFiles).mtime == pFSI->mtime) continue;
						}
						memCat(&data, (char *)pFSI, sizeof(FILE_SEND_ITEM));

						n += sizeof(FILE_SEND_ITEM);
						pFSI++;
					}
					if (data.nUsed > 0) SendData(msg->GetSender(), iter->second.aid, ACT_FLIST_REQ, iter->second.wid, data.z, data.nUsed); // special target most be assigned.

					blkFree(&data);
				}
				break;
			case ACT_FLIST_REQ:
				if (sizeof(FILE_SEND_ITEM) % iter->second.length == 0) {
					int n = 0;
					FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.body.z;

					while (n < iter->second.length) {
						SendFile(msg->GetSender(), iter->second.aid, ACT_FILE, iter->second.wid, pFSI->uri);
						n += sizeof(FILE_SEND_ITEM);
						pFSI++;
					}
					SendData(msg->GetSender(), iter->second.aid, ACT_END, iter->second.wid, 0, 0);// special target most be assigned.
				}
				break;
			case ACT_FILE:
			{
				vRecvFiles::iterator itFiles;
				FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.extra;

				if (pFSI) {
					FILE_RECV_ITEM fri;

					if ((itFiles = std::find_if(gRecvFiles.begin(), gRecvFiles.end(), find_uri(iter->second.wid, msg->GetSender(), pFSI->uri))) != gRecvFiles.end())
						gRecvFiles.erase(itFiles);

					fri.fsize = pFSI->fsize;
					fri.mtime = pFSI->mtime;
					fri.uri = pFSI->uri;
					fri.uid = msg->GetSender();
					fri.wid = iter->second.wid;
					fri.path = mem2tmpfile(iter->second.body.z, iter->second.body.nUsed, fri.uri.substr(fri.uri.find_last_of('.')));

					gRecvFiles.push_back(fri);
				}
			}
			break;
			case ACT_END:
			{
				std::map<int, TRAIN>::iterator _iter;

				if ((_iter = m_mHangar.find(iter->second.aid)) != m_mHangar.end()){
					Save(_iter->second.wid, msg->GetSender(), _iter->second.body.z, _iter->second.length, _iter->second.extra, TRAIN_EXTRA_LEN);
					m_mHangar.erase(_iter);
				}
			}
			break;
			}
			m_mTrain.erase(iter);
		}
		else {
			memCat(&(iter->second.body), (char *)(((int*)data) + 1), size - sizeof(int));
			iter->second.length += size - sizeof(int);
		}
	}
}
