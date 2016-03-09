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

#include "block.h"

#include <stdarg.h>

/*
** Initialize a Block object
*/
void blkInit(Block *p)
{
	p->z = 0;
	p->nAlloc = 0;
	p->nUsed = 0;
}

/*
** Free all memory held by a Block object
*/
void blkFree(Block *p)
{
	sqlite3_free(p->z);
	blkInit(p);
}

/*
** Add formatted text to the end of a Block object
*/
int strPrintf(Block *p, const char *zFormat, ...)
{
	int nNew;
	for (;;){
		if (p->z){
			va_list ap;
			va_start(ap, zFormat);
			sqlite3_vsnprintf(p->nAlloc - p->nUsed, p->z + p->nUsed, zFormat, ap);
			va_end(ap);
			nNew = (int)strlen(p->z + p->nUsed);
		}
		else{
			nNew = p->nAlloc;
		}
		if (p->nUsed + nNew < p->nAlloc - 1){
			p->nUsed += nNew;
			break;
		}
		p->nAlloc = p->nAlloc * 2 + 1000;
		p->z = sqlite3_realloc(p->z, p->nAlloc);
		if (!p->z) break;
	}
	return !!p->z;
}

/*
** Add memory to the end of a Block object
*/
int memCat(Block *p, const char * z, int n)
{
	int nNew;
	for (;;){
		if (p->z){
			if (p->nUsed + n < p->nAlloc - 1) memcpy(p->z + p->nUsed, z, n);
			else nNew = n;
		}
		else{
			nNew = p->nAlloc;
		}
		if (p->nUsed + nNew < p->nAlloc - 1){
			p->nUsed += nNew;
			break;
		}
		p->nAlloc = p->nAlloc * 2 + 1000;
		p->z = sqlite3_realloc(p->z, p->nAlloc);
		if (!p->z) break;
	}
	return !!p->z;
}
