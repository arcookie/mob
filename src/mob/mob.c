
#include "mob.h"
#include <stdio.h>
#include "util.h"
#include "mob_alljoyn.h"

#define QUERY_SQL(__db__, __stmt__, __sql__, ...)	\
	if (sqlite3_prepare_v2(__db__, __sql__, -1, &__stmt__, NULL) == SQLITE_OK) {while (sqlite3_step(__stmt__) == SQLITE_ROW) { __VA_ARGS__ } \
	sqlite3_finalize(__stmt__); __stmt__ = NULL;}

sqlite3 * master_db = 0;           /* The database */

int SendHandler(const char * sText)
{
	sqlite3_stmt *pStmt = NULL;

	QUERY_SQL(master_db, pStmt, "SELECT ptr_main FROM works",
		sqlite3_exec((sqlite3 *)sqlite3_column_int64(pStmt, 0), sText, 0, 0, 0);
		mob_sync_db((sqlite3 *)sqlite3_column_int64(pStmt, 0), 0);
		break;
	);
}

int mob_init(int argc, char** argv)
{
	alljoyn_set_handler(&SendHandler);

	return alljoyn_connect(argc, argv);
}

void mob_exit(void)
{
	alljoyn_disconnect();
}

static int _create_db(long id, const char * mark, sqlite3 **ppDb)
{
	Str fname;

	strInit(&fname);

	strPrintf(&fname, "%ld_%s.db3", id, mark);

	unlink(fname.z);

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

	unlink(fname.z);

	strFree(&fname);

	return rc;
}

int mob_open_db(const char *zFilename, sqlite3 **ppDb)
{
	if (!master_db && sqlite3_open(":memory:", &master_db) == SQLITE_OK)
		sqlite3_exec(master_db, "CREATE TABLE works (ptr_main BIGINT PRIMARY KEY, ptr_back BIGINT, ptr_undo BIGINT); PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
	else {
		master_db = 0;
		return SQLITE_ERROR;
	}

	if (master_db) {
		int nRet = sqlite3_open(zFilename, ppDb);

		if (nRet == SQLITE_OK) {
			Str sql;
			long id = (long)*ppDb;
			sqlite3 *pBackDb;
			sqlite3 *pUndoDb;

			if ((nRet = _create_db(id, "bak", &pBackDb)) == SQLITE_OK)
				sqlite3_exec(pBackDb, "PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
			else return nRet;

			if ((nRet = _create_db(id, "undo", &pUndoDb)) == SQLITE_OK) 
				sqlite3_exec(pUndoDb, "CREATE TABLE works (num INTEGER PRIMARY KEY AUTOINCREMENT DEFAULT 1, undo TEXT, redo TEXT);PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
			else return nRet;

			sqlite3_exec(*ppDb, sqlite3_mprintf("PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;ATTACH '%ld_bak.db3' as aux;", id), 0, 0, 0);

			strInit(&sql);

			strPrintf(&sql, "INSERT INTO works VALUES (%ld, %ld, %ld);", (long)*ppDb, (long)pBackDb, (long)pUndoDb);
			sqlite3_exec(master_db, sql.z, 0, 0, 0);

			strFree(&sql);
		}

		return nRet;
	}
	return SQLITE_ERROR;
}

int mob_sync_db(sqlite3 * pDb, int send)
{
	int done = 0;
	Str undo, redo, bak, sql;
	sqlite3_stmt *pStmt = NULL;

	strInit(&undo);
	strInit(&redo);
	strInit(&bak);
	strInit(&sql);

	strPrintf(&bak, "%ld_bak.db3", (long)pDb);

	get_diff(pDb, bak.z, &redo, &undo);

	if (redo.z){

		strPrintf(&sql, "SELECT ptr_back, ptr_undo FROM works WHERE ptr_main = %ld;", (long)pDb);

		QUERY_SQL(master_db, pStmt, sql.z,
			sqlite3_exec((sqlite3 *)sqlite3_column_int64(pStmt, 0), redo.z, 0, 0, 0);

		strFree(&sql);
		strPrintf(&sql, "INSERT INTO works (undo, redo) VALUES (%Q, %Q);", undo.z, redo.z);

		sqlite3_exec((sqlite3 *)sqlite3_column_int64(pStmt, 1), sql.z, 0, 0, 0);
		break;
		);

		if (send) alljoyn_send(redo.z);
	}

	strFree(&sql);
	strFree(&redo);
	strFree(&undo);
	strFree(&bak);

	return 0;
}

int mob_close_db(sqlite3 * pDb)
{
	if (master_db) {
		Str sql;
		long id = (long)pDb;
		sqlite3_stmt *pStmt = NULL;

		strInit(&sql);

		strPrintf(&sql, "SELECT ptr_back, ptr_undo FROM works WHERE ptr_main = %ld;", (long)pDb);

		sqlite3_exec(pDb, "DETACH aux;", 0, 0, 0);

		QUERY_SQL(master_db, pStmt, sql.z,
			_close_db(id, "bak", (sqlite3 *)sqlite3_column_int64(pStmt, 0));
			_close_db(id, "undo", (sqlite3 *)sqlite3_column_int64(pStmt, 1));
		);

		strFree(&sql);

		strPrintf(&sql, "DELETE FROM works WHERE ptr_main = %ld;", (long)pDb);
		sqlite3_exec(master_db, sql.z, 0, 0, 0);

		strFree(&sql);

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

