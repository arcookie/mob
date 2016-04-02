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

class APPLY;

struct CompareApply
{
	bool operator()(APPLY const * _Left, APPLY const * _Right) const;
};

typedef std::set<APPLY *, CompareApply>	sApply;

class APPLY {
public:
	APPLY(SKEY & p, SKEY & c, const char * d) :prev(p), cur(c), data(d) { parent = NULL; }

	SKEY prev;
	SKEY cur;
	std::string	data;

	APPLY *	parent; 
	sApply	children;
};

typedef std::vector<APPLY*>		vApplies;

inline bool CompareApply::operator()(APPLY const * _Left, APPLY const * _Right) const
{
	return  _Left->cur.joiner < _Right->cur.joiner;
}

typedef std::map<int, vApplies>	mApplies;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// static function

static BOOL save2applies(vApplies & applies, APPLY * pApply)
{
	BOOL bAlone = TRUE;
	vApplies::iterator iter;

	for (iter = applies.begin(); iter != applies.end(); iter++) {
		if ((*iter)->cur == pApply->prev) {
			(*iter)->children.insert(pApply);
			pApply->parent = (*iter);
			bAlone = FALSE;
			break;
		}
	}

	for (iter = applies.begin(); iter != applies.end(); iter++) {
		if ((*iter)->prev == pApply->cur) {
			(*iter)->parent = pApply;
			pApply->children.insert(*iter);
			break;
		}
	}

	applies.push_back(pApply);

	return bAlone || applies.size() == 1;
}

static APPLY * find_first_apply(vApplies & applies, SKEY & prev)
{
	vApplies::iterator iter;

	for (iter = applies.begin(); iter != applies.end(); iter++) {
		if ((*iter)->prev == prev) return (*iter);
	}
	return NULL;
}

static void delete_applies(vApplies & applies)
{
	vApplies::iterator iter;

	for (iter = applies.begin(); iter != applies.end(); iter++) {
		delete (*iter);
	}
}

static void collect_apply(mApplies & applies, int level, APPLY * pApply)
{
	sApply::iterator iter;

	applies[level].push_back(pApply);
	for (iter = pApply->children.begin(); iter != pApply->children.end(); iter++) {
		collect_apply(applies, level + 1, *iter);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSender

void CSender::Apply(SessionId sessionId, const char * pJoiner)
{
	RECEIVE rcv;
	int u, n;
	BOOL bDone = FALSE;
	mReceive::iterator _iter;
	sReceive::iterator __iter;
	sReceive::iterator ___iter;
	sqlite3 * pMainDb = m_pMob->GetMainDB();
	sqlite3 * pBackDb = m_pMob->GetBackDB();
	sqlite3 * pUndoDb = m_pMob->GetUndoDB();

	// for renewal of table list in sqlite
	sqlite3_stmt *pStmt = db_prepare(pMainDb, "SELECT name FROM main.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' UNION\n"
		"SELECT name FROM aux.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' ORDER BY name");

	while (SQLITE_ROW == sqlite3_step(pStmt));

	sqlite3_finalize(pStmt);

	{
		vApplies applies;

		u = INT_MAX;

		for (_iter = m_mReceives.begin(); _iter != m_mReceives.end(); _iter++) {
			for (__iter = _iter->second.begin(); __iter != _iter->second.end(); ) {
				if ((*__iter)->data.z) {
					if ((*__iter)->prev.snum > 0) {
						QUERY_SQL_V(pUndoDb, pStmt, ("SELECT num FROM works WHERE joiner=%Q AND snum=%d AND base_table=%Q", (*__iter)->prev.joiner.data(), (*__iter)->prev.snum, (*__iter)->base_table.data()),
							if (u > (n = sqlite3_column_int(pStmt, 0))) u = n;
						);
					}

					if (!save2applies(applies, new APPLY((*__iter)->prev, SKEY((*__iter)->snum, _iter->first.data()), (*__iter)->data.z))) {
						blkFree(&(*__iter)->data);

						rcv.set((*__iter)->snum - 1, (*__iter)->snum - 1);

						if ((___iter = _iter->second.find(&rcv)) != _iter->second.end()) {
							(*___iter)->snum_end = (*__iter)->snum;
							delete (*__iter);
							__iter = _iter->second.erase(__iter);
							continue;
						}
					}
				}
				__iter++;
			}
		}

		SKEY first_key = { -1, "" };

		if (u < INT_MAX) {
			SKEY prev = { -1, "" };

			QUERY_SQL_V(pUndoDb, pStmt, ("SELECT snum, joiner, redo FROM works WHERE num >= %d AND base_table=%Q ORDER BY num ASC", u, (*__iter)->base_table.data()),
				if (prev.snum > 0) save2applies(applies, new APPLY(prev, SKEY(sqlite3_column_int(pStmt, 0), (const char *)sqlite3_column_text(pStmt, 1)), (const char *)sqlite3_column_text(pStmt, 2)));
				prev.snum = sqlite3_column_int(pStmt, 0);
				prev.joiner = (const char *)sqlite3_column_text(pStmt, 1);
			);
			QUERY_SQL_V(pUndoDb, pStmt, ("SELECT undo FROM works WHERE num > %d AND base_table=%Q ORDER BY num DESC", u, (*__iter)->base_table.data()),
				sqlite3_exec(pMainDb, (const char *)sqlite3_column_text(pStmt, 0), 0, 0, 0);
			);
			EXECUTE_SQL_V(pMainDb, ("DELETE FROM works WHERE num > %d AND base_table=%Q;REINDEX works;", u, (*__iter)->base_table.data()));

			QUERY_SQL_V(pUndoDb, pStmt, ("SELECT snum, joiner FROM works WHERE num == %d AND base_table=%Q LIMIT 1", u, (*__iter)->base_table.data()),
				first_key = SKEY(sqlite3_column_int(pStmt, 0), (const char *)sqlite3_column_text(pStmt, 1));
				break;
			);
		}

		Block undo;
		mApplies _applies;
		mApplies::iterator ____iter;
		vApplies::iterator _____iter;

		blkInit(&undo);

		collect_apply(_applies, 0, find_first_apply(applies, first_key));

		for (____iter = _applies.begin(); ____iter != _applies.end(); ____iter++) {
			for (_____iter = ____iter->second.begin(); _____iter != ____iter->second.end(); _____iter++) {
				file_uri_replace(sessionId, pJoiner, (*_____iter)->data);
				sqlite3_exec(pMainDb, (*_____iter)->data.data(), 0, 0, 0);
				diff_one_table(pMainDb, "main", "aux", (*__iter)->base_table.data(), &undo);
				sqlite3_exec(pBackDb, (*_____iter)->data.data(), 0, 0, 0);
				EXECUTE_SQL_V(pUndoDb, ("INSERT INTO works (joiner, snum, base_table, undo, redo) VALUES (%Q, %d, %Q, %Q, %Q);", (*_____iter)->cur.joiner.data(), (*_____iter)->cur.snum, (*__iter)->base_table.data(), undo.z, (*_____iter)->data.data()));
				blkFree(&undo);
				bDone = TRUE;
			}
		}

		delete_applies(applies);
	}
	if (bDone && fnReceiveProc) fnReceiveProc(pMainDb);
}