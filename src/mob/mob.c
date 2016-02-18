
#include "mob.h"
#include <stdio.h>
#include "util.h"
#include "mob_alljoyn.h"

#define QUERY_SQL(__db__, __stmt__, __sql__, ...)	\
	if (sqlite3_prepare_v2(__db__, __sql__, -1, &__stmt__, NULL) == SQLITE_OK) {\
while (sqlite3_step(__stmt__) == SQLITE_ROW) { __VA_ARGS__ } sqlite3_finalize(__stmt__); __stmt__ = NULL;}

#define QUERY_SQL_V(__db__, __stmt__, __sql__, ...)	\
	{ char * __zSQL__ = sqlite3_mprintf __sql__; if (__zSQL__ && sqlite3_prepare_v2(__db__, __zSQL__, -1, &__stmt__, NULL) == SQLITE_OK) {\
	while (sqlite3_step(__stmt__) == SQLITE_ROW) { __VA_ARGS__ } sqlite3_finalize(__stmt__); __stmt__ = NULL;}	sqlite3_free(__zSQL__);	}

#define EXECUTE_SQL_V(__db__, __sql__, ...)	\
	{ char * __zSQL__ = sqlite3_mprintf __sql__; if (__zSQL__) { sqlite3_exec(__db__, __zSQL__, 0, 0, 0); sqlite3_free(__zSQL__); }}


sqlite3 * master_db = 0;           /* The database */


