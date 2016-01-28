
#include "mob.h"

sqlite3 * master_db = 0;           /* The database */

int mob_open_db(const char *zFilename, sqlite3 **ppDb)
{
	return sqlite3_open(zFilename, ppDb);
}

int mob_sync_db(sqlite3 * pDb)
{
	return 0;
}

int mob_close_db(sqlite3 * pDb)
{
	return sqlite3_close(pDb);
}

void mob_close_all()
{
	if (master_db) {

	}
}
