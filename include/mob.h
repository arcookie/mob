
#ifndef _MOB_H_
#define _MOB_H_

#include "sqlite3.h"

#define ACT_DATA		0
#define ACT_FLIST		1
#define ACT_FLIST_REQ	2
#define ACT_FILE		3
#define ACT_MISSING		4
#define ACT_SIGNAL		5
#define ACT_NO_MISSED	6
#define ACT_END			7

#define NAME_PREFIX	"org.alljoyn.bus.arcookie.mob."

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// defines

#define QUERY_SQL(__db__, __stmt__, __sql__, ...)	\
if (sqlite3_prepare_v2(__db__, __sql__, -1, &__stmt__, NULL) == SQLITE_OK) {\
while (sqlite3_step(__stmt__) == SQLITE_ROW) { __VA_ARGS__ } sqlite3_finalize(__stmt__); __stmt__ = NULL; }

#define QUERY_SQL_V(__db__, __stmt__, __sql__, ...)	\
	{ char * __zSQL__ = sqlite3_mprintf __sql__; if (__zSQL__ && sqlite3_prepare_v2(__db__, __zSQL__, -1, &__stmt__, NULL) == SQLITE_OK) {\
while (sqlite3_step(__stmt__) == SQLITE_ROW) { __VA_ARGS__ } sqlite3_finalize(__stmt__); __stmt__ = NULL; }	sqlite3_free(__zSQL__);	}

#define EXECUTE_SQL_V(__db__, __sql__, ...)	\
	{ char * __zSQL__ = sqlite3_mprintf __sql__; if (__zSQL__) { sqlite3_exec(__db__, __zSQL__, 0, 0, 0); sqlite3_free(__zSQL__); }}

/*
** Make sure we can call this stuff from C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

	/* memory block utility functions */
	 
	typedef struct {
		char *z;        /* Text of the string */
		int nAlloc;     /* Bytes allocated in z[] */
		int nUsed;      /* Bytes actually used in z[] */
	} Block;

	typedef struct {
		int sn;
		int snum;
		int snum_p;
		char uid[16];
		char uid_p[16];
		char base[64];
	} SYNC_DATA;

	typedef struct {
		int sn;
		char uid[16];
	} SYNC_SIGNAL;

	extern void blkInit(Block *p);
	extern void blkFree(Block *p);
	extern const char * get_writable_path();
	extern int memCat(Block *p, const char * z, int n);
	extern int strPrintf(Block *p, const char *zFormat, ...);
	sqlite3_stmt *db_prepare(sqlite3 * pDB, const char *zFormat, ...);
	void diff_one_table(sqlite3 * pDB, const char *zMain, const char *zAux, const char *zTab, Block *out);

	/* alljoyn related functions */

	sqlite3 * alljoyn_open_db(const char *zFilename);
	void alljoyn_close_db(sqlite3 * pDb);

	void alljoyn_disconnect(void);
	int alljoyn_connect(const char * advertisedName, const char * joinName);

	int alljoyn_send(unsigned int nSID, const char * pJoiner, int nAction, char * sText, int nLength, const char * pExtra, int nExtLen);
	//void mob_send_missed_db(unsigned int sid, const char * uid_to, const char * having);

	int alljoyn_session_id();
	const char * alljoyn_join_name();

	/* mob functions */

	int mob_open_db(const char *zFilename, sqlite3 **ppDb);
	int mob_sync_db(sqlite3 * pDb);
	//void mob_apply_db(unsigned int sid, const char * uid, int sn, int snum, const char * base, const char * sql);
	//void mob_undo_db(unsigned int sid, const char * uid, int snum, const char * base);
//	int mob_close_db(sqlite3 * pDb);
//	void mob_no_missed_db(unsigned int sid, const char * snum);
	//void mob_signal_db(unsigned int sid);
	extern int mob_find_parent_db(unsigned int sid, const char * uid, int snum, const char * base);
	extern int mob_get_db(unsigned int sid, int num, const char * base, SYNC_DATA * pSD);


#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif /* _MOB_H_ */
