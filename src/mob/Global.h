/*
*   2016.2.25
*
*   Copyright arCookie. All rights reserved.
*
*   The license under which the Mob source code is released is the GPLv2 (or later) from the Free Software Foundation.
*
*   A copy of the license is included with every copy of Mob source code, but you can also read the text of the license here(http://www.arcookie.com/?page_id=414).
*
****************************************************************************************
*
*   Copyright AllSeen Alliance. All rights reserved.
*
*   Permission to use, copy, modify, and/or distribute this software for any
*   purpose with or without fee is hereby granted, provided that the above
*   copyright notice and this permission notice appear in all copies.
*
*   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
*   WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
*   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
*   ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
*   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
*   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
*   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include "mob.h"
#include "qcc/String.h"
#include "sqlite3.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// defines

#define ACT_DATA		0
#define ACT_FLIST		1
#define ACT_FLIST_REQ	2
#define ACT_FILE		3
#define ACT_MISSING		4
#define ACT_SIGNAL		5
#define ACT_NO_MISSED	6
#define ACT_END			7

#define QUERY_SQL(__db__, __stmt__, __sql__, ...)	\
if (sqlite3_prepare_v2(__db__, __sql__, -1, &__stmt__, NULL) == SQLITE_OK) {\
	while (sqlite3_step(__stmt__) == SQLITE_ROW) { __VA_ARGS__ } sqlite3_finalize(__stmt__); __stmt__ = NULL; }

#define QUERY_SQL_V(__db__, __stmt__, __sql__, ...)	\
		{ char * __zSQL__ = sqlite3_mprintf __sql__; if (__zSQL__ && sqlite3_prepare_v2(__db__, __zSQL__, -1, &__stmt__, NULL) == SQLITE_OK) {\
		while (sqlite3_step(__stmt__) == SQLITE_ROW) { __VA_ARGS__ } sqlite3_finalize(__stmt__); __stmt__ = NULL; }	sqlite3_free(__zSQL__);	}

#define EXECUTE_SQL_V(__db__, __sql__, ...)	\
		{ char * __zSQL__ = sqlite3_mprintf __sql__; if (__zSQL__) { sqlite3_exec(__db__, __zSQL__, 0, 0, 0); sqlite3_free(__zSQL__); }}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// types

typedef struct {
	int auto_inc;
	int snum;
	int snum_prev;
	char joiner[16];
	char joiner_prev[16];
	char base_table[64];
} SYNC_DATA;

typedef struct {
	int auto_inc;
	char joiner[16];
} SYNC_SIGNAL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// classes

class CAlljoynMob;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// global variables

extern HANDLE gMutex;
extern qcc::String gWPath;
extern CAlljoynMob * gpMob;
extern MobReceiveProc fnReceiveProc;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// global functions

extern qcc::String get_writable_path();
extern void remove_dir(qcc::String sPath);
extern int get_file_mtime(const char * sPath);
extern qcc::String get_uri(const char * path);
extern qcc::String get_path(const char * uri);
extern long get_file_length(const char * sPath);
extern const qcc::String get_unique_path(const char * sExt);
extern const qcc::String mem2file(const char * data, int length, const char * sExt);
extern int alljoyn_send(unsigned int nSID, const char * pJoiner, int nAction, char * sText, int nLength, const char * pExtra, int nExtLen);

#endif /* _GLOBAL_H_ */

