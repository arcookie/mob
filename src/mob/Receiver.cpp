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

struct find_uri : std::unary_function<FILE_RECV_ITEM*, bool> {
	int session_id;
	qcc::String joiner;
	qcc::String uri;

	find_uri(int nDocID, const char * pJoiner, const char * pURI){
		session_id = nDocID;
		joiner = pJoiner;
		uri = pURI;
	}
	bool operator()(FILE_RECV_ITEM const * m) const {
		return (m->session_id == session_id && m->joiner == joiner && m->uri == uri);
	}
};

CSender::~CSender()
{
	vRecvFiles::iterator iter;
	mReceives::iterator miter;
	vReceives::iterator viter;

	for (iter = gRecvFiles.begin(); iter != gRecvFiles.end(); iter++){
		delete (*iter);
	}
	for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {
		for (viter = miter->second.begin(); viter != miter->second.end(); viter++) {
			delete (*viter);
		}
	}
}

QStatus CSender::SendFile(const char * sJoiner, int nFootPrint, int nAction, SessionId sessionId, FILE_SEND_ITEM * pFSI)
{
	FILE *fp;
	QStatus status = ER_OK;

	if ((fp = fopen(get_path(pFSI->uri).data(), "rb")) != NULL) {
		int l;
		BYTE Buf[SEND_BUF];
		TRAIN_HEADER th;
		uint8_t flags = 0;

		TRAIN_HEADER(th.marks);

		th.footprint = nFootPrint;
		th.action = nAction;
		th.chain = time(NULL);

		memcpy(th.extra, (const char *)pFSI, sizeof(FILE_SEND_ITEM));

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

void CSender::Save(SessionId sessionId, const char * pJoiner, Block * pText, const char * pExtra, int nExtLen)
{
	RECEIVE * pRCV = new RECEIVE;
	qcc::String p;
	size_t start_pos = 0;
	size_t end_pos;
	vRecvFiles::iterator itFiles;
	SYNC_DATA * pSD = (SYNC_DATA *)pExtra;

	pRCV->joiner_prev = pSD->joiner_prev;
	pRCV->joiner = pSD->joiner;
	pRCV->snum_prev = pSD->snum_prev;
	pRCV->snum = pSD->snum;
	pRCV->auto_inc_start = pSD->auto_inc;
	pRCV->auto_inc_end = pSD->auto_inc;
	pRCV->data.assign(pText->z, pText->nUsed);

	while ((start_pos = pRCV->data.find("file://", start_pos)) != std::string::npos) {
		if ((end_pos = pRCV->data.find("\'", start_pos)) != std::string::npos) {
			if ((itFiles = std::find_if(gRecvFiles.begin(), gRecvFiles.end(), find_uri(sessionId, pJoiner, pRCV->data.substr(start_pos, end_pos - start_pos).data()))) != gRecvFiles.end())
				pRCV->data.replace(start_pos, end_pos - start_pos, get_uri((*itFiles)->path.data()).data());
			start_pos += p.length();
		}
		else start_pos ++;
	}

	m_mReceives[pSD->base_table].push_back(pRCV);

	if (!StartMissingCheck()) Apply(sessionId);
}

void CSender::OnEnd(int footprint, const char * pJoiner)
{
	mTrain::iterator iter;
	SessionId sessionId = m_pMob->GetSessionID();

	if ((iter = m_mStation[pJoiner].find(footprint)) != m_mStation[pJoiner].end()){
		Save(sessionId, pJoiner, &(iter->second.body), iter->second.extra, TRAIN_EXTRA_LEN);
		blkFree(&(iter->second.body));
		m_mStation[pJoiner].erase(iter);
	}
}

void CSender::OnRecvData(const InterfaceDescription::Member* /*pMember*/, const char* /*srcPath*/, Message& msg)
{
	uint8_t * data;
	size_t size;
	mTrain::iterator iter;
	const char * pJoiner = msg->GetSender();
	mTrain & train = m_mTrain[pJoiner];
	SessionId sessionId = m_pMob->GetSessionID();

	msg->GetArg(0)->Get("ay", &size, &data);

	if (IS_TRAIN_HEADER(data) && size == sizeof(TRAIN_HEADER)) {
		TRAIN_HEADER * pTH = (TRAIN_HEADER *)data;

		if (pTH->action == ACT_END) OnEnd(pTH->footprint, pJoiner);
		else if (pTH->action == ACT_SIGNAL) {
			SYNC_SIGNAL * pSS = (SYNC_SIGNAL *)pTH->extra;

			if (pSS) MissingCheck(pSS->joiner, pSS->auto_inc);
		}
		else {
			train[pTH->chain].action = pTH->action;
			train[pTH->chain].footprint = pTH->footprint;
			blkInit(&(train[pTH->chain].body));
			memcpy(train[pTH->chain].extra, pTH->extra, TRAIN_EXTRA_LEN);

			if (pTH->action == ACT_FILE) {
				FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)pTH->extra;

				if (pFSI) {
					qcc::String uri = pFSI->uri;

					train[pTH->chain].path = mem2file(0, 0, uri.substr(uri.find_last_of('.')).data());
				}
			}
		}
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
						if ((itFiles = std::find_if(gRecvFiles.begin(), gRecvFiles.end(), find_uri(sessionId, msg->GetSender(), pFSI->uri))) == gRecvFiles.end()|| 
							(*itFiles)->mtime != pFSI->mtime || (*itFiles)->mtime != pFSI->mtime) 
							mem2mem(&data, (char *)pFSI, sizeof(FILE_SEND_ITEM));

						n += sizeof(FILE_SEND_ITEM);
						pFSI++;
					}
					if (data.nUsed > 0) SendData(pJoiner, iter->second.footprint, ACT_FLIST_REQ, sessionId, data.z, data.nUsed); // special target most be assigned.
					else OnEnd(iter->second.footprint, pJoiner);

					blkFree(&data);
				}
				break;
			case ACT_FLIST_REQ:
				if (sizeof(FILE_SEND_ITEM) % iter->second.body.nUsed == 0) {
					int n = 0;
					FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.body.z;

					while (n < iter->second.body.nUsed) {
						SendFile(pJoiner, iter->second.footprint, ACT_FILE, sessionId, pFSI);
						n += sizeof(FILE_SEND_ITEM);
						pFSI++;
					}
					SendData(pJoiner, iter->second.footprint, ACT_END, sessionId, 0, 0);// special target most be assigned.
				}
				break;
			case ACT_FILE:
			{
				vRecvFiles::iterator itFiles;
				FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.extra;

				if (pFSI) {
					FILE_RECV_ITEM * pFRI = new FILE_RECV_ITEM;

					if ((itFiles = std::find_if(gRecvFiles.begin(), gRecvFiles.end(), find_uri(sessionId, pJoiner, pFSI->uri))) != gRecvFiles.end()) {
						delete (*itFiles);
						gRecvFiles.erase(itFiles);
					}

					pFRI->fsize = pFSI->fsize;
					pFRI->mtime = pFSI->mtime;
					pFRI->uri = pFSI->uri;
					pFRI->joiner = pJoiner;
					pFRI->session_id = sessionId;
					pFRI->path = iter->second.path;

					gRecvFiles.push_back(pFRI);
				}
				break;
			}
			case ACT_NO_MISSING:
				EXECUTE_SQL_V(m_pMob->GetMainDB(), ("UPDATE works SET auto_inc=%s WHERE num=%d;", iter->second.body.z, sessionId));
				break;
			}
			train.erase(iter);
		}
		else if (iter->second.action == ACT_FILE) {
			FILE *fp;
			FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.extra;

			if (pFSI) {
				if ((fp = fopen(iter->second.path.data(), "ab")) != NULL) {
					fwrite((char *)(((int*)data) + 1), sizeof(char), size - sizeof(int), fp);
					fclose(fp);
				}
			}
		}
		else mem2mem(&(iter->second.body), (char *)(((int*)data) + 1), size - sizeof(int));
	}
}
