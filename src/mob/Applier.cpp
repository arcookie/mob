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
typedef std::map<qcc::String, vApplies> msApplies;

typedef struct {
	SKEY skey;
	int num;
} FIRST_SKEY;

typedef std::map<qcc::String, FIRST_SKEY> mFSKey;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// static function

static void save2applies(vApplies & applies, APPLY * pApply)
{
	vApplies::iterator iter;

	for (iter = applies.begin(); iter != applies.end(); iter++) {
		printf("find parent ! prev: %d %s, cur: %d %s\n", pApply->prev.snum, pApply->prev.joiner.data(), (*iter)->cur.snum, (*iter)->cur.joiner.data());
		if ((*iter)->cur == pApply->prev) {
			(*iter)->children.insert(pApply);
			pApply->parent = (*iter);
			break;
		}
	}

	for (iter = applies.begin(); iter != applies.end(); iter++) {
		printf("find children ! cur: %d %s, child: %d %s\n", pApply->cur.snum, pApply->cur.joiner.data(), (*iter)->prev.snum, (*iter)->prev.joiner.data());
		if ((*iter)->prev == pApply->cur) {
			(*iter)->parent = pApply;
			pApply->children.insert(*iter);
			break;
		}
	}

	applies.push_back(pApply);
}

static APPLY * find_first_apply(vApplies & applies, SKEY & prev)
{
	vApplies::iterator iter;

	for (iter = applies.begin(); iter != applies.end(); iter++) {
		if ((*iter)->prev == prev) return (*iter);
	}
	return NULL;
}

