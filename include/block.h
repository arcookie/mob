/*
*   Memory block utility functions
*
*   2016.2.25
*
*   Copyright arCookie. All rights reserved.
*
*   The license under which the Mob source code is released is the GPLv2 (or later) from the Free Software Foundation.
*
*   A copy of the license is included with every copy of Mob source code, but you can also read the text of the license here(http://www.arcookie.com/?page_id=414).
*
*	This source code is a modified copy of sqldiff.c at http://www.sqlite.org
*/

#ifndef _BLOCK_H_
#define _BLOCK_H_

/*
** Make sure we can call this stuff from C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// types

	typedef struct {
		char *z;        /* Text of the string */
		int nAlloc;     /* Bytes allocated in z[] */
		int nUsed;      /* Bytes actually used in z[] */
	} Block;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// global functions

	extern void blkInit(Block *p);
	extern void blkFree(Block *p);
	extern int memCat(Block *p, const char * z, int n);
	extern int strPrintf(Block *p, const char *zFormat, ...);

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif /* _BLOCK_H_ */
