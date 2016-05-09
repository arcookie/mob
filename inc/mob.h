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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// defines

#define NAME_PREFIX	"org.alljoyn.bus.arcookie.mob."

typedef void(*MobReceiveProc)(sqlite3 * pDb);

/*
** Make sure we can call this stuff from C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// export functions

	extern void mob_init(int nIsSvr, const char * sSvrName);
	extern int mob_connect();
	extern void mob_disconnect(void);

	extern int mob_sync_db(sqlite3 * pDb);
	extern void mob_close_db(sqlite3 * pDb);
	extern sqlite3 * mob_open_db(const char * sPath);

	extern void mob_receive_proc(MobReceiveProc fn);

	extern int mob_get_interrupt(void);
	extern void mob_set_interrupt(int n);

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif /* _MOB_H_ */
