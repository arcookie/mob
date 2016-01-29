
#ifndef _MOB_H_
#define _MOB_H_

#include "sqlite3.h"

/*
** Make sure we can call this stuff from C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

	void mob_close_all();
	int mob_sync_db(sqlite3 * pDb);
	int mob_close_db(sqlite3 * pDb);
	int mob_open_db(const char *zFilename, sqlite3 **ppDb);

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif /* _MOB_H_ */