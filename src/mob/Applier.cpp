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


struct find_id_applies : std::unary_function<RECEIVE*, bool> {
	int snum_prev;
	qcc::String joiner_prev;
	find_id_applies(const char * u, int sn) :joiner_prev(u), snum_prev(sn) { }
	bool operator()(APPLIES const & m) const {
		return (m.prev.joiner == joiner_prev && m.prev.snum == snum_prev);
	}
};

BOOL CSender::PushApply(vApplies & applies, const char * sJoiner, const RECEIVE * pReceive, const char * sTable, const char * sJoinerPrev, int nSNumPrev, BOOL bFirst)
{
	vApplies::iterator viter;

	if ((viter = std::find_if(applies.begin(), applies.end(), find_id_applies(sJoinerPrev, nSNumPrev))) != applies.end()) (*viter).applies[sJoiner] = pReceive;
	else if (bFirst) {
		APPLIES appl;

		appl.applies[sJoiner] = pReceive;

		applies.push_back(appl);
		bFirst = FALSE;
	}
	return TRUE;
}

void CSender::Apply(SessionId sessionId)
{
	BOOL bDone = FALSE;
	BOOL bFirst;
	sqlite3 * pMainDb = m_pMob->GetMainDB();
	sqlite3 * pBackDb = m_pMob->GetBackDB();
	sqlite3 * pUndoDb = m_pMob->GetUndoDB();
	mReceives::iterator iter;
	RECEIVE rcv;

	// for renewal of table list in sqlite
	sqlite3_stmt *pStmt = db_prepare(pMainDb, "SELECT name FROM main.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' UNION\n"
		"SELECT name FROM aux.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' ORDER BY name");

	while (SQLITE_ROW == sqlite3_step(pStmt));

	sqlite3_finalize(pStmt);
	
	for (iter = m_mReceives.begin(); iter != m_mReceives.end(); iter++) {
		vApplies applies;
		{
			int undo = INT_MAX, n;
			mReceive::iterator _iter;
			sReceive::iterator __iter;
			sReceive::iterator ___iter;

			for (_iter = iter->second.begin(); _iter != iter->second.end(); _iter++) {
				for (__iter = _iter->second.begin(); __iter != _iter->second.end(); ) {
					if (!(*__iter)->data.empty()) {
						QUERY_SQL_V(pUndoDb, pStmt, ("SELECT num FROM works WHERE joiner=%Q AND snum=%d AND base_table=%Q", (*__iter)->prev.joiner.data(), (*__iter)->prev.snum, iter->first.data()),
							if (undo > (n = sqlite3_column_int(pStmt, 0))) undo = n;
						);
						// push apply (reversed)

						(*__iter)->data.clear();

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
				QUERY_SQL_V(pUndoDb, pStmt, ("SELECT undo FROM works WHERE num > %d AND base_table=%Q ORDER BY num DESC", undo, iter->first.data()),
					// push apply (reversed)
					sqlite3_exec(pMainDb, (const char *)sqlite3_column_text(pStmt, 0), 0, 0, 0);
				);
				EXECUTE_SQL_V(pMainDb, ("DELETE FROM works WHERE num > %d AND base_table=%Q;REINDEX works;", undo, iter->first.data()));
			}
		}

		{
			Block undo;
			vApplies::iterator _iter;
			mApplies::iterator __iter;

			blkInit(&undo);

			for (_iter = applies.begin(); _iter != applies.end(); _iter++) {
				for (__iter = (*_iter).applies.begin(); __iter != (*_iter).applies.end(); __iter++) {
					sqlite3_exec(pMainDb, __iter->second->data.data(), 0, 0, 0);
					diff_one_table(pMainDb, "main", "aux", iter->first.data(), &undo);
					sqlite3_exec(pBackDb, __iter->second->data.data(), 0, 0, 0);
					EXECUTE_SQL_V(pUndoDb, ("INSERT INTO works (joiner, snum, base_table, undo, redo) VALUES (%Q, %d, %Q, %Q, %Q);", __iter->first.data(), __iter->second->snum, iter->first.data(), undo.z, __iter->second->data.data()));
					blkFree(&undo);
					bDone = TRUE;
				}
			}
		}
	}
	if (bDone && fnReceiveProc) fnReceiveProc(pMainDb);
}