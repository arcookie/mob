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
	bool operator()(APPLIES const * m) const {
		return (m->joiner_prev == joiner_prev && m->snum_prev == snum_prev);
	}
};

BOOL CSender::PushApply(vApplies & applies, APPLY & apply, const char * sTable, const char * sJoinerPrev, int nSNumPrev, BOOL bFirst)
{
	vApplies::iterator viter;

	if ((viter = std::find_if(applies.begin(), applies.end(), find_id_applies(sJoinerPrev, nSNumPrev))) != applies.end()) (*viter)->applies.insert(apply);
	else if (bFirst) {
		APPLIES * pAppl = new APPLIES;

		pAppl->applies.insert(apply);
		applies.push_back(pAppl);
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
	BOOL bDone = FALSE;
	BOOL bFirst;
	int num, n;
	BOOL bWorked;
	APPLY apply;
	SYNC_DATA sd;
	sqlite3 * pMainDb = m_pMob->GetMainDB();
	sqlite3 * pBackDb = m_pMob->GetBackDB();
	sqlite3 * pUndoDb = m_pMob->GetUndoDB();
	sApplies::iterator siter;
	vApplies::iterator viter;
	mReceives::iterator mRcvIt;
	vReceives::iterator vRcvIt;

	sqlite3_stmt *pStmt = db_prepare(pMainDb, "SELECT name FROM main.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' UNION\n"
		"SELECT name FROM aux.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' ORDER BY name");

	while (SQLITE_ROW == sqlite3_step(pStmt));

	sqlite3_finalize(pStmt);

	for (mRcvIt = m_mReceives.begin(); mRcvIt != m_mReceives.end(); mRcvIt++) {
		vApplies applies;

		//for (vRcvIt = mRcvIt->second.begin(); vRcvIt != mRcvIt->second.end();) {
		//	if (!(*vRcvIt)->data.empty() && (n = mob_find_parent_db(sessionId, (*vRcvIt)->joiner_prev.data(), (*vRcvIt)->snum_prev, mRcvIt->first.data())) > num) num = n;
		//}

		//while ((num = mob_get_db(sessionId, num, mRcvIt->first.data(), &sd)) > 0) PushApply(applies, apply, mRcvIt->first.data(), sd.joiner_prev, sd.snum_prev, TRUE);

		bFirst = TRUE;

		do {
			bWorked = FALSE;
			for (vRcvIt = mRcvIt->second.begin(); vRcvIt != mRcvIt->second.end();) {

				apply.snum = (*vRcvIt)->snum;
				apply.joiner = (*vRcvIt)->joiner;
				apply.data = (*vRcvIt)->data;

				if (!(*vRcvIt)->data.empty() && PushApply(applies, apply, mRcvIt->first.data(), (*vRcvIt)->joiner_prev.data(), (*vRcvIt)->snum_prev, bFirst)) {
					bFirst = FALSE;
					(*vRcvIt)->data.clear();

					vReceives::iterator it = std::find_if(mRcvIt->second.begin(), mRcvIt->second.end(), find_id((*vRcvIt)->joiner, (*vRcvIt)->snum - 1));

					if (it != mRcvIt->second.end()) {
						(*it)->snum_end = (*vRcvIt)->snum;
						delete (*vRcvIt);
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

			blkInit(&undo);

			QUERY_SQL_V(pUndoDb, pStmt2, ("SELECT num FROM works WHERE joiner=%Q AND snum=%d AND base_table=%Q", (*viter)->joiner_prev.data(), (*viter)->snum_prev, mRcvIt->first.data()),
				int num = sqlite3_column_int(pStmt2, 0);

			QUERY_SQL_V(pUndoDb, pStmt3, ("SELECT undo FROM works WHERE num > %d AND base_table=%Q ORDER BY num DESC", num, mRcvIt->first.data()),
				sqlite3_exec(pMainDb, (const char *)sqlite3_column_text(pStmt3, 0), 0, 0, 0);
			);
			EXECUTE_SQL_V(pMainDb, ("DELETE FROM works WHERE num > %d AND base_table=%Q;REINDEX works;", num, mRcvIt->first.data()));
			break;
			);

			for (viter = applies.begin(); viter != applies.end(); viter++) {
				for (siter = (*viter)->applies.begin(); siter != (*viter)->applies.end(); siter++) {
					sqlite3_exec(pMainDb, (*siter).data.data(), 0, 0, 0);
					bDone = TRUE;
				}
			}

			diff_one_table(pMainDb, "main", "aux", mRcvIt->first.data(), &undo);

			for (viter = applies.begin(); viter != applies.end(); viter++) {
				for (siter = (*viter)->applies.begin(); siter != (*viter)->applies.end(); siter++) {
					sqlite3_exec(pBackDb, (*siter).data.data(), 0, 0, 0);
					EXECUTE_SQL_V(pUndoDb, ("INSERT INTO works (joiner, snum, base_table, undo, redo) VALUES (%Q, %d, %Q, %Q, %Q);", (*siter).joiner.data(), (*siter).snum, mRcvIt->first.data(), undo.z, (*siter).data.data()));
				}
			}
			blkFree(&undo);
		}
		for (viter = applies.begin(); viter != applies.end(); viter++) {
			delete (*viter);
		}
		if (bDone && fnReceiveProc) fnReceiveProc(pMainDb);
	}
}