static BOOL is_valid_applies(msApplies & applies, mFSKey & root) {
	msApplies::iterator msiter;
	vApplies::iterator viter;

	for (msiter = applies.begin(); msiter != applies.end(); msiter++) {
		for (viter = msiter->second.begin(); viter != msiter->second.end(); viter++) {
			if ((*viter)->parent == NULL && (*viter)->prev != root[msiter->first].skey) return FALSE;
		}
	}

	return TRUE;
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

static void apply_db(sqlite3 * pMainDb, sqlite3 * pBackDb, sqlite3 * pUndoDb, SessionId sessionId, const char * pJoiner, const char * sTable, APPLY * pApply)
{
	Block undo;

	blkInit(&undo);

	file_uri_replace(sessionId, pJoiner, pApply->data);
	sqlite3_exec(pMainDb, pApply->data.data(), 0, 0, 0);
	diff_one_table(pMainDb, "main", "aux", sTable, &undo);
	sqlite3_exec(pBackDb, pApply->data.data(), 0, 0, 0);
	EXECUTE_SQL_V(pUndoDb, ("INSERT INTO works (joiner, snum, base_table, undo, redo) VALUES (%Q, %d, %Q, %Q, %Q);", pApply->cur.joiner.data(), pApply->cur.snum, sTable, undo.z, pApply->data.data()));

	blkFree(&undo);
}

static BOOL applies_db(sqlite3 * pMainDb, sqlite3 * pBackDb, sqlite3 * pUndoDb, SessionId sessionId, const char * pJoiner, std::map<qcc::String, vApplies> & applies, mFSKey & root)
{
	APPLY * pApply;
	BOOL bDone = FALSE;
	mApplies::iterator miter;
	vApplies::iterator viter;
	msApplies::iterator msiter;

	for (msiter = applies.begin(); msiter != applies.end(); msiter++) {
		if ((pApply = find_first_apply(msiter->second, root[msiter->first].skey)) != NULL) {
			mApplies _applies;

			collect_apply(_applies, 0, pApply);

			for (miter = _applies.begin(); miter != _applies.end(); miter++) {
				for (viter = miter->second.begin(); viter != miter->second.end(); viter++) {
					apply_db(pMainDb, pBackDb, pUndoDb, sessionId, pJoiner, msiter->first.data(), *viter);
				}
			}
			bDone = TRUE;
		}

		delete_applies(msiter->second);
	}

	return bDone;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSender

void CSender::Apply(SessionId sessionId, const char * pJoiner)
{
	sqlite3 * pMainDB = m_pMob->GetMainDB();
	sqlite3 * pUndoDB = m_pMob->GetUndoDB();

	// for renewal of table list in sqlite
	sqlite3_stmt *pStmt = db_prepare(pMainDB, "SELECT name FROM main.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' UNION\n"
		"SELECT name FROM aux.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' ORDER BY name");

	while (SQLITE_ROW == sqlite3_step(pStmt));

	sqlite3_finalize(pStmt);

	int n;
	mFSKey root;
	mReceive::iterator miter;
	sReceive::iterator siter;
	msApplies applies;

	m[0].lock();

	for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {
		for (siter = miter->second.begin(); siter != miter->second.end(); siter++) {
			if ((*siter)->data.z) {
				if ((*siter)->prev.snum > 0) {
					QUERY_SQL_V(pUndoDB, pStmt, ("SELECT num FROM works WHERE joiner=%Q AND snum=%d", (*siter)->prev.joiner.data(), (*siter)->prev.snum),
						if (root.find((*siter)->base_table) == root.end()) root[(*siter)->base_table].num = INT_MAX;
						if (root[(*siter)->base_table].num > (n = sqlite3_column_int(pStmt, 0))) {
							root[(*siter)->base_table].num = n;
							root[(*siter)->base_table].skey = (*siter)->prev;
						}
					);
				}
				else {
					root[(*siter)->base_table].num = -1;
					root[(*siter)->base_table].skey = (*siter)->prev;
				}
				save2applies(applies[(*siter)->base_table], new APPLY((*siter)->prev, SKEY((*siter)->snum, miter->first.data()), (*siter)->data.z));
			}
		}
	}

	m[0].unlock();

	SKEY prev, cur;
	BOOL bUndo = FALSE;
	mFSKey::iterator mfiter;

	for (mfiter = root.begin(); mfiter != root.end(); mfiter++) {
		if (mfiter->second.num < INT_MAX) {
			prev = mfiter->second.skey;
			QUERY_SQL_V(pUndoDB, pStmt, ("SELECT snum, joiner, redo FROM works WHERE num > %d AND base_table=%Q ORDER BY num ASC", mfiter->second.num, mfiter->first.data()),
				cur = SKEY(sqlite3_column_int(pStmt, 0), (const char *)sqlite3_column_text(pStmt, 1));
				save2applies(applies[mfiter->first], new APPLY(prev, cur, (const char *)sqlite3_column_text(pStmt, 2)));
				prev = cur;
				bUndo = TRUE;
			);
		}
	}

	if (is_valid_applies(applies, root)) {
		if (bUndo) {
			for (mfiter = root.begin(); mfiter != root.end(); mfiter++) {
				if (mfiter->second.num < INT_MAX) {
					QUERY_SQL_V(pUndoDB, pStmt, ("SELECT undo FROM works WHERE num > %d AND base_table=%Q ORDER BY num DESC", mfiter->second.num, mfiter->first.data()),
						sqlite3_exec(pMainDB, (const char *)sqlite3_column_text(pStmt, 0), 0, 0, 0);
					);
					EXECUTE_SQL_V(pMainDB, ("DELETE FROM works WHERE num > %d AND base_table=%Q;", mfiter->second.num, mfiter->first.data()));
				}
			}
			sqlite3_exec(pMainDB, "REINDEX works;", 0, 0, 0);
		}

		m[0].lock();

		for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {
			sReceive::iterator prev = miter->second.begin();
			for (siter = miter->second.begin(); siter != miter->second.end();) {
				if ((*siter)->data.z) {
					blkFree(&(*siter)->data);
					if (prev != siter) {
						(*prev)->snum_end = (*siter)->snum;
						delete (*siter);
						siter = miter->second.erase(siter);
					}
				}
				else {
					prev = siter;
					siter++;
				}
			}
		}

		m[0].unlock();

		if (applies_db(pMainDB, m_pMob->GetBackDB(), m_pMob->GetUndoDB(), sessionId, pJoiner, applies, root) && fnReceiveProc) fnReceiveProc(pMainDB);
	}
}
