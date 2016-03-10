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
#include "dbutil.h"
#include "Global.h"
#include "Sender.h"
#include "AlljoynMob.h"

vRecvFiles gRecvFiles;

struct find_id : std::unary_function<RECEIVE, bool> {
	int snum;
	qcc::String joiner;
	find_id(qcc::String & u, int sn) :joiner(u), snum(sn) { }
	bool operator()(RECEIVE const& m) const {
		return (m.joiner == joiner && m.auto_inc_start <= snum && m.auto_inc_end >= snum);
	}
};

struct find_id_applies : std::unary_function<RECEIVE, bool> {
	int snum_prev;
	qcc::String joiner_prev;
	find_id_applies(const char * u, int sn) :joiner_prev(u), snum_prev(sn) { }
	bool operator()(APPLIES const& m) const {
		return (m.joiner_prev == joiner_prev && m.snum_prev == snum_prev);
	}
};

struct find_uri : std::unary_function<FILE_RECV_ITEM, bool> {
	int session_id;
	qcc::String joiner;
	qcc::String uri;

	find_uri(int nDocID, const char * pJoiner, const char * pURI){
		session_id = nDocID;
		joiner = pJoiner;
		uri = pURI;
	}
	bool operator()(FILE_RECV_ITEM const& m) const {
		return (m.session_id == session_id && m.joiner == joiner && m.uri == uri);
	}
};

