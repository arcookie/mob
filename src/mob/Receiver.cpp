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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// define

#define IS_TRAIN_HEADER(a)	\
	(((int*)a)[0] == TRAIN_MARK_1 && ((int*)a)[1] == TRAIN_MARK_2 && ((int*)a)[2] == TRAIN_MARK_3 && \
	((int*)a)[3] == TRAIN_MARK_4 && ((int*)a)[4] == TRAIN_MARK_5 && ((int*)a)[5] == TRAIN_MARK_6)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// typedef

typedef struct {
	int session_id;
	int mtime;
	long fsize;
	qcc::String uri;
	qcc::String path;
	qcc::String joiner;
} FILE_RECV_ITEM;

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

typedef std::vector<FILE_RECV_ITEM*>		vRecvFiles;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// global variables

vRecvFiles gRecvFiles;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// static functions

void file_uri_replace(SessionId sessionId, const char * pJoiner, std::string & data)
{
	qcc::String p;
	vRecvFiles::iterator iter;
	size_t start_pos = 0;
	size_t end_pos;

	while ((start_pos = data.find("file://", start_pos)) != std::string::npos) {
		if ((end_pos = data.find("\'", start_pos)) != std::string::npos) {
			if ((iter = std::find_if(gRecvFiles.begin(), gRecvFiles.end(), find_uri(sessionId, pJoiner, data.substr(start_pos, end_pos - start_pos).data()))) != gRecvFiles.end())
				data.replace(start_pos, end_pos - start_pos, get_uri((*iter)->path.data()).data());
			start_pos += p.length();
		}
		else start_pos++;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSender

CSender::~CSender()
{
	{
		vRecvFiles::iterator iter;

		for (iter = gRecvFiles.begin(); iter != gRecvFiles.end(); iter++){
			delete (*iter);
		}
	}

	{
		mReceive::iterator _iter;
		sReceive::iterator __iter;

		for (_iter = m_mReceives.begin(); _iter != m_mReceives.end(); _iter++) {
			for (__iter = _iter->second.begin(); __iter != _iter->second.end(); __iter++) {
				delete (*__iter);
			}
		}
	}
}

void CSender::Save(SessionId sessionId, const char * pJoiner, Block * pText, const char * pExtra, int nExtLen)
{
	RECEIVE * pRCV = new RECEIVE;
	SYNC_DATA * pSD = (SYNC_DATA *)pExtra;

	pRCV->prev.joiner = pSD->joiner_prev;
	pRCV->prev.snum = pSD->snum_prev;
	pRCV->set(pSD->snum, pSD->snum);
	pRCV->base_table = pSD->base_table;
	blkMove(&pRCV->data, pText);

	m_mReceives[pSD->joiner].insert(pRCV);

	if (!SetMissingTimer()) Apply(sessionId, pJoiner);
}

void CSender::SaveFileInfo(SessionId sessionId, const char * pJoiner, const char * sPath, const FILE_SEND_ITEM * pFSI)
{
	if (pFSI) {
		vRecvFiles::iterator _iter;

		if ((_iter = std::find_if(gRecvFiles.begin(), gRecvFiles.end(), find_uri(sessionId, pJoiner, pFSI->uri))) != gRecvFiles.end()) {
			(*_iter)->fsize = pFSI->fsize;
			(*_iter)->mtime = pFSI->mtime;
			(*_iter)->path = sPath;
		}
		else {
			FILE_RECV_ITEM * pFRI = new FILE_RECV_ITEM;

			pFRI->fsize = pFSI->fsize;
			pFRI->mtime = pFSI->mtime;
			pFRI->uri = pFSI->uri;
			pFRI->joiner = pJoiner;
			pFRI->session_id = sessionId;
			pFRI->path = sPath;

			gRecvFiles.push_back(pFRI);
		}
	}
}

void CSender::OnDataEnd(int footprint, const char * pJoiner)
{
	mTrain::iterator iter;
	SessionId sessionId = m_pMob->GetSessionID();

	if ((iter = m_mStation[pJoiner].find(footprint)) != m_mStation[pJoiner].end()){
		Save(sessionId, pJoiner, &(iter->second.body), iter->second.extra, TRAIN_EXTRA_LEN);
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

		if (pTH->action == ACT_END) OnDataEnd(pTH->footprint, pJoiner);
		else if (pTH->action == ACT_SIGNAL) {
			SYNC_SIGNAL * pSS = (SYNC_SIGNAL *)pTH->extra;

			if (pSS) MissingCheck(pSS->joiner, pSS->snum);
		}
		else if (pTH->action == ACT_NO_MISSING) m_pMob->SetSignal(pJoiner, false);
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
				qcc::String having;

				having.assign(iter->second.body.z, iter->second.body.nUsed);

				blkFree(&(iter->second.body));

				printf("%s: %ssqlite>", msg->GetSender(), having.data());

				QUERY_SQL_V(m_pMob->GetUndoDB(), pStmt2, ("SELECT num, snum, base_table, redo FROM works WHERE joiner=%Q AND snum IN (%s)", sd.joiner, having.data()),
					sd.snum = sqlite3_column_int(pStmt2, 1);
					strcpy_s(sd.base_table, sizeof(sd.base_table), (const char *)sqlite3_column_text(pStmt2, 2));
					sd.snum_prev = -1;
					QUERY_SQL_V(m_pMob->GetUndoDB(), pStmt, ("SELECT joiner, snum FROM works WHERE num < %d AND base_table=%Q ORDER BY num DESC LIMIT 1", sqlite3_column_int(pStmt2, 0), sd.base_table),
						strcpy_s(sd.joiner_prev, sizeof(sd.joiner_prev), (const char *)sqlite3_column_text(pStmt, 0));
						sd.snum_prev = sqlite3_column_int(pStmt, 1);
						break;
					);
					alljoyn_send(sessionId, msg->GetSender(), ACT_DATA, (char *)sqlite3_column_text(pStmt2, 3), sqlite3_column_bytes(pStmt2, 3) + 1, (const char *)&sd, sizeof(SYNC_DATA));
				);
				break;
			}
			case ACT_DATA:
			{
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
					vRecvFiles::iterator _iter;
					FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.body.z;

					blkInit(&data);

					while (n < iter->second.body.nUsed) {
						if ((_iter = std::find_if(gRecvFiles.begin(), gRecvFiles.end(), find_uri(sessionId, msg->GetSender(), pFSI->uri))) == gRecvFiles.end() ||
							(*_iter)->mtime != pFSI->mtime || (*_iter)->mtime != pFSI->mtime)
							mem2mem(&data, (char *)pFSI, sizeof(FILE_SEND_ITEM));

						n += sizeof(FILE_SEND_ITEM);
						pFSI++;
					}
					if (data.nUsed > 0) SendData(pJoiner, iter->second.footprint, ACT_FLIST_REQ, sessionId, data.z, data.nUsed); // special target most be assigned.
					else OnDataEnd(iter->second.footprint, pJoiner);

					blkFree(&data);
					blkFree(&(iter->second.body));
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
				blkFree(&(iter->second.body));
				break;
			case ACT_FILE:
				SaveFileInfo(sessionId, pJoiner, iter->second.path.data(), (FILE_SEND_ITEM *)iter->second.extra);
				break;
			default:
				blkFree(&(iter->second.body));
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
