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
		return (m.joiner_prev == joiner_prev && m.snum_prev == snum_prev);
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
	SYNC_DATA sd;
	sqlite3 * pMainDb = m_pMob->GetMainDB();
	sqlite3 * pBackDb = m_pMob->GetBackDB();
	sqlite3 * pUndoDb = m_pMob->GetUndoDB();
	mReceives::iterator iter;
	mReceive::iterator _mRcvIt;
	sReceive::iterator sRcvIt;
	RECEIVE rcv;

	// for renewal of table list in sqlite
	sqlite3_stmt *pStmt = db_prepare(pMainDb, "SELECT name FROM main.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' UNION\n"
		"SELECT name FROM aux.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' ORDER BY name");

	while (SQLITE_ROW == sqlite3_step(pStmt));

	sqlite3_finalize(pStmt);
	
	for (iter = m_mReceives.begin(); iter != m_mReceives.end(); iter++) {
		vApplies applies;
		/*
		for (vRcvIt = mRcvIt->second.begin(); vRcvIt != mRcvIt->second.end();) {
			if (!(*vRcvIt)->data.empty() && (n = mob_find_parent_db(sessionId, (*vRcvIt)->joiner_prev.data(), (*vRcvIt)->snum_prev, mRcvIt->first.data())) > num) num = n;
		}

		while ((num = mob_get_db(sessionId, num, mRcvIt->first.data(), &sd)) > 0) PushApply(applies, apply, mRcvIt->first.data(), sd.joiner_prev, sd.snum_prev, TRUE);

		bFirst = TRUE;

		do {
			bWorked = FALSE;
			for (_mRcvIt = mRcvIt->second.begin(); _mRcvIt != mRcvIt->second.end(); _mRcvIt++) {
				for (sRcvIt = _mRcvIt->second.begin(); sRcvIt != _mRcvIt->second.end();) {

					//apply.snum = (*sRcvIt)->snum;
					//apply.joiner = _mRcvIt->first;
					//apply.data = (*sRcvIt)->data;

					if (!(*sRcvIt)->data.empty() && PushApply(applies, _mRcvIt->first.data(), (*sRcvIt), mRcvIt->first.data(), (*sRcvIt)->joiner_prev.data(), (*sRcvIt)->snum_prev, bFirst)) {
						bFirst = FALSE;
						(*sRcvIt)->data.clear();

						rcv.snum_end = rcv.snum = (*sRcvIt)->snum - 1;

						sReceive::iterator it = _mRcvIt->second.find(&rcv);

						if (it != _mRcvIt->second.end()) {
							(*it)->snum_end = (*sRcvIt)->snum;
							delete (*sRcvIt);
							sRcvIt = _mRcvIt->second.erase(sRcvIt);
						}
						else sRcvIt++;

						bWorked = TRUE;
					}
				}
			}
		} while (bWorked);
		*/

		vApplies::iterator _iter = applies.begin();

		if (_iter != applies.end()) {
			sqlite3_stmt *pStmt = NULL;

			QUERY_SQL_V(pUndoDb, pStmt, ("SELECT num FROM works WHERE joiner=%Q AND snum=%d AND base_table=%Q", (*_iter).joiner_prev.data(), (*_iter).snum_prev, iter->first.data()),
				sqlite3_stmt *_pStmt = NULL;
				int num = sqlite3_column_int(pStmt, 0);

				QUERY_SQL_V(pUndoDb, _pStmt, ("SELECT undo FROM works WHERE num > %d AND base_table=%Q ORDER BY num DESC", num, iter->first.data()),
					sqlite3_exec(pMainDb, (const char *)sqlite3_column_text(_pStmt, 0), 0, 0, 0););
				EXECUTE_SQL_V(pMainDb, ("DELETE FROM works WHERE num > %d AND base_table=%Q;REINDEX works;", num, iter->first.data()));
				break;
			);

			Block undo;
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
		if (bDone && fnReceiveProc) fnReceiveProc(pMainDb);
	}
}