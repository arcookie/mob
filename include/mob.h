
#ifndef _MOB_H_
#define _MOB_H_

#include "sqlite3.h"

/*
** Make sure we can call this stuff from C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

	void mob_exit(void);
	int mob_close_db(sqlite3 * pDb);
	int mob_init(int argc, char** argv);
	int mob_sync_db(sqlite3 * pDb, int send, int uid, int snum);
	int mob_open_db(const char *zFilename, sqlite3 **ppDb);
	int mob_apply(int wid, int uid, int snum, const char * sql);

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif /* _MOB_H_ */
