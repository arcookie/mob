/*
*   2016.2.25
*
*   Copyright arCookie. All rights reserved.
*
*   The license under which the Mob source code is released is the GPLv2 (or later) from the Free Software Foundation. 
*
*   A copy of the license is included with every copy of Mob source code, but you can also read the text of the license here(http://www.arcookie.com/?page_id=414).
*/

#include <stdio.h>
#include <stdlib.h>

#include "mob.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// defines

#define QUERY_SQL(__db__, __stmt__, __sql__, ...)	\
	if (sqlite3_prepare_v2(__db__, __sql__, -1, &__stmt__, NULL) == SQLITE_OK) {\
while (sqlite3_step(__stmt__) == SQLITE_ROW) { __VA_ARGS__ } sqlite3_finalize(__stmt__); __stmt__ = NULL;}

#define QUERY_SQL_V(__db__, __stmt__, __sql__, ...)	\
	{ char * __zSQL__ = sqlite3_mprintf __sql__; if (__zSQL__ && sqlite3_prepare_v2(__db__, __zSQL__, -1, &__stmt__, NULL) == SQLITE_OK) {\
	while (sqlite3_step(__stmt__) == SQLITE_ROW) { __VA_ARGS__ } sqlite3_finalize(__stmt__); __stmt__ = NULL;}	sqlite3_free(__zSQL__);	}

#define EXECUTE_SQL_V(__db__, __sql__, ...)	\
	{ char * __zSQL__ = sqlite3_mprintf __sql__; if (__zSQL__) { sqlite3_exec(__db__, __zSQL__, 0, 0, 0); sqlite3_free(__zSQL__); }}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// static variables

sqlite3 * master_db = 0;           /* The database */

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// static functions

static void usage()
{
	printf("Usage: mob [-h] [-s <name>] | [-j <name>]\n");
	exit(EXIT_FAILURE);
}

static int _create_db(long id, const char * mark, sqlite3 **ppDb)
{
	Block fname;

	blkInit(&fname);

	strPrintf(&fname, "%ld_%s.db3", id, mark);

	_unlink(fname.z);

	int rc = sqlite3_open(fname.z, ppDb);

	blkFree(&fname);

	return rc;
}

