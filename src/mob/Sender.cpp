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

#include "mob.h"
#include "Sender.h"
#include "AlljoynMob.h"

struct find_id : std::unary_function<RECEIVE, bool> {
	int snum;
	std::string uid;
	find_id(std::string & u, int sn) :uid(u), snum(sn) { }
	bool operator()(RECEIVE const& m) const {
		return (m.uid == uid && m.snum_s <= snum && m.snum_e >= snum);
	}
};


static int catmem(char ** data, void * fsi, int len)
{
	return 0;
}

static BOOL asVector(LPCTSTR sText, int nLength, LPCTSTR cDelimit, std::vector<std::string> & stlRes)
{
	stlRes.clear();

	char *nexttoken = NULL;
	char *text = (char *)malloc(nLength + 1);

	if (text) {
		memcpy(text, sText, nLength);

		text[nLength] = 0;

		char *token = strtok_s(text, cDelimit, &nexttoken);

		while (token != NULL) {
			if (strlen(token) > 0) stlRes.push_back(token);
			token = strtok_s(NULL, cDelimit, &nexttoken);
		}

		free(text);
	}

	return !stlRes.empty();
}

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
	vReceives::iterator iter;

	for (iter = m_vReceives.begin(); iter != m_vReceives.end(); iter++) {
		delete (*iter).data;
	}
}

void CSender::Save(int nDocID, const char * sText, int nLength)
{
	const char * e;
	vReceives::iterator iter;
	const char * s = strstr(sText, "/*|");

	if (s) {
		s += 3;
		if ((e = strstr(s, "|*/")) != NULL) {
			std::vector<std::string> v;

			if (asVector(s, e - s, "|", v) && v.size() == 3) {
				RECEIVE rcv = { v[0].data(), v[0].data(), atoi(v[1].data()), atoi(v[1].data()), atoi(v[1].data()), v[2].data(), sText };

				m_vReceives.push_back(rcv);

				for (iter = m_vReceives.begin(); iter != m_vReceives.end(); iter++) {
					if ((*iter).snum_s != (*iter).snum_e && (*iter).snum_e > 1 && std::find_if(m_vReceives.begin(), m_vReceives.end(), find_id((*iter).uid, (*iter).snum_e - 1)) == m_vReceives.end()) {
						return;
					}
				}
			}
		}
	}

	Apply(nDocID);
}

void CSender::Apply(int nDocID)
{
	APPLY ap;
	int snum_undo;
	vApplies applies;
	vReceives::iterator iter;
	const char * uid_undo = NULL;

	for (iter = m_vReceives.begin(); iter != m_vReceives.end();) {
		if ((*iter).data) {
			ap.uid = (*iter).uid;
			ap.snum = (*iter).snum_e;
			ap.data = (*iter).data;
			applies.push_back(ap);
			(*iter).data = NULL;

			vReceives::iterator it = std::find_if(m_vReceives.begin(), m_vReceives.end(), find_id((*iter).uid, (*iter).snum_e - 1));

			if (it != m_vReceives.end()) {
				(*it).snum_e = (*iter).snum_e;
				iter = m_vReceives.erase(iter);
			}
			else iter++;
		}
		else iter++;
	}

	if (uid_undo) mob_undo_db(nDocID, uid_undo, snum_undo);

	vApplies::iterator iter2;

	for (iter2 = applies.begin(); iter2 != applies.end(); iter2++) {
		mob_apply_db(nDocID, (*iter2).uid.data(), (*iter2).snum, (*iter2).data);
		delete (*iter2).data;
	}
}

