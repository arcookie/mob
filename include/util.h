
#ifndef _UTIL_H_
#define _UTIL_H_

#include "sqlite3.h"

#define MAX_URI	1024

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
	extern void strInit(Str *p);

	/*
	** Free all memory held by a Str object
	*/
	extern void strFree(Str *p);

	/*
	** Add formatted text to the end of a Str object
	*/
	extern int strPrintf(Str *p, const char *zFormat, ...);

	extern void get_diff(sqlite3 * pDB, const char * zBackDB, Str * base, Str * redo, Str * undo);

	extern int get_file_mtime(const char * path);
	extern char * get_file_path(const char * path);
	extern long long get_file_length(const char * path);
	extern int catmem(char ** data, void * fsi, int len);

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif /* _UTIL_H_ */
