
#ifndef _MOB_H_
#define _MOB_H_

#include "sqlite3.h"

#define NAME_PREFIX	"org.alljoyn.bus.arcookie.mob."

/*
** Make sure we can call this stuff from C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

	void alljoyn_disconnect(void);
	int alljoyn_connect(const char * advertisedName, const char * joinName);

	int alljoyn_send(int nDocID, char * sText, int nLength);

	int alljoyn_session_id();
	const char * alljoyn_join_name();

	int mob_init(int argc, char** argv);
	int mob_open_db(const char *zFilename, sqlite3 **ppDb);
	int mob_sync_db(sqlite3 * pDb, const char * uid, int snum);
	int mob_apply(int wid, const char * uid, int snum, const char * sql);
	int mob_close_db(sqlite3 * pDb);
	void mob_exit(void);

	typedef struct Str Str;

	struct Str {
		char *z;        /* Text of the string */
		int nAlloc;     /* Bytes allocated in z[] */
		int nUsed;      /* Bytes actually used in z[] */
	};

	extern void strInit(Str *p);
	extern void strFree(Str *p);
	extern int strPrintf(Str *p, const char *zFormat, ...);

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif /* _MOB_H_ */