void CSender::MissingCheck()
{
	std::map<std::string, std::string> miss;
	{
		vReceives::iterator iter;

		for (iter = m_vReceives.begin(); iter != m_vReceives.end(); iter++) {
			if ((*iter).snum_s != (*iter).snum_e && (*iter).snum_e > 1 && std::find_if(m_vReceives.begin(), m_vReceives.end(), find_id((*iter).uid, (*iter).snum_e - 1)) == m_vReceives.end()) {
				miss[(*iter).uid] += std::to_string((*iter).snum_e - 1) + "|";
			}
		}
	}
	{
		std::map<std::string, std::string>::iterator iter;

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
	}
	else if ((iter = m_mTrain.find(((int*)data)[0])) != m_mTrain.end()){
		if (size == sizeof(int)) {
			switch (iter->second.action) {
			case ACT_MISSING:
				// undo DB 에서 uid, snum 을 찾아 타킷 발송.
				break;
			case ACT_DATA:
				printf("%s:(%d) %ssqlite>", msg->GetSender(), iter->second.length, iter->second.body);
				m_mHangar[iter->second.aid].action = iter->second.action;
				m_mHangar[iter->second.aid].wid = iter->second.wid;
				memcpy(m_mHangar[iter->second.aid].body, iter->second.body, iter->second.length);
				m_mHangar[iter->second.aid].length = iter->second.length;
				// 
				break;
			case ACT_FLIST:
				if (sizeof(FILE_SEND_ITEM) % iter->second.length == 0) {
					int n = 0, len = 0;
					char * data = 0;
					FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.body;

					while (n < iter->second.length) {
						//								find pFSI->uri and cpy

						if (catmem(&data, pFSI, sizeof(FILE_SEND_ITEM)) == sizeof(FILE_SEND_ITEM)) len += sizeof(FILE_SEND_ITEM);

						n += sizeof(FILE_SEND_ITEM);
						pFSI++;
					}
					if (len > 0) SendData(NULL/*iter->second.uid*/, iter->second.aid, ACT_FLIST_REQ, iter->second.wid, data, len); // special target most be assigned.
				}
				break;
			case ACT_FLIST_REQ:
				if (sizeof(FILE_SEND_ITEM) % iter->second.length == 0) {
					int n = 0;
					FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.body;

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

					Save(_iter->second.wid, _iter->second.body, _iter->second.length);

					m_mHangar.erase(_iter);
				}
			}
				break;
			}
			m_mTrain.erase(iter);
		}
		else {
			catmem(&(iter->second.body), (void *)(((int*)data) + 1), size - sizeof(int));
			iter->second.length += size - sizeof(int);
		}
	}
}

QStatus CSender::_Send(const char * sJoinName, int nChain, const char * pData, int nLength)
{
	uint8_t flags = 0;
	QStatus status = ER_FAIL;
	int l = (nLength > 0 ? nLength : 0) + sizeof(int);
	char * pBuf = new char[l];

	if (pBuf) {
		((int*)pBuf)[0] = nChain;
		((int*)pBuf)[1] = nLength;

		if (nLength > 0) memcpy(pBuf + sizeof(int), pData, nLength);

		MsgArg mobArg("ay", l, pBuf);

		status = Signal(sJoinName, m_pMob->GetSessionID(), *m_pMobSignalMember, &mobArg, 1, 0, flags);

		delete[] pBuf;
	}

	return status;
}

QStatus CSender::SendData(const char * sJoinName, int nAID, int nAction, int wid, const char * msg, int nLength)
{
	TRAIN_HEADER th;
	uint8_t flags = 0;

	TRAIN_HEADER(th.marks);

	th.aid = nAID;
	th.wid = wid;
	th.action = nAction;
	th.chain = time(NULL);
	//	th.chain |= s_user_id; // indivisualize using by user id

	QStatus status;
	MsgArg mobArg("ay", sizeof(TRAIN_HEADER), &th);

	if ((status = Signal(sJoinName, m_pMob->GetSessionID(), *m_pMobSignalMember, &mobArg, 1, 0, flags)) == ER_OK && nLength > 0) {
		int l = nLength > SEND_BUF ? SEND_BUF : nLength;
		const char * p = msg;

		while ((status = _Send(sJoinName, th.chain, p, l)) == ER_OK) {
			if (nLength > SEND_BUF) {
				nLength -= SEND_BUF;
				p += SEND_BUF;
				l = nLength > SEND_BUF ? SEND_BUF : nLength;
			}
			else break;
		}
		_Send(sJoinName, th.chain, 0, -1);
	}

	return status;
}