QStatus CSender::SendFile(const char * sJoiner, int nFootPrint, int nAction, SessionId sessionId, LPCSTR sPath)
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

		th.footprint = nFootPrint;
		th.action = nAction;
		th.chain = time(NULL);

		fsi.fsize = get_file_length(sPath);
		fsi.mtime = get_file_mtime(sPath);
		memcpy(fsi.uri, sPath, strlen(sPath));

		memcpy(th.extra, (const char *)&fsi, sizeof(FILE_SEND_ITEM));

		MsgArg mobArg("ay", sizeof(TRAIN_HEADER), &th);

		if ((status = Signal(sJoiner, sessionId, *m_pMobSignalMember, &mobArg, 1, 0, flags)) == ER_OK) {
			while ((l = fread(Buf, sizeof(BYTE), SEND_BUF, fp)) > 0) {
				if ((status = _Send(sessionId, sJoiner, th.chain, (const char *)Buf, l)) != ER_OK) break;
			}
			_Send(sessionId, sJoiner, th.chain, 0, -1);
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

void CSender::Save(SessionId sessionId, const char * pJoiner, Block * pText, const char * pExtra, int nExtLen)
{
	RECEIVE rcv;
	std::string p;
	size_t start_pos;
	size_t end_pos;
	mReceives::iterator miter;
	vReceives::iterator viter;
	SYNC_DATA * pSD = (SYNC_DATA *)pExtra;

	rcv.joiner_prev = pSD->joiner_prev;
	rcv.joiner = pSD->joiner;
	rcv.snum_prev = pSD->snum_prev;
	rcv.snum = pSD->snum;
	rcv.auto_inc_start = pSD->auto_inc;
	rcv.auto_inc_end = pSD->auto_inc;
	rcv.data.assign(pText->z, pText->nUsed);

	while ((start_pos = rcv.data.find("file://", start_pos)) != std::string::npos) {
		if ((end_pos = rcv.data.find("\'", start_pos)) != std::string::npos) {
			p.assign(GetLocalPath(sessionId, pJoiner, rcv.data.substr(start_pos, end_pos - start_pos).data()));
			rcv.data.replace(start_pos, end_pos - start_pos, p);
			start_pos += p.length();
		}
		else start_pos += 7;
	}

	m_mReceives[pSD->base_table].push_back(rcv);

	for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {
		for (viter = miter->second.begin(); viter != miter->second.end(); viter++) {
			if ((*viter).auto_inc_start != (*viter).auto_inc_end && (*viter).auto_inc_end > 1 && std::find_if(miter->second.begin(), miter->second.end(), find_id((*viter).joiner, (*viter).auto_inc_end - 1)) == miter->second.end()) {
				SetTimer(NULL, TM_MISSING_CHECK, INT_MISSING_CHECK, &fnMissingCheck);
				return;
			}
		}
	}

	Apply(sessionId);
}

BOOL CSender::PushApply(vApplies & applies, const char * sTable, const char * sJoinerPrev, int nSNumPrev, BOOL bFirst)
{
	APPLY apply;
	vApplies::iterator viter;

	if ((viter = std::find_if(applies.begin(), applies.end(), find_id_applies(sJoinerPrev, nSNumPrev))) != applies.end()) (*viter).applies.insert(apply);
	else if (bFirst) {
		APPLIES appl;

		appl.applies.insert(apply);
		applies.push_back(appl);
		bFirst = FALSE;
	}
	return TRUE;
}

int mob_find_parent_db(unsigned int sid, const char * joiner, int snum, const char * sTable)
{
	return 0;
}

int mob_get_db(unsigned int sid, int num, const char * sTable, SYNC_DATA * pSD)
{
	return 0;
}

void CSender::Apply(SessionId sessionId)
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
			if (!(*vRcvIt).data.empty() && (n = mob_find_parent_db(sessionId, (*vRcvIt).joiner_prev.data(), (*vRcvIt).snum_prev, mRcvIt->first.data())) > num) num = n;
		}

		while ((num = mob_get_db(sessionId, num, mRcvIt->first.data(), &sd)) > 0) PushApply(applies, mRcvIt->first.data(), sd.joiner_prev, sd.snum_prev, TRUE);

		bFirst = TRUE;

		do {
			bWorked = FALSE;
			for (vRcvIt = mRcvIt->second.begin(); vRcvIt != mRcvIt->second.end();) {
				if (!(*vRcvIt).data.empty() && PushApply(applies, mRcvIt->first.data(), (*vRcvIt).joiner_prev.data(), (*vRcvIt).snum_prev, bFirst)) {
					bFirst = FALSE;
					(*vRcvIt).data.clear();

					vReceives::iterator it = std::find_if(mRcvIt->second.begin(), mRcvIt->second.end(), find_id((*vRcvIt).joiner, (*vRcvIt).auto_inc_end - 1));

					if (it != mRcvIt->second.end()) {
						(*it).auto_inc_end = (*vRcvIt).auto_inc_end;
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
			Block undo;
			sqlite3_stmt *pStmt2 = NULL;
			sqlite3_stmt *pStmt3 = NULL;

			QUERY_SQL_V(m_pMob->GetUndoDB(), pStmt2, ("SELECT num FROM works WHERE joiner=%Q AND snum=%d AND base_table=%Q", (*viter).joiner_prev.data(), (*viter).snum_prev, mRcvIt->first.data()),
				int num = sqlite3_column_int(pStmt2, 0);

				QUERY_SQL_V(m_pMob->GetUndoDB(), pStmt3, ("SELECT undo FROM works WHERE num > %d AND base_table=%Q ORDER BY num DESC", num, mRcvIt->first.data()),
					sqlite3_exec(m_pMob->GetMainDB(), (const char *)sqlite3_column_text(pStmt3, 0), 0, 0, 0);
				);
				EXECUTE_SQL_V(m_pMob->GetMainDB(), ("DELETE FROM works WHERE num > %d AND base_table=%Q;REINDEX works;", num, mRcvIt->first.data()));
				break;
			);

			for (; viter != applies.end(); viter++) {
				for (siter = (*viter).applies.begin(); siter != (*viter).applies.end(); siter++) {

					blkInit(&undo);

					sqlite3_exec(m_pMob->GetMainDB(), (*siter).data, 0, 0, 0);
					diff_one_table(m_pMob->GetMainDB(), "main", "aux", mRcvIt->first.data(), &undo);
					sqlite3_exec(m_pMob->GetBackDB(), (*siter).data, 0, 0, 0);
					EXECUTE_SQL_V(m_pMob->GetUndoDB(), ("INSERT INTO works (joiner, auto_inc, snum, base_table, undo, redo) VALUES (%Q, %d, %d, %Q, %Q, %Q);", (*siter).joiner.data(), (*siter).auto_inc, (*siter).snum, mRcvIt->first.data(), undo.z, (*siter).data));

					blkFree(&undo);

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
				if ((*viter).auto_inc_start != (*viter).auto_inc_end && (*viter).auto_inc_end > 1 && std::find_if(miter->second.begin(), miter->second.end(), find_id((*viter).joiner, (*viter).auto_inc_end - 1)) == miter->second.end()) {
					if (s != "") s += ",";
					s += qcc::I32ToString((*viter).auto_inc_end - 1);
				}
			}
			if (!s.empty()) miss[(*viter).joiner] = s;
		}
	}
	{
		std::map<qcc::String, qcc::String>::iterator iter;

		for (iter = miss.begin(); iter != miss.end(); iter++) {
			if (!iter->second.empty()) m_pMob->SendData(iter->first.data(), time(NULL), ACT_MISSING, m_pMob->GetSessionID(), iter->second.data(), iter->second.size());
		}
	}
}

const char * CSender::GetLocalPath(SessionId sessionId, const char * pJoiner, const char * sURI)
{
	vRecvFiles::iterator itFiles;

	if ((itFiles = std::find_if(gRecvFiles.begin(), gRecvFiles.end(), find_uri(sessionId, pJoiner, sURI))) != gRecvFiles.end())
		return (*itFiles).path.data();
	else return NULL;
}

void CSender::MissingCheck(const char * sJoiner, int nSNum)
{
	mReceives::iterator miter;
	vReceives::iterator viter;
	qcc::String s = qcc::I32ToString(nSNum);

	for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {
		for (viter = miter->second.begin(); viter != miter->second.end(); viter++) {
			if (!((*viter).auto_inc_start <= nSNum && (*viter).auto_inc_end >= nSNum && (*viter).joiner == sJoiner)) {
				m_pMob->SendData(sJoiner, time(NULL), ACT_MISSING, m_pMob->GetSessionID(), s.data(), s.size());
				return;
			}
		}
	}
	m_pMob->SendData(sJoiner, time(NULL), ACT_NO_MISSED, m_pMob->GetSessionID(), s.data(), s.size());
}

void CSender::OnRecvData(const InterfaceDescription::Member* pMember, const char* srcPath, Message& msg)
{
	QCC_UNUSED(pMember);
	QCC_UNUSED(srcPath);

	const char * pJoiner = msg->GetSender();
	uint8_t * data;
	size_t size;
	std::map<int, TRAIN>::iterator iter;
	mTrain & train = m_mTrain[pJoiner];
	SessionId sessionId = m_pMob->GetSessionID();

	msg->GetArg(0)->Get("ay", &size, &data);

	if (IS_TRAIN_HEADER(data) && size == sizeof(TRAIN_HEADER)) {
		TRAIN_HEADER * pTH = (TRAIN_HEADER *)data;

		train[pTH->chain].action = pTH->action;
		train[pTH->chain].footprint = pTH->footprint;
		blkInit(&(train[pTH->chain].body));
		memcpy(train[pTH->chain].extra, pTH->extra, TRAIN_EXTRA_LEN);
	}
	else if ((iter = train.find(((int*)data)[0])) != train.end()){
		if (size == sizeof(int)) {
			switch (iter->second.action) {
			case ACT_MISSING:
			{
				sqlite3_stmt *pStmt = NULL;
				sqlite3_stmt *pStmt2 = NULL;
				SYNC_DATA sd;

				strcpy_s(sd.joiner, sizeof(sd.joiner), m_pMob->GetJoinName());

				QUERY_SQL_V(m_pMob->GetUndoDB(), pStmt2, ("SELECT num, auto_inc, snum, base_table, undo FROM works WHERE joiner=%Q AND auto_inc IN (%s)", sd.joiner, iter->second.body.z),
					sd.auto_inc = sqlite3_column_int(pStmt2, 1);
					sd.snum = sqlite3_column_int(pStmt2, 2);
					strcpy_s(sd.base_table, sizeof(sd.base_table), (const char *)sqlite3_column_text(pStmt2, 3));
					QUERY_SQL_V(m_pMob->GetUndoDB(), pStmt, ("SELECT joiner, snum FROM works WHERE num > %d AND base_table=%Q ORDER BY num DESC LIMIT 1", sqlite3_column_int(pStmt2, 0), sd.base_table),
						strcpy_s(sd.joiner_prev, sizeof(sd.joiner_prev), (const char *)sqlite3_column_text(pStmt, 0));
						sd.snum_prev = sqlite3_column_int(pStmt, 1);
						alljoyn_send(sessionId, msg->GetSender(), ACT_DATA, (char *)sqlite3_column_text(pStmt2, 4), sqlite3_column_bytes(pStmt2, 4), (const char *)&sd, sizeof(SYNC_DATA));
						break;
					);
				);
				break;
			}
			case ACT_DATA:
			{
				printf("%s: %ssqlite>", msg->GetSender(), iter->second.body);
				TRAIN & cargo = m_mStation[pJoiner][iter->second.footprint];
				cargo.action = iter->second.action;
				blkMove(&(cargo.body), &(iter->second.body));
				memcpy(cargo.extra, iter->second.extra, TRAIN_EXTRA_LEN);
				break;
			}
			case ACT_FLIST:
				if (sizeof(FILE_SEND_ITEM) % iter->second.body.nUsed == 0) {
					int n = 0;
					Block data;
					vRecvFiles::iterator itFiles;
					FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.body.z;

					blkInit(&data);

					while (n < iter->second.body.nUsed) {
						if ((itFiles = std::find_if(gRecvFiles.begin(), gRecvFiles.end(), find_uri(sessionId, msg->GetSender(), pFSI->uri))) != gRecvFiles.end()) {
							if ((*itFiles).mtime == pFSI->mtime && (*itFiles).mtime == pFSI->mtime) continue;
						}
						mem2mem(&data, (char *)pFSI, sizeof(FILE_SEND_ITEM));

						n += sizeof(FILE_SEND_ITEM);
						pFSI++;
					}
					if (data.nUsed > 0) SendData(msg->GetSender(), iter->second.footprint, ACT_FLIST_REQ, sessionId, data.z, data.nUsed); // special target most be assigned.

					blkFree(&data);
				}
				break;
			case ACT_FLIST_REQ:
				if (sizeof(FILE_SEND_ITEM) % iter->second.body.nUsed == 0) {
					int n = 0;
					FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.body.z;

					while (n < iter->second.body.nUsed) {
						SendFile(msg->GetSender(), iter->second.footprint, ACT_FILE, sessionId, pFSI->uri);
						n += sizeof(FILE_SEND_ITEM);
						pFSI++;
					}
					SendData(msg->GetSender(), iter->second.footprint, ACT_END, sessionId, 0, 0);// special target most be assigned.
				}
				break;
			case ACT_FILE:
			{
				vRecvFiles::iterator itFiles;
				FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.extra;

				if (pFSI) {
					FILE_RECV_ITEM fri;

					if ((itFiles = std::find_if(gRecvFiles.begin(), gRecvFiles.end(), find_uri(sessionId, msg->GetSender(), pFSI->uri))) != gRecvFiles.end())
						gRecvFiles.erase(itFiles);

					fri.fsize = pFSI->fsize;
					fri.mtime = pFSI->mtime;
					fri.uri = pFSI->uri;
					fri.joiner = msg->GetSender();
					fri.session_id = sessionId;
					fri.path = mem2file(iter->second.body.z, iter->second.body.nUsed, fri.uri.substr(fri.uri.find_last_of('.')).data());

					gRecvFiles.push_back(fri);
				}
				break;
			}
			case ACT_END:
			{
				std::map<int, TRAIN>::iterator _iter;

				if ((_iter = m_mStation[pJoiner].find(iter->second.footprint)) != m_mStation[pJoiner].end()){
					Save(sessionId, msg->GetSender(), &(_iter->second.body), _iter->second.extra, TRAIN_EXTRA_LEN);
					blkFree(&(_iter->second.body));
					m_mStation[pJoiner].erase(_iter);
				}
				break;
			}
			case ACT_SIGNAL:
			{
				SYNC_SIGNAL * pSS = (SYNC_SIGNAL *)iter->second.extra;

				if (pSS) MissingCheck(pSS->joiner, pSS->auto_inc);
				break;
			}
			case ACT_NO_MISSED:
				EXECUTE_SQL_V(m_pMob->GetMainDB(), ("UPDATE works SET auto_inc=%s WHERE num=%d;", iter->second.body.z, sessionId));
				break;
			}
			train.erase(iter);
		}
		else mem2mem(&(iter->second.body), (char *)(((int*)data) + 1), size - sizeof(int));
	}
}
