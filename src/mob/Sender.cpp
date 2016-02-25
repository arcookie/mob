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

#include "mob.h"
#include "Sender.h"
#include "AlljoynMob.h"


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
	std::vector<WORKS *>::iterator iter;

	for (iter = m_vWorks.begin(); iter != m_vWorks.end(); iter++) {
		delete (*iter);
	}
}

BOOL CSender::IsOmitted(int nDocID)
{
	return FALSE;
}

void CSender::FixOmitted(int nDocID)
{
	//
}

const std::string & CSender::Save(int nDocID, const char * sText, int nLength, std::string & out)
{
	const char * s = strstr(sText, "/*|");
	const char * e;

	if (s) {
		s += 3;
		if ((e = strstr(s, "|*/")) != NULL) {
			std::vector<std::string> v;

			if (asVector(s, e - s, "|", v) && v.size() == 3) {
				m_vWorks.push_back(new WORKS(v[0].data(), atoi(v[1].data()), v[2].data(), sText));
			}
			// 누락이 있으면 누락 전송용 타이머 시작
			// 
			if (IsOmitted(nDocID)) FixOmitted(nDocID);
		}
	}

	// 충돌이 존재하면 롤백

	// 보냄과 동시에 정리.

	return out;
}
/*

const Value & CHncWebCtrl::GetSNumList(CHncSQLite & dbWork, LPCTSTR sTable, int nUID, Value & jData, BOOL & bMissed)
{
	int n = 1;
	Value jWork;
	CString s, sHaving = _T("");

	bMissed = FALSE;

	if (dbWork.VQuerySQL(jWork, _T("SELECT snum FROM %s WHERE uid = %d AND snum >= 0 ORDER BY snum"), sTable, nUID)) {
		map<int, int>::iterator iter;

		if ((iter = m_mMissing.find(nUID)) != m_mMissing.end()) {
			if (iter->second < jWork[0]["snum"].asInt()) {
				int k = iter->second;
				int snum0 = jWork[0]["snum"].asInt();

				while (k < snum0) {
					s.Format(_T("%s%d"), sHaving.IsEmpty() ? _T("") : _T(","), k++);
					sHaving += s;
				}
				bMissed = TRUE;
			}
			m_mMissing.erase(iter);
		}

		int i = -1, cnt = jWork.size(), e;

		while (++i < cnt) {
			n = (jWork[i]["snum"].asInt() + 1);
			if (i < cnt - 1 && (e = jWork[i + 1]["snum"].asInt()) != n) {
				bMissed = TRUE;
				while (n < e) {
					s.Format(_T("%s%d"), sHaving.IsEmpty() ? _T("") : _T(","), n++);
					sHaving += s;
				}
			}
		}
	}

	s.Format(_T("%s%d"), sHaving.IsEmpty() ? _T("") : _T(","), n);
	sHaving += s;

	jData["__having__"] = (LPCTSTR)sHaving;
	jData["__last__"] = n;

	return jData;
}

BOOL CHncWebCtrl::CollectMissing(CHncSQLite & dbWork, LPCTSTR sTable, int nUID, Value & jData, string & sUIDList)
{
	CString s;
	BOOL bMissed = FALSE, bFlag;
	Value jWork;

	sUIDList = _T("");

	if (dbWork.VQuerySQL(jWork, _T("SELECT num FROM member WHERE num <> %d ORDER BY num"), nUID)) {
		int i = -1, cnt = jWork.size(), uid;

		while (++i < cnt) {
			uid = jWork[i]["num"].asInt();
			jData["__data__"]["__snum_list__"][jWork[i]["num"].asCString()] = GetSNumList(dbWork, sTable, uid, Value(), bFlag);
			if (bFlag) {
				s.Format(_T("%d|"), uid);
				sUIDList += s;
				bMissed = TRUE;
			}
		}
	}
	return bMissed;
}

BOOL CHncWebCtrl::MissingCheck(LPCTSTR sTable)
{
	string sUIDList;
	Value jData, jSetting;
	int nDocID = get_docid(GetSafeHwnd());
	CHncSQLite & dbWork = get_dbwork(nDocID);
	int nUID = GetUserID(nDocID);

	if (CollectMissing(dbWork, sTable, nUID, jData, sUIDList)) {
		jData["__work__"] = _T("send_chat");
		jData["__type__"] = 1;
		jData["__local_wid__"] = nDocID;
		jData["__data__"]["__table__"] = sTable;
		jData["__data__"]["__uid_list__"] = sUIDList;

		m_wObj.SaveWork(jData);

		m_wObj.SendMain();

		return TRUE;
	}
	return FALSE;
}


LPSTR CHncWebCtrl::Apply(int nDocID) 
{
#define MISSING_RETURN	if (snum > 1 && !dbWork.VQuerySQL(Value(), _T("SELECT num FROM working WHERE uid=%s AND snum=%d"), jData[0L]["uid"].asCString(), snum - 1)) { LOG("누락 발생: uid - %s , snum - %d 없음", jData[0L]["uid"].asCString(), snum - 1); return (LPSTR)m_sApplyData.data(); }

	m_sApplyData = _T("");

	KillTimer(TM_APPLY_WORK_REPEAT);

	if (nDocID > 0) {
		Value jData;
		CString  sSQL;
		int nUID = GetUserID(nDocID);
		CHncSQLite & dbWork = get_dbwork(nDocID);

		if (gAppID == AID_HSHOW) sSQL.Format(_T("SELECT num, uid, snum, uri FROM working WHERE receivers LIKE '%%%d|%%' ORDER BY uid DESC, snum ASC LIMIT 1"), nUID);
		else sSQL.Format(_T("SELECT num, uid, snum, uri FROM working WHERE receivers LIKE '%%%d|%%' ORDER BY uid, snum LIMIT 1"), nUID);

		if (dbWork.QuerySQL(jData, sSQL)) {
			int num = jData[0L]["num"].asInt();
			int snum = jData[0L]["snum"].asInt();
			string s0, uids = jData[0L]["uid"].asString() + "|";
			int uid = get_int(jData[0L]["uri"].asString(), "uid=", " AND", -1);

#ifdef HCC_REPLAY
			if (!IsInReplaying()) {
#endif
				MISSING_RETURN;

				while (uid > 0 && dbWork.VQuerySQL(jData, _T("SELECT num, uid, snum, uri FROM working WHERE uid=%d AND receivers LIKE '%%%d|%%' ORDER BY snum LIMIT 1"), uid, nUID)) {
					snum = jData[0L]["snum"].asInt();

					MISSING_RETURN;

					s0 = jData[0L]["uid"].asString() + "|";
					if (uids.find(s0) == string::npos) {
						uids += s0;
						uid = get_int(jData[0L]["uri"].asString(), "uid=", " AND", -1);
						num = jData[0L]["num"].asInt();
					}
					else break;
				}

				sSQL.Format(_T("SELECT num, snum, uid, type, uri, vid, receivers, title FROM working WHERE num = %d"), num);
#ifdef HCC_REPLAY
			}
			else {
				if (!GetRecordKey(jData)) return (LPSTR)m_sApplyData.data();
				else {
					uid = jData[0L]["uid"].asInt();
					snum = jData[0L]["snum"].asInt();
					sSQL.Format(_T("SELECT num, snum, uid, type, uri, vid, receivers, title FROM working WHERE uid = %d AND snum = %d"), uid, snum);
				}
			}
#endif

			if (dbWork.QuerySQL(jData, sSQL)) {
				FastWriter writer;
				CString s, sFiles;
				const Value & jRow = jData[0L];
				CHncCommData & commData = get_comm_data(nDocID);

				uid = jRow["uid"].asInt();
				num = jRow["num"].asInt();
				snum = jRow["snum"].asInt();

				LOG("오피스에 반영: uid - %d , snum - %d", uid, snum);

#ifdef HCC_REPLAY
				if (IsInRecording()) RecordSNum(uid, snum);
#endif

				Json::Value jFiles;

#ifdef HCC_REPLAY
				if (IsInReplaying()) GetRecordedFiles(uid, snum, jFiles);
				else 
#endif
				jFiles = commData.GetFiles(num);

				if (jFiles.isArray() && jFiles.size() > 0) {
					if (!jRow["vid"].asString().empty()) download_files(jFiles);
					sFiles = writer.write(jFiles).data();
#ifdef HCC_REPLAY
					if (IsInRecording()) RecordFiles(uid, snum, jFiles);
#endif
				}
				else sFiles = _T("[]");

				s.Format(_T("{\"__type__\":%d,\"__uid_from__\":%d,\"__num__\":%d,\"__snum__\":%d,\"__uri__\":\"uid=%d AND snum=%d\",\"__title__\":\"%s\",\"__data__\":"),
					jRow["type"].asInt(), uid, num, snum, uid, snum, jRow["title"].asCString());

				m_sApplyData = s;
				m_sApplyData += writer.write(commData.GetData(num));
				m_sApplyData += _T(",\"__files__\":");
				m_sApplyData += sFiles;
				m_sApplyData += _T("} ");
			}
		}
		return (LPSTR)m_sApplyData.data();

#undef MISSING_RETURN
	}
}

void CHncWebCtrl::CheckMissing() 
{
	int nDocID = get_docid(GetSafeHwnd());

	if (nDocID > 0) {
		Value jWork;
		int nUID = GetUserID(nDocID);
		CHncSQLite & dbWork = get_dbwork(nDocID);

		KillTimer(TM_SEND_WORK_SLIST);

		if (dbWork.VQuerySQL(jWork, _T("SELECT uid, snum FROM working WHERE receivers LIKE '%%%d|%%'"), nUID)) {
			int uid, snum, i = -1, cnt = jWork.size();

			while (++i < cnt) {
				const Value & jRow = jWork[i];

				uid = jRow["uid"].asInt();
				snum = jRow["snum"].asInt() - 1;

				if (snum > 0 && !dbWork.VQuerySQL(Value(), _T("SELECT num FROM working WHERE uid=%d AND snum=%d"), uid, snum)) {
					map<int, int>::iterator iter2;

					if ((iter2 = m_mMissing.find(uid)) != m_mMissing.end()) {
						if (iter2->second > snum) m_mMissing[uid] = snum;
					}
					else m_mMissing[uid] = snum;

					SetTimer(TM_SEND_WORK_SLIST, 3000, 0);
					return;
				}
			}
		}
	}
}

*/