int SendHandler(int nDocID, const char * sText, int nLength)
{
	sqlite3_stmt *pStmt = NULL;
	int uid_snum;
	int snum;
	char * base; // get from sText /*| ... |*/
	// 여기서 누락체크하여 누락이 있으면 잠시후 다시 체크하여 전송 (missing 태이블)
	// 없으면 삭제 가능한 위치로 전송 
	// 누락이 없으면 순서대로 바로 소진(data 는 테이블에 남지 않는다.)

	QUERY_SQL_V(master_db, pStmt, ("SELECT ptr_main FROM works WHERE num=%d", alljoyn_doc_id()),
		sqlite3_exec((sqlite3 *)sqlite3_column_int64(pStmt, 0), sText, 0, 0, 0);
		mob_sync_db((sqlite3 *)sqlite3_column_int64(pStmt, 0), 0);
		break;
	);
	return 0;
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
		sqlite3_exec(master_db, "CREATE TABLE works (num INTEGER PRIMARY KEY AUTOINCREMENT DEFAULT 1, ptr_main BIGINT, ptr_back BIGINT, ptr_undo BIGINT, ptr_mob BIGINT, uid INTEGER, uid_snum INT DEFAULT 0, snum INT DEFAULT 1); PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
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
			sqlite3 *pMobDb;

			if ((nRet = _create_db(id, "bak", &pBackDb)) == SQLITE_OK)
				sqlite3_exec(pBackDb, "PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
			else return nRet;

			if ((nRet = _create_db(id, "undo", &pUndoDb)) == SQLITE_OK) 
				sqlite3_exec(pUndoDb, "CREATE TABLE works (num INTEGER PRIMARY KEY AUTOINCREMENT DEFAULT 1, undo TEXT, redo TEXT);PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
			else return nRet;

			if ((nRet = sqlite3_open(":memory:", &pMobDb)) == SQLITE_OK) {
				sqlite3_exec(pMobDb, 
					"CREATE TABLE member (num INTEGER PRIMARY KEY AUTOINCREMENT DEFAULT 1, key CHAR(32), pwd CHAR(32), connected BOOL DEFAULT 0);"
					"CREATE TABLE received (uid INTEGER, snum INTEGER, region VARCHAR(1024), data TEXT, CONSTRAINT[] PRIMARY KEY(uid, snum));"
					"CREATE TABLE files (num INTEGER PRIMARY KEY AUTOINCREMENT DEFAULT 0, sent BOOL default 0, mtime TIMESTAMP, size INT64, uri VARCHAR(1024), path VARCHAR(1024));"
					"PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
				if (alljoyn_is_server() == 1) sqlite3_exec(pMobDb, "INSERT INTO member (pwd) VALUES ('-');INSERT INTO member (pwd) VALUES ('12345678');", 0, 0, 0);
			}
			else return nRet;

			EXECUTE_SQL_V(*ppDb, ("PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;ATTACH '%ld_bak.db3' as aux;", id));
			EXECUTE_SQL_V(master_db, ("INSERT INTO works (num, ptr_main, ptr_back, ptr_undo, ptr_mob, uid, uid_snum) VALUES (%d, %ld, %ld, %ld, %ld, %d, %d);", alljoyn_doc_id(), (long)*ppDb, (long)pBackDb, (long)pUndoDb, (long)pMobDb, alljoyn_user_id(), alljoyn_user_id()));
		}

		return nRet;
	}
	return SQLITE_ERROR;
}

int mob_sync_db(sqlite3 * pDb, int send)
{
	int done = 0;
	Str base, undo, redo, bak;
	sqlite3_stmt *pStmt = NULL;

	strInit(&undo);
	strInit(&redo);
	strInit(&bak);
	strInit(&base);

	strPrintf(&bak, "%ld_bak.db3", (long)pDb);

	get_diff(pDb, bak.z, &base, &redo, &undo);

	if (redo.z){
		int wid = -1;
		int uid_snum, snum;
		sqlite3 *pMobDb = 0;

		QUERY_SQL_V(master_db, pStmt, ("SELECT num, ptr_back, ptr_undo, ptr_mob, uid_snum, snum FROM works WHERE ptr_main = %ld;", (long)pDb),
			wid = sqlite3_column_int(pStmt, 0);
			sqlite3_exec((sqlite3 *)sqlite3_column_int64(pStmt, 1), redo.z, 0, 0, 0);

			EXECUTE_SQL_V((sqlite3 *)sqlite3_column_int64(pStmt, 2), ("INSERT INTO works (undo, redo) VALUES (%Q, %Q);", undo.z, redo.z));

			pMobDb = (sqlite3 *)sqlite3_column_int64(pStmt, 3);
			uid_snum = sqlite3_column_int(pStmt, 4);
			snum = sqlite3_column_int(pStmt, 5);

			EXECUTE_SQL_V(master_db, ("UPDATE works SET uid_snum=uid, snum=snum+1 WHERE ptr_main = %ld;", (long)pDb));
			break;
		);

		if (pMobDb && send && wid > 0) {
			strPrintf(&redo, "/*|%d|%d|%s|*/", uid_snum, snum, base.z);
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

/*

len = p2 - p;
if (len < 1024)	{
memcpy(fi.uri, p, len);
fi.mtime = get_file_mtime(p);
fi.fsize = get_file_length(p);

add2mem(data, &fi, sizeof(FILE_SEND_ITEM));
total += sizeof(FILE_SEND_ITEM);
}

QUERY_SQL_V(pMobDb, pStmt, ("SELECT mtime, size FROM files WHERE path=%Q AND sent=1", path),
if (mtime == sqlite3_column_int(pStmt, 0) && fsize == sqlite3_column_int64(pStmt, 1)) skip = 1;
break;
);

if (skip == 0) EXECUTE_SQL_V(pMobDb, ("INSERT INTO files (uri, path, mtime, size) VALUES (%Q, %Q, %d, %ld)", p, path, mtime, fsize));
*/


/*

if (total > 0) alljoyn_send(ACT_FLIST, wid, data, total);

char ** table = 0;
int rows = 0, cols = 0;

if (sqlite3_get_table(pMobDb, "SELECT uri, mtime, size FROM files WHERE sent=0", &table, &rows, &cols, 0) == SQLITE_OK) {
int r = 0, c;

while (++r <= rows) {
c = -1;
while (++c < cols) {
//s = table[r * cols + c];
}
}

alljoyn_send(ACT_FLIST, wid, table, redo.nUsed);
sqlite3_exec(pMobDb, "UPDATE sent=1 WHERE sent=0", 0, 0, 0);
}

if (table) sqlite3_free_table(table);
*/
