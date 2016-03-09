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
#include <time.h>
#include <ShlObj.h>
#include <alljoyn/Init.h>

#include "qcc/StringUtil.h"
#include "Global.h"
#include "AlljoynMob.h"


qcc::String gWPath;
HANDLE gMutex = NULL;
CAlljoynMob * gpMob = NULL;

const qcc::String get_unique_path(const char * ext)
{
	FILE *fp;
	qcc::String sPath;
	static ULONGLONG ullCount = 0;

	do {
		sPath = gWPath + qcc::I32ToString(++ullCount) + ext;
	} while (GetFileAttributes(sPath.data()) != INVALID_FILE_ATTRIBUTES);

	return sPath;
}

qcc::String GetVirtualStorePath()
{
	CHAR buffer[MAX_PATH];

	SHGetSpecialFolderPath(NULL, buffer, CSIDL_LOCAL_APPDATA, 0);

	return qcc::String(buffer) + "\\mob\\";
}

void remove_dir(qcc::String wFile)
{
	HANDLE				hFile;
	WIN32_FIND_DATA		nFileSizeLow;
	qcc::String			sFile;

	if ((hFile = FindFirstFile((wFile + "*.*").data(), &nFileSizeLow)) != INVALID_HANDLE_VALUE){
		do {
			sFile = nFileSizeLow.cFileName;
			if (!sFile.empty() && sFile != "." && sFile != ".."){
				sFile = wFile + sFile;
				if (nFileSizeLow.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
					remove_dir(sFile + "\\");
					RemoveDirectory(sFile.data());
				}
				else {
					SetFileAttributes(sFile.data(), FILE_ATTRIBUTE_NORMAL);
					DeleteFile(sFile.data());
				}
			}
		} while (FindNextFile(hFile, &nFileSizeLow));

		FindClose(hFile);
	}
}

int get_file_mtime(const char * path)
{
	HANDLE fh;

	if ((fh = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE) {
		FILETIME modtime;
		SYSTEMTIME stUTC;
		char buf[32];

		GetFileTime(fh, NULL, NULL, &modtime);

		CloseHandle(fh);

		FileTimeToSystemTime(&modtime, &stUTC);

		sprintf(buf, "%02d%02d%02d%02d%02d", stUTC.wMonth, stUTC.wDay, stUTC.wHour, stUTC.wMinute, stUTC.wSecond);

		return atoi(buf);
	}

	return 0;
}

long get_file_length(const char * path)
{
	FILE *fp;
	long sz;

	if ((fp = fopen(path, "rb")) != NULL) {
		fseek(fp, 0, SEEK_END);
		sz = ftell(fp);
		fclose(fp);
		return sz;
	}
	return 0L;
}

const qcc::String mem2file(const char * data, int length, const char * ext)
{
	FILE *fp;
	qcc::String sPath = get_unique_path(ext);

	if ((fp = fopen(sPath.data(), "wb")) != NULL) {
		fwrite(data, sizeof(char), length, fp);
		fclose(fp);
		return sPath;
	}
	return "";
}

void CALLBACK fnSendSignal(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR idEvent, DWORD /*dwTime*/)
{
	KillTimer(NULL, idEvent);

	SYNC_SIGNAL ss;
	sqlite3_stmt *pStmt = NULL;

	strcpy_s(ss.uid, sizeof(ss.uid), gpMob->GetJoinName());

	QUERY_SQL_V(gpMob->GetMainDB(), pStmt, ("SELECT MAX(sn) AS n FROM works WHERE uid = %Q;", ss.uid),
		ss.sn = sqlite3_column_int(pStmt, 0);
	if (ss.sn != gpMob->GetSerial()) alljoyn_send(gpMob->GetSessionID(), NULL, ACT_SIGNAL, 0, 0, (const char *)&ss, sizeof(SYNC_SIGNAL));
	break;
	);
}

int alljoyn_send(unsigned int nSID, const char * pJoiner, int nAction, char * sText, int nLength, const char * pExtra, int nExtLen)
{
	time_t aid = time(NULL);
	int ret = gpMob->SendData(pJoiner, aid, nAction, nSID, sText, nLength, pExtra, nExtLen);

	if (sText && ER_OK == ret) {
		int l;
		char * p = sText;
		Block data;
		char * p2;
		FILE_SEND_ITEM fsi;

		blkInit(&data);

		while ((p = strstr(p, "file://")) != NULL) {
			p += 7;
			if ((p2 = strchr(p, '\'')) != NULL && (l = (p2 - p)) > 0) {
				if (l < MAX_URI) {
					memcpy(fsi.uri, p, l);
					fsi.uri[l] = 0;
					fsi.mtime = get_file_mtime(fsi.uri);
					fsi.fsize = get_file_length(fsi.uri);

					memCat(&data, (char *)&fsi, sizeof(FILE_SEND_ITEM));
				}

				p = p2 + 1;
			}
			else p++;
		}
		if (data.nUsed > 0) {
			ret = gpMob->SendData(pJoiner, aid, ACT_FLIST, nSID, data.z, data.nUsed);
			SetTimer(NULL, TM_SEND_SIGNAL, INT_SEND_SIGNAL, &fnSendSignal);
		}

		blkFree(&data);
	}

	return ret;
}
