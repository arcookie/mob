/*
*   2016.2.25
*
*   Copyright arCookie. All rights reserved.
*
*   The license under which the Mob source code is released is the GPLv2 (or later) from the Free Software Foundation.
*
*   A copy of the license is included with every copy of Mob source code, but you can also read the text of the license here(http://www.arcookie.com/?page_id=414).
*
*/

#ifndef _MOB_H_
#define _MOB_H_

#include "sqlite3.h"
#include "block.h"

#define NAME_PREFIX	"org.alljoyn.bus.arcookie.mob."

/*
** Make sure we can call this stuff from C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

	extern sqlite3_stmt *db_prepare(sqlite3 * pDB, const char *zFormat, ...);
	extern void diff_one_table(sqlite3 * pDB, const char *zMain, const char *zAux, const char *zTab, Block *out);

	/* alljoyn related functions */
	extern sqlite3 * alljoyn_open_db(const char *zFilename);
	extern void alljoyn_close_db(sqlite3 * pDb);

	extern void alljoyn_disconnect(void);
	extern int alljoyn_connect(const char * advertisedName, const char * joinName);

	/* mob functions */
	extern int mob_sync_db(sqlite3 * pDb);

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif /* _MOB_H_ */
