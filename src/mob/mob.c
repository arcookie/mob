
#include "mob.h"
#include <stdio.h>
#include "util.h"

#define QUERY_SQL(__db__, __stmt__, __sql__, ...)	\
	if (sqlite3_prepare_v2(__db__, __sql__, -1, &__stmt__, NULL) == SQLITE_OK) {while (sqlite3_step(__stmt__) == SQLITE_ROW) { __VA_ARGS__ } \
	sqlite3_finalize(__stmt__); __stmt__ = NULL;}

sqlite3 * master_db = 0;           /* The database */

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
		sqlite3_exec(master_db, "CREATE TABLE works (ptr_main BIGINT PRIMARY KEY, ptr_back BIGINT, ptr_undo BIGINT, ptr_redo BIGINT); PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
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
			sqlite3 *pRedoDb;

			if ((nRet = _create_db(id, "bak", &pBackDb)) == SQLITE_OK)
				sqlite3_exec(pBackDb, "PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
			else return nRet;

			if ((nRet = _create_db(id, "undo", &pUndoDb)) == SQLITE_OK) 
				sqlite3_exec(pUndoDb, "PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
			else return nRet;

			if ((nRet = _create_db(id, "redo", &pRedoDb)) == SQLITE_OK) 
				sqlite3_exec(pRedoDb, "PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
			else return nRet;

			sqlite3_exec(*ppDb, "PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);

			strInit(&sql);

			strPrintf(&sql, "INSERT INTO works VALUES (%ld, %ld, %ld, %ld);", (long)*ppDb, (long)pBackDb, (long)pUndoDb, (long)pRedoDb);
			sqlite3_exec(master_db, sql.z, 0, 0, 0);

			strFree(&sql);
		}

		return nRet;
	}
	return SQLITE_ERROR;
}

int mob_sync_db(sqlite3 * pDb)
{
	Str undo, redo;

	strInit(&undo);
	strInit(&redo);

	//get_diff(pDb, "", &redo, &undo);

	strFree(&redo);
	strFree(&undo);

	return 0;
}

int mob_close_db(sqlite3 * pDb)
{
	if (master_db) {
		Str sql;
		long id = (long)pDb;
		sqlite3_stmt *pStmt = NULL;

		strInit(&sql);

		strPrintf(&sql, "SELECT ptr_back, ptr_undo, ptr_redo FROM works WHERE ptr_main = %ld;", (long)pDb);

		QUERY_SQL(master_db, pStmt, sql.z,
			_close_db(id, "bak", (sqlite3 *)sqlite3_column_int64(pStmt, 0));
			_close_db(id, "undo", (sqlite3 *)sqlite3_column_int64(pStmt, 1));
			_close_db(id, "redo", (sqlite3 *)sqlite3_column_int64(pStmt, 2));
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

