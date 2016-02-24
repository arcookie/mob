/*
*   2016.2.25
*
*   Copyright arCookie. All rights reserved.
*
*   The license under which the Mob source code is released is the GPLv2 (or later) from the Free Software Foundation. 
*
*   A copy of the license is included with every copy of Mob source code, but you can also read the text of the license here(http://www.arcookie.com/?page_id=414).
*/

#include "mob.h"
#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "mob_alljoyn.h"

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
	Str fname;

	strInit(&fname);

	strPrintf(&fname, "%ld_%s.db3", id, mark);

	_unlink(fname.z);

	int rc = sqlite3_open(fname.z, ppDb);

	strFree(&fname);

	return rc;
}

static int _close_db(long id, const char * mark, sqlite3 *pDb)
{
	Str fname;
	int rc = sqlite3_close(pDb);

	strInit(&fname);

	strPrintf(&fname, "%ld_%s.db3", id, mark);

	_unlink(fname.z);

	strFree(&fname);

	return rc;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// global functions

int mob_apply(int wid, int uid, int snum, const char * sql)
{
	sqlite3_stmt *pStmt = NULL;
	// 여기서 누락체크하여 누락이 있으면 잠시후 다시 체크하여 전송 (missing 태이블)
	// 없으면 삭제 가능한 위치로 전송 
	// 누락이 없으면 순서대로 바로 소진(data 는 테이블에 남지 않는다.)

	QUERY_SQL_V(master_db, pStmt, ("SELECT ptr_main FROM works WHERE num=%d", wid),
		sqlite3_exec((sqlite3 *)sqlite3_column_int64(pStmt, 0), sql, 0, 0, 0);
		mob_sync_db((sqlite3 *)sqlite3_column_int64(pStmt, 0), 1, uid, snum);
		break;
	);
	return 0;
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
		sqlite3_exec(master_db, "CREATE TABLE works (num INTEGER PRIMARY KEY AUTOINCREMENT DEFAULT 1, uid INT, ptr_main BIGINT, ptr_back BIGINT, ptr_undo BIGINT); PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
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
				sqlite3_exec(pUndoDb, "CREATE TABLE works (num INTEGER PRIMARY KEY AUTOINCREMENT DEFAULT 1, uid INT, snum INT DEFAULT 1, undo TEXT, redo TEXT);PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
			else return nRet;

			EXECUTE_SQL_V(*ppDb, ("PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;ATTACH '%ld_bak.db3' as aux;", id));
			EXECUTE_SQL_V(master_db, ("INSERT INTO works (num, uid, ptr_main, ptr_back, ptr_undo) VALUES (%d, %d, %ld, %ld, %ld);", alljoyn_doc_id(), alljoyn_user_id(), (long)*ppDb, (long)pBackDb, (long)pUndoDb));
		}

		return nRet;
	}
	return SQLITE_ERROR;
}

int mob_sync_db(sqlite3 * pDb, int end, int uid, int snum)
{
	int done = 0;
	Str base, undo, redo, bak;
	sqlite3_stmt *pStmt = NULL;
	sqlite3_stmt *pStmt2 = NULL;

	strInit(&undo);
	strInit(&redo);
	strInit(&bak);
	strInit(&base);

	strPrintf(&bak, "%ld_bak.db3", (long)pDb);

	get_diff(pDb, bak.z, &base, &redo, &undo);

	if (redo.z){
		int wid = -1;
		sqlite3 *pUndoDb = 0;

		QUERY_SQL_V(master_db, pStmt, ("SELECT num, ptr_back, ptr_undo FROM works WHERE ptr_main = %ld;", (long)pDb),
			wid = sqlite3_column_int(pStmt, 0);
			sqlite3_exec((sqlite3 *)sqlite3_column_int64(pStmt, 1), redo.z, 0, 0, 0);

			pUndoDb = (sqlite3 *)sqlite3_column_int64(pStmt, 2);

			if (!end) {
				uid = alljoyn_user_id();
				QUERY_SQL_V(pUndoDb, pStmt2, ("SELECT MAX(snum) AS sn FROM works WHERE uid = %d;", uid),
					snum = sqlite3_column_int(pStmt, 0) + 1;
					break;
				);
			}
			EXECUTE_SQL_V(pUndoDb, ("INSERT INTO works (uid, snum, base, undo, redo) VALUES (%d, %d, %Q, %Q, %Q);", uid, snum, base, undo.z, redo.z));
			break;
		);

		if (!end && wid > 0) {
			strPrintf(&redo, "/*|%d|%d|%s|*/", uid, snum, base.z);
			alljoyn_send(wid, redo.z, redo.nUsed);
		}
	}

	strFree(&redo);
	strFree(&undo);
	strFree(&base);
	strFree(&bak);

	return 0;
}

int mob_close_db(sqlite3 * pDb)
{
	if (master_db) {
		long id = (long)pDb;
		sqlite3_stmt *pStmt = NULL;

		sqlite3_exec(pDb, "DETACH aux;", 0, 0, 0);

		QUERY_SQL_V(master_db, pStmt, ("SELECT ptr_back, ptr_undo FROM works WHERE ptr_main = %ld;", (long)pDb),
			_close_db(id, "bak", (sqlite3 *)sqlite3_column_int64(pStmt, 0));
			_close_db(id, "undo", (sqlite3 *)sqlite3_column_int64(pStmt, 1));
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