static int _close_db(long id, const char * mark, sqlite3 *pDb)
{
	Block fname;
	int rc = sqlite3_close(pDb);

	blkInit(&fname);

	strPrintf(&fname, "%ld_%s.db3", id, mark);

	_unlink(fname.z);

	blkFree(&fname);

	return rc;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// global functions

void mob_apply_db(unsigned int sid, const char * uid, int sn, int snum, const char * base, const char * sql)
{
	Block undo;
	sqlite3_stmt *pStmt = NULL;

	blkInit(&undo);

	QUERY_SQL_V(master_db, pStmt, ("SELECT ptr_main, ptr_back, ptr_undo FROM works WHERE num=%d", sid),
		sqlite3 * pDB = (sqlite3 *)sqlite3_column_int64(pStmt, 0);
		sqlite3 * pBackDB = (sqlite3 *)sqlite3_column_int64(pStmt, 1);
		sqlite3 * pUndoDB = (sqlite3 *)sqlite3_column_int64(pStmt, 2);

		sqlite3_exec(pDB, sql, 0, 0, 0);
		diff_one_table(pDB, "main", "aux", base, &undo);
		sqlite3_exec(pBackDB, sql, 0, 0, 0);
		EXECUTE_SQL_V(pUndoDB, ("INSERT INTO works (uid, sn, snum, base, undo, redo) VALUES (%Q, %d, %d, %Q, %Q, %Q);", uid, sn, snum, base, undo.z, sql));
		break;
	);

	blkFree(&undo);
}

int mob_init(int argc, char** argv)
{
	char * joinName = 0;
	char * advertisedName = 0;

	/* Parse command line args */
	for (int i = 1; i < argc; ++i) {
		if (0 == strcmp("-s", argv[i])) {
			if ((++i < argc) && (argv[i][0] != '-')) {
				advertisedName = sqlite3_mprintf("%s%s", NAME_PREFIX, argv[i]);
			}
			else {
				printf("Missing parameter for \"-s\" option\n");
				usage();
			}
		}
		else if (0 == strcmp("-j", argv[i])) {
			if ((++i < argc) && (argv[i][0] != '-')) {
				joinName = sqlite3_mprintf("%s%s", NAME_PREFIX, argv[i]);
			}
			else {
				printf("Missing parameter for \"-j\" option\n");
				usage();
			}
		}
		else {
			if (0 != strcmp("-h", argv[i])) printf("Unknown argument \"%s\"\n", argv[i]);
			usage();
		}
	}
	/* Validate command line */
	if (advertisedName && joinName) {
		printf("Must specify either -s or -j\n");
		usage();
	}
	else if (!advertisedName && !joinName) {
		printf("Cannot specify both -s  and -j\n");
		usage();
	}

	int ret = alljoyn_connect(advertisedName, joinName);

	if (advertisedName) sqlite3_free(advertisedName);
	if (joinName) sqlite3_free(joinName);

	return ret;
}

void mob_exit(void)
{
	alljoyn_disconnect();
}

int mob_open_db(const char *zFilename, sqlite3 **ppDb)
{
	if (!master_db && sqlite3_open(":memory:", &master_db) == SQLITE_OK)
		sqlite3_exec(master_db, "CREATE TABLE works (num INTEGER PRIMARY KEY AUTOINCREMENT DEFAULT 1, uid CHAR(16), ptr_main BIGINT, ptr_back BIGINT, ptr_undo BIGINT); PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
	else {
		master_db = 0;
		return SQLITE_ERROR;
	}

	if (master_db) {
		int nRet = sqlite3_open(zFilename, ppDb);

		if (nRet == SQLITE_OK) {
			long id = (long)*ppDb;
			sqlite3 *pBackDb;
			sqlite3 *pUndoDb;

			if ((nRet = _create_db(id, "bak", &pBackDb)) == SQLITE_OK)
				sqlite3_exec(pBackDb, "PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
			else return nRet;

			if ((nRet = _create_db(id, "undo", &pUndoDb)) == SQLITE_OK) 
				sqlite3_exec(pUndoDb, "CREATE TABLE works (num INTEGER PRIMARY KEY AUTOINCREMENT DEFAULT 1, uid CHAR(16), sn INT DEFAULT 1, snum INT DEFAULT 1, base VARCHAR(64), undo TEXT, redo TEXT);PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
			else return nRet;

			EXECUTE_SQL_V(*ppDb, ("PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;ATTACH '%ld_bak.db3' as aux;", id));
			EXECUTE_SQL_V(master_db, ("INSERT INTO works (num, uid, ptr_main, ptr_back, ptr_undo) VALUES (%d, %d, %ld, %ld, %ld);", alljoyn_session_id(), alljoyn_join_name(), (long)*ppDb, (long)pBackDb, (long)pUndoDb));
		}

		return nRet;
	}
	return SQLITE_ERROR;
}

void mob_undo_db(unsigned int sid, const char * uid, int snum, const char * base)
{
	sqlite3_stmt *pStmt = NULL;
	sqlite3_stmt *pStmt2 = NULL;
	sqlite3_stmt *pStmt3 = NULL;

	QUERY_SQL_V(master_db, pStmt, ("SELECT ptr_main, ptr_undo FROM works WHERE num=%d", sid),
		sqlite3 * pDB = (sqlite3 *)sqlite3_column_int64(pStmt, 0);
		sqlite3 * pDBUndo = (sqlite3 *)sqlite3_column_int64(pStmt, 1);

		QUERY_SQL_V(pDBUndo, pStmt2, ("SELECT num FROM works WHERE uid=%Q AND snum=%d AND base=%Q", uid, snum, base),
			int num = sqlite3_column_int(pStmt2, 0);

			QUERY_SQL_V(pDBUndo, pStmt3, ("SELECT undo FROM works WHERE num > %d AND base=%Q ORDER BY num DESC", num, base),
				sqlite3_exec(pDB, sqlite3_column_text(pStmt3, 0), 0, 0, 0);
			);
			EXECUTE_SQL_V(pDB, ("DELETE FROM works WHERE num > %d AND base=%Q;REINDEX works;", num, base));
			break;
		);
		break;
	);
}

int mob_sync_db(sqlite3 * pDb)
{
	SYNC_DATA sd;
	Block undo, redo;
	sqlite3_stmt *pStmt = NULL;

	blkInit(&undo);
	blkInit(&redo);

	QUERY_SQL_V(master_db, pStmt, ("SELECT num, uid, ptr_back, ptr_undo FROM works WHERE ptr_main = %ld;", (long)pDb),
		int wid = sqlite3_column_int(pStmt, 0);
		sqlite3 * pBackDb = (sqlite3 *)sqlite3_column_int64(pStmt, 2);
		sqlite3 * pUndoDb = (sqlite3 *)sqlite3_column_int64(pStmt, 3);
		sqlite3_stmt *pStmt2 = db_prepare(pDb, "SELECT name FROM main.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' UNION\n"
			"SELECT name FROM aux.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' ORDER BY name");

		strcpy(sd.uid, sqlite3_column_text(pStmt, 1));
		strcpy(sd.uid_p, sd.uid);

		while (SQLITE_ROW == sqlite3_step(pStmt2)){
			strcpy(sd.base, (const char*)sqlite3_column_text(pStmt2, 0));
			diff_one_table(pDb, "main", "aux", sd.base, &undo);

			if (undo.z){
				sd.snum_p = -1;
				QUERY_SQL_V(pUndoDb, pStmt2, ("SELECT uid, snum FROM works WHERE base = %Q ORDER BY num DESC LIMIT 1;", sd.base),
					strcpy(sd.uid_p, sqlite3_column_text(pStmt2, 0));
					sd.snum_p = sqlite3_column_int(pStmt2, 1);
					break;
				);

				sd.sn = 1;
				QUERY_SQL_V(pUndoDb, pStmt2, ("SELECT (MAX(sn) + 1) AS sn FROM works WHERE uid = %Q;", sd.uid), sd.sn = sqlite3_column_int(pStmt2, 0); break;);

				sd.snum = 1;
				QUERY_SQL_V(pUndoDb, pStmt2, ("SELECT (MAX(snum) + 1) AS sn FROM works WHERE uid = %Q AND base = %Q;", sd.uid, sd.base), sd.snum = sqlite3_column_int(pStmt2, 0); break;);

				diff_one_table(pDb, "aux", "main", sd.base, &redo);
				sqlite3_exec(pBackDb, redo.z, 0, 0, 0);

				EXECUTE_SQL_V(pUndoDb, ("INSERT INTO works (uid, sn, snum, base, undo, redo) VALUES (%Q, %d, %Q, %Q, %Q);", sd.uid, sd.sn, sd.snum, sd.base, undo.z, redo.z));

				alljoyn_send(wid, ACT_DATA, redo.z, redo.nUsed, (const char *)&sd, sizeof(SYNC_DATA));

				blkFree(&redo);
				blkFree(&undo);
			}
			sqlite3_finalize(pStmt2);

			break;
		);
	}

	blkFree(&redo);
	blkFree(&undo);

	return 0;
}

void mob_signal_db(unsigned int sid)
{
	SYNC_SIGNAL ss;
	sqlite3_stmt *pStmt = NULL;
	sqlite3_stmt *pStmt2 = NULL;

	QUERY_SQL_V(master_db, pStmt, ("SELECT ptr_main, uid FROM works WHERE num=%d", sid),
		strcpy(ss.uid, sqlite3_column_text(pStmt, 1));
		QUERY_SQL_V((sqlite3 *)sqlite3_column_int64(pStmt, 0), pStmt2, ("SELECT MAX(sn) AS n FROM works WHERE uid = %Q;", ss.uid),
			alljoyn_send(sid, ACT_SIGNAL, 0, 0, (const char *)&ss, sizeof(SYNC_SIGNAL));
			break;
		);
		break;
	);
}

int mob_close_db(sqlite3 * pDb)
{
	if (master_db) {
		sqlite3_stmt *pStmt = NULL;

		sqlite3_exec(pDb, "DETACH aux;", 0, 0, 0);

		QUERY_SQL_V(master_db, pStmt, ("SELECT ptr_back, ptr_undo FROM works WHERE ptr_main = %ld;", (long)pDb),
			_close_db((long)pDb, "bak", (sqlite3 *)sqlite3_column_int64(pStmt, 0));
			_close_db((long)pDb, "undo", (sqlite3 *)sqlite3_column_int64(pStmt, 1));
		);

		EXECUTE_SQL_V(master_db, ("DELETE FROM works WHERE ptr_main = %ld;", (long)pDb));

		int nFind = 0;

		QUERY_SQL(master_db, pStmt, "SELECT ptr_main FROM works",
			nFind = 1;
			break;
		);

		if (nFind == 0) {
			sqlite3_close(master_db);
			master_db = 0;
		}
	}

	return sqlite3_close(pDb);
}

int mob_find_parent_db(unsigned int sid, const char * uid, int snum, const char * base)
{
	return 0;
}

int mob_get_db(unsigned int sid, int num, const char * base, SYNC_DATA * pSD)
{
	return 0;
}
