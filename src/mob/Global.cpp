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
#include <Shlwapi.h>
#include <alljoyn/Init.h>

#include "qcc/StringUtil.h"
#include "Global.h"
#include "AlljoynMob.h"


std::mutex m[NUM_OF_MUTEXES];
qcc::String gWPath;
HANDLE gMutex = NULL;
CAlljoynMob * gpMob = NULL;
MobReceiveProc fnReceiveProc = NULL;

const qcc::String get_unique_path(const char * sExt)
{
	FILE *fp;
	qcc::String sPath;
	static ULONGLONG ullCount = 0;

	do {
		sPath = gWPath + qcc::I32ToString(++ullCount) + sExt;
	} while (GetFileAttributes(sPath.data()) != INVALID_FILE_ATTRIBUTES);

	return sPath;
}

qcc::String get_writable_path()
{
	CHAR buffer[MAX_PATH];

	SHGetSpecialFolderPath(NULL, buffer, CSIDL_LOCAL_APPDATA, 0);

	return qcc::String(buffer) + "\\mob\\";
}

void remove_dir(qcc::String sPath)
{
	HANDLE				hFile;
	WIN32_FIND_DATA		nFileSizeLow;
	qcc::String			sFile;

	if ((hFile = FindFirstFile((sPath + "*.*").data(), &nFileSizeLow)) != INVALID_HANDLE_VALUE){
		do {
			sFile = nFileSizeLow.cFileName;
			if (!sFile.empty() && sFile != "." && sFile != ".."){
				sFile = sPath + sFile;
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

int get_file_mtime(const char * sPath)
{
	HANDLE fh;

	if ((fh = CreateFile(sPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE) {
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

long get_file_length(const char * sPath)
{
	FILE *fp;
	long sz;

	if ((fp = fopen(sPath, "rb")) != NULL) {
		fseek(fp, 0, SEEK_END);
		sz = ftell(fp);
		fclose(fp);
		return sz;
	}
	return 0L;
}

const qcc::String mem2file(const char * data, int length, const char * sExt)
{
	FILE *fp;
	qcc::String sPath = get_unique_path(sExt);

	if ((fp = fopen(sPath.data(), "wb")) != NULL) {
		if (length > 0) fwrite(data, sizeof(char), length, fp);
		fclose(fp);
		return sPath;
	}
	return "";
}

int alljoyn_send(unsigned int nSessionID, const char * pJoiner, int nAction, const char * sText, int nLength, const char * pExtra /* NULL */, int nExtLen /* 0 */)
{
	time_t footprint = time(NULL);
	int ret = gpMob->SendData(pJoiner, footprint, nAction, nSessionID, sText, nLength, pExtra, nExtLen);

	if (sText && ER_OK == ret) {
		int l;
		const char * p = sText;
		Block data;
		const char * p2;
		qcc::String path;
		FILE_SEND_ITEM fsi;

		blkInit(&data);

		while ((p = strstr(p, "file://")) != NULL) {
			if ((p2 = strchr(p, '\'')) != NULL && (l = (p2 - p)) > 0) {
				if (l < MAX_PATH) {
					memcpy(fsi.uri, p, l);
					fsi.uri[l] = 0;
					path = get_path(fsi.uri);
					fsi.mtime = get_file_mtime(path.data());
					fsi.fsize = get_file_length(path.data());

					mem2mem(&data, (char *)&fsi, sizeof(FILE_SEND_ITEM));
				}

				p = p2 + 1;
			}
			else p++;
		}
		if (data.nUsed > 0) ret = gpMob->SendData(pJoiner, footprint, ACT_FLIST, nSessionID, data.z, data.nUsed);
		else gpMob->SendData(pJoiner, footprint, ACT_END, nSessionID, 0, 0);

		blkFree(&data);
	}

	return ret;
}

qcc::String get_uri(const char * path)
{
	TCHAR output[MAX_PATH]; // allocate buffer in memory (stack)
	DWORD dwDisp = MAX_PATH; // max posible buffer size
	LPDWORD lpdwDisp = &dwDisp;
	HRESULT res2 = UrlCreateFromPath(path, output, lpdwDisp, NULL);

	return output;
}

qcc::String get_path(const char * uri)
{
	TCHAR output[MAX_PATH]; // allocate buffer in memory (stack)
	DWORD dwDisp = MAX_PATH; // max posible buffer size
	LPDWORD lpdwDisp = &dwDisp;
	HRESULT res2 = PathCreateFromUrl(uri, output, lpdwDisp, NULL);

	return output;
}
