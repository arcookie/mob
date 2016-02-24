/*
*   2016.2.25
*
*   Copyright arCookie. All rights reserved.
*
*   The license under which the Mob source code is released is the GPLv2 (or later) from the Free Software Foundation.
*
*   A copy of the license is included with every copy of Mob source code, but you can also read the text of the license here(http://www.arcookie.com/?page_id=414).
*/

#include <string.h>
#include <stdarg.h>

#include "util.h"
#include "sqlite3.h"

/*
** Initialize a Str object
*/
void strInit(Str *p)
{
	p->z = 0;
	p->nAlloc = 0;
	p->nUsed = 0;
}

/*
** Free all memory held by a Str object
*/
void strFree(Str *p)
{
	sqlite3_free(p->z);
	strInit(p);
}

/*
** Add formatted text to the end of a Str object
*/
int strPrintf(Str *p, const char *zFormat, ...)
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


char * get_file_path(const char * path)
{
	return 0;
}

int get_file_mtime(const char * path)
{
	return 0;
}

long long get_file_length(const char * path)
{
	return 0L;
}

int catmem(char ** data, void * fsi, int len)
{
	return 0;
}
