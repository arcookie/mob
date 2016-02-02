
#include "mob.h"
#include "util.h"

sqlite3 * master_db = 0;           /* The database */

int mob_open_db(const char *zFilename, sqlite3 **ppDb)
{
	if (!master_db && sqlite3_open(":memory:", &master_db) == SQLITE_OK)
		sqlite3_exec(master_db, "CREATE TABLE works (ptr_main BIGINT PRIMARY KEY); PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
	else {
		master_db = 0;
		return SQLITE_ERROR;
	}

	if (master_db) {
		int nRet = sqlite3_open(zFilename, ppDb);

		if (nRet == SQLITE_OK) {
			Str sql;

			sqlite3_exec(*ppDb, "PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;ATTACH ':memory:' as buffer;", 0, 0, 0);

			strInit(&sql);

			strPrintf(&sql, "INSERT INTO works VALUES (%ld);", (long)*ppDb);
			sqlite3_exec(master_db, sql.z, 0, 0, 0);

			strFree(&sql);
		}

		return nRet;
	}
	return SQLITE_ERROR;
}

int mob_sync_db(sqlite3 * pDb)
{
	return 0;
}

int mob_close_db(sqlite3 * pDb)
{
	if (master_db) {
		Str sql;
		char ** retStrings = 0;
		int iRows = 0, iCols = 0;

		strInit(&sql);

		strPrintf(&sql, "DELETE FROM documents WHERE ptr_main = %ld;", (long)pDb);
		sqlite3_exec(master_db, sql.z, 0, 0, 0);

		strFree(&sql);

		if (!(sqlite3_get_table(master_db, "SELECT ptr_main FROM documents", &retStrings, &iRows, &iCols, 0) == SQLITE_OK && iRows <= 1 && iCols > 0)) {
			sqlite3_close(master_db);
			master_db = 0;
		}
		sqlite3_free_table(retStrings);
	}

	return sqlite3_close(pDb);
}

