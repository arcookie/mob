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
#include "dbutil.h"
#include "Global.h"
#include <alljoyn/Init.h>
#include "MobClient.h"
#include "MobServer.h"


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// export functions

void mob_init(int nIsSvr, const char * sSvrName)
{
	gWPath = get_writable_path();

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

	if (nIsSvr == 1) gpMob = new CMobServer(sSvrName);
	else gpMob = new CMobClient(sSvrName);
}

int mob_connect()
{
	return gpMob->Connect();
}

void mob_disconnect(void)
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

sqlite3 * mob_open_db(const char * sPath)
{
	return gpMob->OpenDB(sPath);
}

void mob_close_db(sqlite3 * pDb)
{
	gpMob->CloseDB();
}

void CALLBACK fnSendSignal(HWND /*hwnd*/, UINT /*uMsg*/, UINT idEvent, DWORD /*dwTime*/)
{
	if (!gpMob->SendSignal()) KillTimer(NULL, idEvent);
}

int mob_sync_db(sqlite3 * pDb)
{
	SYNC_DATA sd;
	Block undo, redo;

	blkInit(&undo);
	blkInit(&redo);

	int session_id = gpMob->GetSessionID();
	sqlite3 * pBackDb = gpMob->GetBackDB();
	sqlite3 * pUndoDb = gpMob->GetUndoDB();
	std::vector<qcc::String> tables;
	std::vector<qcc::String> sqls;
	std::vector<qcc::String>::const_iterator iter;
	sqlite3_stmt *pStmt = db_prepare(pDb, "SELECT name FROM main.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' UNION\n"
		"SELECT name FROM aux.sqlite_master WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%' ORDER BY name");

	while (SQLITE_ROW == sqlite3_step(pStmt)) tables.push_back((const char*)sqlite3_column_text(pStmt, 0));

	sqlite3_finalize(pStmt);

	strcpy_s(sd.joiner, sizeof(sd.joiner), gpMob->GetJoinName());
	memset(sd.joiner_prev, 0, sizeof(sd.joiner_prev));

	for (iter = tables.begin(); iter != tables.end(); iter++) {
		strcpy_s(sd.base_table, sizeof(sd.base_table), (*iter).data());
		diff_one_table(pDb, "main", "aux", sd.base_table, &undo);

		if (undo.z){
			sd.snum_prev = -1;
			QUERY_SQL_V(pUndoDb, pStmt, ("SELECT joiner, snum FROM works WHERE base_table = %Q ORDER BY num DESC LIMIT 1;", sd.base_table),
				strcpy_s(sd.joiner_prev, sizeof(sd.joiner_prev), (const char *)sqlite3_column_text(pStmt, 0));
				sd.snum_prev = sqlite3_column_int(pStmt, 1);
				break;
			);

			sd.snum = 1;
			QUERY_SQL_V(pUndoDb, pStmt, ("SELECT (MAX(snum) + 1) AS sn FROM works WHERE joiner = %Q;", sd.joiner), 
				int n = sqlite3_column_int(pStmt, 0);
				if (n > 0) sd.snum = n;
				break;
			);

			diff_one_table(pDb, "aux", "main", (*iter).data(), &redo);

			sqlite3_exec(pBackDb, redo.z, 0, 0, 0);

			EXECUTE_SQL_V(pUndoDb, ("INSERT INTO works (joiner, snum, base_table, undo, redo) VALUES (%Q, %d, %Q, %Q, %Q);", sd.joiner, sd.snum, (*iter).data(), undo.z, redo.z));

			if (gpMob->IsConnected()) {
				alljoyn_send(session_id, NULL, ACT_DATA, redo.z, redo.nUsed + 1, (const char *)&sd, sizeof(SYNC_DATA));

				gpMob->SetSignal(NULL, true);
				SetTimer(NULL, TM_SEND_SIGNAL, INT_SEND_SIGNAL, (TIMERPROC)&fnSendSignal);
			}
			blkFree(&redo);
			blkFree(&undo);
		}
	}

	blkFree(&redo);
	blkFree(&undo);

	return 0;
}

void mob_receive_proc(MobReceiveProc fn)
{
	fnReceiveProc = fn;
}
