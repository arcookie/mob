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

#include "mob.h"
#include "Global.h"
#include <alljoyn/Init.h>
#include "MobClient.h"
#include "MobServer.h"


/** Take input from stdin and send it as a mob message, continue until an error or
* SIGINT occurs, return the result status. */
int alljoyn_connect(const char * advertisedName, const char * joinName)
{
	gWPath = GetVirtualStorePath();

	CreateDirectory(gWPath.data(), FALSE);

	gMutex = CreateMutex(NULL, TRUE, "PreverntSecondInstanceOfMob");
	if (GetLastError() != ERROR_ALREADY_EXISTS) remove_dir(gWPath);

	QStatus status = AllJoynInit();

#ifdef ROUTER
	if (ER_OK == status) {
		status = AllJoynRouterInit();
		if (ER_OK != status) {
			AllJoynShutdown();
		}
	}
#endif

	if (advertisedName) {
		gpMob = new CMobServer();
		return gpMob->Init(advertisedName);
	}
	else {
		gpMob = new CMobClient();
		return gpMob->Init(joinName);
	}
}

void alljoyn_disconnect(void)
{
	if (gpMob) {
		delete gpMob;
		gpMob = NULL;
	}

#ifdef ROUTER
	AllJoynRouterShutdown();
#endif
	AllJoynShutdown();

	if (gMutex) {
		ReleaseMutex(gMutex);
		CloseHandle(gMutex);
	}
}

sqlite3 * alljoyn_open_db(const char *zFilename)
{
	return gpMob->OpenDB(zFilename);
}

void alljoyn_close_db(sqlite3 * pDb)
{
	gpMob->CloseDB();
}

int mob_sync_db(sqlite3 * pDb)
{
	SYNC_DATA sd;
	Block undo, redo;

	blkInit(&undo);
	blkInit(&redo);

	int wid = gpMob->GetSessionID();
	sqlite3 * pBackDb = gpMob->GetBackDB();
	sqlite3 * pUndoDb = gpMob->GetUndoDB();
	sqlite3_stmt *pStmt = db_prepare(pDb, "SELECT name FROM main.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' UNION\n"
		"SELECT name FROM aux.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' ORDER BY name");

	strcpy_s(sd.uid, sizeof(sd.uid), gpMob->GetJoinName());
	strcpy_s(sd.uid_p, sizeof(sd.uid_p), sd.uid);

	while (SQLITE_ROW == sqlite3_step(pStmt)){
		strcpy_s(sd.base, sizeof(sd.base), (const char*)sqlite3_column_text(pStmt, 0));
		diff_one_table(pDb, "main", "aux", sd.base, &undo);

		if (undo.z){
			sd.snum_p = -1;
			QUERY_SQL_V(pUndoDb, pStmt, ("SELECT uid, snum FROM works WHERE base = %Q ORDER BY num DESC LIMIT 1;", sd.base),
				strcpy_s(sd.uid_p, sizeof(sd.uid_p), (const char *)sqlite3_column_text(pStmt, 0));
			sd.snum_p = sqlite3_column_int(pStmt, 1);
			break;
			);

			sd.sn = 1;
			QUERY_SQL_V(pUndoDb, pStmt, ("SELECT (MAX(sn) + 1) AS sn FROM works WHERE uid = %Q;", sd.uid), sd.sn = sqlite3_column_int(pStmt, 0); break;);

			sd.snum = 1;
			QUERY_SQL_V(pUndoDb, pStmt, ("SELECT (MAX(snum) + 1) AS sn FROM works WHERE uid = %Q AND base = %Q;", sd.uid, sd.base), sd.snum = sqlite3_column_int(pStmt, 0); break;);

			diff_one_table(pDb, "aux", "main", sd.base, &redo);
			sqlite3_exec(pBackDb, redo.z, 0, 0, 0);

			EXECUTE_SQL_V(pUndoDb, ("INSERT INTO works (uid, sn, snum, base, undo, redo) VALUES (%Q, %d, %Q, %Q, %Q);", sd.uid, sd.sn, sd.snum, sd.base, undo.z, redo.z));

			alljoyn_send(wid, NULL, ACT_DATA, redo.z, redo.nUsed, (const char *)&sd, sizeof(SYNC_DATA));

			blkFree(&redo);
			blkFree(&undo);
		}
	}
	sqlite3_finalize(pStmt);

	blkFree(&redo);
	blkFree(&undo);

	return 0;
}

