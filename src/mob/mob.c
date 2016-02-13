
#include "mob.h"
#include <stdio.h>
#include "util.h"
#include "mob_alljoyn.h"

#define QUERY_SQL(__db__, __stmt__, __sql__, ...)	\
	if (sqlite3_prepare_v2(__db__, __sql__, -1, &__stmt__, NULL) == SQLITE_OK) {while (sqlite3_step(__stmt__) == SQLITE_ROW) { __VA_ARGS__ } \
	sqlite3_finalize(__stmt__); __stmt__ = NULL;}

sqlite3 * master_db = 0;           /* The database */

/*

* PKT_HEADER

int foot_print[16];
int req;
int wid;  // doc id
int uid;  // 보내는이의 user id
int snum;
int fid;  // 파일인 경우 파일 id > 0 아니면 0
int chain; // 0 이면 시그널.
Str base;  // 대상이 되는 테이블(들)

* data packet

int chain;
int length; // 양수이면 중간 음수이면 끝.
char * data;

의 처리 결과 아래 핸들러로 들어감.

int SendHandler(int nDocID, int nUID, const char * sText, int nLength)
{
로그인일때 nUID == 0 , sText 는 패스워드 가능하면 

누적된 DB 를 정상 전송

아니면 에러 시그널

}

보낼때 
Send(int nDocID, int nUID, const char * sText, int nLength)
이면 file 들에 대한 처리후 본 데이터 처리.

alljoyn_send(const char * sText)
에서 

uid 이 0 이면 로그인

fid, chain 이 0 이면 시그널

로그인 에러이면 접속 종료후 term

로그인 성공이면 협업자 접속 리스트 갱신 전파 시그널

개설자 로그아웃 시그널이면 접속 종료후 term

협업자 로그아웃 시그널이면 맴버 종료후 동기화

개설자는 시작시 그동안의 묶음을 주고 시간이 뒤바뀐 다른 참여자의 데이터의 경우 누락복구로 수신.

그러므로 참여자는 예외처리없는 참여가 허용된다.

redo 의 키로 저장 (redo 에 uid, snum 기록)

redo 에 반영되기 직전 부모가 같으면 언두후 리두처리(redo 테이블 삽입후 처리)

도착한 데이터들은 처리를 위한 큐 테이블에 쌓인다.

member table, apply table 존재.

*/

int SendHandler(const char * sText, int nLength)
{
	sqlite3_stmt *pStmt = NULL;

	QUERY_SQL(master_db, pStmt, "SELECT ptr_main FROM works",
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
		sqlite3_exec(master_db, "CREATE TABLE works (num INTEGER PRIMARY KEY AUTOINCREMENT DEFAULT 1, ptr_main BIGINT, ptr_back BIGINT, ptr_undo BIGINT); PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
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

			strPrintf(&sql, "INSERT INTO works (ptr_main, ptr_back, ptr_undo) VALUES (%ld, %ld, %ld);", (long)*ppDb, (long)pBackDb, (long)pUndoDb);
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
	Str base, undo, redo, bak, sql;
	sqlite3_stmt *pStmt = NULL;

	strInit(&undo);
	strInit(&redo);
	strInit(&bak);
	strInit(&base);
	strInit(&sql);

	strPrintf(&bak, "%ld_bak.db3", (long)pDb);

	get_diff(pDb, bak.z, &base, &redo, &undo);

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
	strFree(&base);
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