struct find_id : std::unary_function<monster, bool> {
	int snum;
	std::string uid;
	find_id(const char * u, int sn) :uid(u), snum(sn) { }
	bool operator()(monster const& m) const {
		return (m.uid == uid && m.snum == snum);
	}
};

void CSender::Find(const char * sUID, int nSNum)
{
	it = std::find_if(m_vReceives.begin(), m_vReceives.end(), find_id(sUID, nSNum));
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
			case ACT_OMITTED:
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
					if (len > 0) SendData(iter->second.aid, ACT_FLIST_REQ, iter->second.wid, data, len); // special target most be assigned.
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
						SendData(iter->second.aid, ACT_FILE, iter->second.wid, fmem, fsize);// special target most be assigned.
						n += sizeof(FILE_SEND_ITEM);
						pFSI++;
					}
					SendData(iter->second.aid, ACT_END, iter->second.wid, 0, 0);// special target most be assigned.
				}
				break;
			case ACT_FILE:
				// save as file of data and register in file list table(or map).
				break;
			case ACT_END:
			{
				std::map<int, TRAIN>::iterator _iter;

				if ((_iter = m_mHangar.find(iter->second.aid)) != m_mHangar.end()){
					std::string s;

					// file:// 를 테이블을 이용하여 변환하여 DB 에 반영
					// apply 하고 머지후 전달할것.

					Save(_iter->second.wid, _iter->second.body, _iter->second.length, s);
// if missing is exist do this timer work.
//			case TM_SEND_WORK_SLIST:
	//			MissingCheck(_T("working"));
		//		break;
			//case TM_CHECK_MISSING:
			//	CheckMissing();
			//	break;
					//KillTimer(hWnd, TM_CHECK_MISSING);
					//SetTimer(hWnd, TM_CHECK_MISSING, 100, NULL);

					std::vector<WORKS *>::iterator __iter;

					for (__iter = m_vWorks.begin(); __iter != m_vWorks.end(); __iter++) {
						mob_apply(_iter->second.wid, (*__iter)->uid.data(), (*__iter)->snum, (*__iter)->data);
					}
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

QStatus CSender::_Send(int nChain, const char * pData, int nLength)
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

		status = Signal(NULL, m_pMob->GetSessionID(), *m_pMobSignalMember, &mobArg, 1, 0, flags);

		delete[] pBuf;
	}

	return status;
}

QStatus CSender::SendData(int nAID, int nAction, int wid, const char * msg, int nLength)
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

	if ((status = Signal(NULL, m_pMob->GetSessionID(), *m_pMobSignalMember, &mobArg, 1, 0, flags)) == ER_OK && nLength > 0) {
		int l = nLength > SEND_BUF ? SEND_BUF : nLength;
		const char * p = msg;

		while ((status = _Send(th.chain, p, l)) == ER_OK) {
			if (nLength > SEND_BUF) {
				nLength -= SEND_BUF;
				p += SEND_BUF;
				l = nLength > SEND_BUF ? SEND_BUF : nLength;
			}
			else break;
		}
		_Send(th.chain, 0, -1);
	}

	return status;
}
