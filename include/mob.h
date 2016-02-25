
#ifndef _MOB_H_
#define _MOB_H_

#include "sqlite3.h"

/*
** Make sure we can call this stuff from C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

	int mob_init(int argc, char** argv);
	int mob_open_db(const char *zFilename, sqlite3 **ppDb);
	int mob_sync_db(sqlite3 * pDb, const char * uid, int snum);
	int mob_apply(int wid, const char * uid, int snum, const char * sql);
	int mob_close_db(sqlite3 * pDb);
	void mob_exit(void);

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif /* _MOB_H_ */
