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

#include "dbutil.h"
#include "Global.h"
#include "Sender.h"
#include "AlljoynMob.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// typedef

typedef struct {
	int snum;
	std::string data;
} APPLY;

typedef struct {
	SKEY prev;
	std::map<qcc::String, APPLY> applies;
} APPLIES;

typedef std::vector<APPLIES>			vApplies;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// static function

static void save2applies(vApplies & applies, SKEY & prev, const char * pJoiner, int snum, const char * data)
{
	vApplies::iterator iter;

	for (iter = applies.begin(); iter != applies.end(); iter++) {
		if ((*iter).prev.snum == prev.snum && (*iter).prev.joiner == prev.joiner) {
			(*iter).applies[pJoiner].snum = snum;
			(*iter).applies[pJoiner].data = data;
			break;
		}
	}
	if (iter == applies.end()) {
		applies.push_back(APPLIES());
		applies.back().prev = prev;
		(*iter).applies[pJoiner].snum = snum;
		(*iter).applies[pJoiner].data = data;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSender

void CSender::Apply(SessionId sessionId, const char * pJoiner)
{
	BOOL bDone = FALSE;
	mReceives::iterator iter;
	sqlite3 * pMainDb = m_pMob->GetMainDB();
	sqlite3 * pBackDb = m_pMob->GetBackDB();
	sqlite3 * pUndoDb = m_pMob->GetUndoDB();

	// for renewal of table list in sqlite
	sqlite3_stmt *pStmt = db_prepare(pMainDb, "SELECT name FROM main.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' UNION\n"
		"SELECT name FROM aux.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' ORDER BY name");

	while (SQLITE_ROW == sqlite3_step(pStmt));

	sqlite3_finalize(pStmt);

	for (iter = m_mReceives.begin(); iter != m_mReceives.end(); iter++) {
		vApplies applies;
		{
			RECEIVE rcv;
			int undo = INT_MAX, n;
			mReceive::iterator _iter;
			sReceive::iterator __iter;
			sReceive::iterator ___iter;

			for (_iter = iter->second.begin(); _iter != iter->second.end(); _iter++) {
				for (__iter = _iter->second.begin(); __iter != _iter->second.end(); ) {
					if ((*__iter)->data.z) {
						QUERY_SQL_V(pUndoDb, pStmt, ("SELECT num FROM works WHERE joiner=%Q AND snum=%d AND base_table=%Q", (*__iter)->prev.joiner.data(), (*__iter)->prev.snum, iter->first.data()),
							if (undo > (n = sqlite3_column_int(pStmt, 0))) undo = n;
						);

						save2applies(applies, (*__iter)->prev, _iter->first.data(), (*__iter)->snum, (*__iter)->data.z);

						blkFree(&(*__iter)->data);

						rcv.snum_end = rcv.snum = (*__iter)->snum - 1;

						if ((___iter = _iter->second.find(&rcv)) != _iter->second.end()) {
							(*___iter)->snum_end = (*__iter)->snum;
							delete (*__iter);
							__iter = _iter->second.erase(__iter);
							continue;
						}
					}
					__iter++;
				}
			}

			if (undo < INT_MAX) {
				SKEY prev = { -1, "" };

				QUERY_SQL_V(pUndoDb, pStmt, ("SELECT snum, joiner, redo FROM works WHERE num >= %d AND base_table=%Q ORDER BY num ASC", undo, iter->first.data()),
					if (prev.snum > 0) save2applies(applies, prev, (const char *)sqlite3_column_text(pStmt, 1), sqlite3_column_int(pStmt, 0), (const char *)sqlite3_column_text(pStmt, 2));
					prev.snum = sqlite3_column_int(pStmt, 0);
					prev.joiner = (const char *)sqlite3_column_text(pStmt, 1);
				);
				QUERY_SQL_V(pUndoDb, pStmt, ("SELECT undo FROM works WHERE num > %d AND base_table=%Q ORDER BY num DESC", undo, iter->first.data()),
					sqlite3_exec(pMainDb, (const char *)sqlite3_column_text(pStmt, 0), 0, 0, 0);
				);
				EXECUTE_SQL_V(pMainDb, ("DELETE FROM works WHERE num > %d AND base_table=%Q;REINDEX works;", undo, iter->first.data()));
			}
		}

		{
			Block undo;
			vApplies::iterator _iter;
			std::map<qcc::String, APPLY>::iterator __iter;

			blkInit(&undo);

			for (_iter = applies.begin(); _iter != applies.end(); _iter++) {
				for (__iter = (*_iter).applies.begin(); __iter != (*_iter).applies.end(); __iter++) {
					file_uri_replace(sessionId, pJoiner, __iter->second.data);
					sqlite3_exec(pMainDb, __iter->second.data.data(), 0, 0, 0);
					diff_one_table(pMainDb, "main", "aux", iter->first.data(), &undo);
					sqlite3_exec(pBackDb, __iter->second.data.data(), 0, 0, 0);
					EXECUTE_SQL_V(pUndoDb, ("INSERT INTO works (joiner, snum, base_table, undo, redo) VALUES (%Q, %d, %Q, %Q, %Q);", __iter->first.data(), __iter->second.snum, iter->first.data(), undo.z, __iter->second.data.data()));
					blkFree(&undo);
					bDone = TRUE;
				}
			}
		}
	}
	if (bDone && fnReceiveProc) fnReceiveProc(pMainDb);
}