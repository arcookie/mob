
#ifndef _UTIL_H_
#define _UTIL_H_

#include "sqlite3.h"

/*
** Make sure we can call this stuff from C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

	/*
	** Dynamic string object
	*/
	typedef struct Str Str;

	struct Str {
		char *z;        /* Text of the string */
		int nAlloc;     /* Bytes allocated in z[] */
		int nUsed;      /* Bytes actually used in z[] */
	};

	/*
	** Initialize a Str object
	*/
	void strInit(Str *p);

	/*
	** Free all memory held by a Str object
	*/
	void strFree(Str *p);

	/*
	** Add formatted text to the end of a Str object
	*/
	int strPrintf(Str *p, const char *zFormat, ...);

	void get_diff(sqlite3 * pDB, const char * zBackDB, Str * redo, Str * undo);

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif /* _UTIL_H_ */
