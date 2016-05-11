/***************************************************************************
*                                  _   _ ____  _
*  Project                     ___| | | |  _ \| |
*                             / __| | | | |_) | |
*                            | (__| |_| |  _ <| |___
*                             \___|\___/|_| \_\_____|
*
* Copyright (C) 1998 - 2016, Daniel Stenberg, <daniel@haxx.se>, et al.
*
* This software is licensed as described in the file COPYING, which
* you should have received as part of this distribution. The terms
* are also available at https://curl.haxx.se/docs/copyright.html.
*
* You may opt to use, copy, modify, merge, publish, distribute and/or sell
* copies of the Software, and permit persons to whom the Software is
* furnished to do so, under the terms of the COPYING file.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
*
***************************************************************************/
/* <DESC>
* Download a given URL into a local file named page.out.
* </DESC>
*/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <string>

#include "mob.h"
#include "curl/curl.h"
#include "json/json.h"

std::mutex m;//you can use std::lock_guard if you want to be exception safe

void __cdecl SigIntHandler(int /*sig*/)
{
	mob_set_interrupt(1);
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	std::string * pCmd = (std::string *)stream;

	int n = pCmd->size();

	*pCmd += (const char *)ptr;

	return pCmd->size() - n;
}

void makeACallFromPhoneBooth()
{
	m.lock();//man gets a hold of the phone booth door and locks it. The other men wait outside

	CURL *curl_handle;
	std::string sCmd;

	/* init the curl session */
	curl_handle = curl_easy_init();

	/* set URL to get here */
	curl_easy_setopt(curl_handle, CURLOPT_URL, "http://www.arcookie.com/cmd.json");

	/* Switch on full protocol/debug output while testing */
	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);

	/* disable progress meter, set to 0L to enable and disable debug output */
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

	/* send all data to this function  */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

		/* write the page body to this file handle */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &sCmd);

	/* get it! */
	curl_easy_perform(curl_handle);

	/* cleanup curl stuff */
	curl_easy_cleanup(curl_handle);

	Json::Value jData;
	Json::Reader read;

	if (read.parse(sCmd, jData) && jData.size() > 0) {
		printf("%s\n", jData[0L]["cmd"].asCString());
	}

	m.unlock();//man lets go of the door handle and unlocks the door
}

static void usage()
{
	printf("Usage: mob [-h] [-s <name>] | [-j <name>]\n");
	exit(EXIT_FAILURE);
}

static void alljoyn_init(int argc, char** argv)
{
	char * joinName = 0;
	char * advertisedName = 0;

	/* Parse command line args */
	for (int i = 1; i < argc; ++i) {
		if (0 == strcmp("-s", argv[i])) {
			if ((++i < argc) && (argv[i][0] != '-')) {
				advertisedName = sqlite3_mprintf("%s%s", NAME_PREFIX, argv[i]);
			}
			else {
				printf("Missing parameter for \"-s\" option\n");
				usage();
			}
		}
		else if (0 == strcmp("-j", argv[i])) {
			if ((++i < argc) && (argv[i][0] != '-')) {
				joinName = sqlite3_mprintf("%s%s", NAME_PREFIX, argv[i]);
			}
			else {
				printf("Missing parameter for \"-j\" option\n");
				usage();
			}
		}
		else {
			if (0 != strcmp("-h", argv[i])) printf("Unknown argument \"%s\"\n", argv[i]);
			usage();
		}
	}
	/* Validate command line */
	if (advertisedName && joinName) {
		printf("Must specify either -s or -j\n");
		usage();
	}
	else if (!advertisedName && !joinName) {
		printf("Cannot specify both -s  and -j\n");
		usage();
	}

	mob_init((advertisedName ? 1 : 0), (advertisedName ? advertisedName : joinName));

	if (advertisedName) sqlite3_free(advertisedName);
	if (joinName) sqlite3_free(joinName);
}

int SQLITE_CDECL main(int argc, char **argv)
{
	/* Install SIGINT handler. */
	signal(SIGINT, SigIntHandler);

	curl_global_init(CURL_GLOBAL_ALL);

	alljoyn_init(argc, argv);

	sqlite3 * db = mob_open_db(":memory:");

	mob_connect();

	const int bufSize = 1024;
	char buf[bufSize];

	std::thread man1(makeACallFromPhoneBooth);
	std::thread man2(makeACallFromPhoneBooth);

	while (gets(buf) && !mob_get_interrupt()) {
		m.lock();
		if (sqlite3_exec(db, buf, 0, 0, NULL) == SQLITE_OK) {
			mob_sync_db(db);
			printf("%s\n", buf);
		}
		m.unlock();
	}

	mob_disconnect();

	man1.join();
	man2.join();

	return 0;
}

/*


#pragma once

typedef void(*TM_FUNC)(WPARAM wParam, LPARAM lParam);

typedef struct {
	HANDLE	handle;
	BOOL	stopped;
	DWORD	tick;
	DWORD	timming;	
	TM_FUNC func;
	WPARAM	wparam;
	LPARAM	lparam;
} HNC_TM_INFO;

class CHncCommTimer
{
public:
	CHncCommTimer()	{ ZeroMemory(m_stTmInfo, sizeof(m_stTmInfo)); }
	~CHncCommTimer();

	enum {
		T_SEND_SIGNAL = 0,
		T_SEND_WORK_SLIST,		
		T_CHECK_MISSING,		
		T_VNET_POLLING,
		T_OPEN_JSON_FILE,
		T_CLOSE_ALIVE,
		T_NAVIGATE,
		T_WORK_SIGNAL,
		T_APPLY_WORK_REPEAT,
		T_APPLY_WORK,
		T_LOGIN_WAIT_ON_IP_DROPPED_CLIENT,
		T_LOGIN_FAILED,
		T_LOGIN_WAIT,
		T_WDB_REQUESTS,
		T_REDRAW_MS,
		T_CHAT_UPDATE,
		T_COUNT
	};

	enum {
		ELAPSE_T_SEND_SIGNAL = (CLOCKS_PER_SEC * 30),
		ELAPSE_T_SEND_WORK_SLIST = (CLOCKS_PER_SEC * 30),
		ELAPSE_T_CHECK_MISSING = (CLOCKS_PER_SEC / 10),
		ELAPSE_T_VNET_POLLING = 0,
		ELAPSE_T_OPEN_JSON_FILE = (CLOCKS_PER_SEC * 2),
		ELAPSE_T_CLOSE_ALIVE = (CLOCKS_PER_SEC / 2),
		ELAPSE_T_NAVIGATE = (CLOCKS_PER_SEC / 2),
		ELAPSE_T_WORK_SIGNAL = (CLOCKS_PER_SEC * 30),
		ELAPSE_T_APPLY_WORK_REPEAT = (CLOCKS_PER_SEC / 10),
		ELAPSE_T_APPLY_WORK = (CLOCKS_PER_SEC / 10),
		ELAPSE_T_LOGIN_WAIT_ON_IP_DROPPED_CLIENT = (CLOCKS_PER_SEC * 20),
		ELAPSE_T_LOGIN_FAILED = (CLOCKS_PER_SEC * 60),
		ELAPSE_T_LOGIN_WAIT = (CLOCKS_PER_SEC * 60),
		ELAPSE_T_WDB_REQUESTS = (CLOCKS_PER_SEC / 2),
		ELAPSE_T_WDB_REQUESTS_RETRY = (CLOCKS_PER_SEC * 3),
		ELAPSE_T_REDRAW_MS = (CLOCKS_PER_SEC / 10),
		ELAPSE_T_CHAT_UPDATE = (CLOCKS_PER_SEC / 10),
		ELAPSE_T_COUNT = 0
	};

	void KillTimer(DWORD dwID) { if (dwID >= 0 && dwID < T_COUNT) m_stTmInfo[dwID].tick = GetTickCount(); }
	void SetTimer(DWORD dwID, DWORD dwInterval, TM_FUNC fnFunc, WPARAM wParam, LPARAM lParam);

	static void ClearTimers();
	static void KillTimer(int nDocID, DWORD dwID);
	static void SetTimer(int nDocID, DWORD dwID, DWORD dwInterval, TM_FUNC fnFunc, WPARAM wParam, LPARAM lParam);

private:
	static UINT WINAPI	TimerThread(LPVOID pVoid);

	HNC_TM_INFO m_stTmInfo[T_COUNT];
};

extern std::map<int, CHncCommTimer*> gHncCommTimers;





#include "stdafx.h"
#include "HncCommTimer.h"

#define SLEEP_TIME		100
#define WAIT_TIME		5000

class HNC_TM_PARAM{
public:
	HNC_TM_PARAM(WPARAM	wParam, LPARAM lParam)	{ m_wParam = wParam; m_lParam = lParam; }
	WPARAM	m_wParam;
	LPARAM	m_lParam;
};

std::map<int, CHncCommTimer*> gHncCommTimers;

UINT WINAPI CHncCommTimer::TimerThread(LPVOID pVoid)
{
	HNC_TM_PARAM * pHTP = (HNC_TM_PARAM *)pVoid;

	if (pHTP) {
		CHncCommTimer * pTM = (CHncCommTimer *)pHTP->m_wParam;
		DWORD dwID = pHTP->m_lParam;

		delete pHTP;

		if (dwID >= 0 && dwID < T_COUNT) {
			DWORD dwTick = pTM->m_stTmInfo[dwID].tick;
			DWORD dwCount = pTM->m_stTmInfo[dwID].timming / 100;
			DWORD dwTimming = pTM->m_stTmInfo[dwID].timming % 100;

			while (dwCount-- > 0 && pTM->m_stTmInfo[dwID].tick == dwTick) Sleep(100);

			if (pTM->m_stTmInfo[dwID].tick == dwTick && dwTimming > 0) Sleep(dwTimming);
			if (pTM->m_stTmInfo[dwID].tick == dwTick) pTM->m_stTmInfo[dwID].func(pTM->m_stTmInfo[dwID].wparam, pTM->m_stTmInfo[dwID].lparam);

			pTM->m_stTmInfo[dwID].stopped = true;
		}
	}

	return 0;
}

CHncCommTimer::~CHncCommTimer()
{
	int i = -1;

	while (++i < T_COUNT) {
		m_stTmInfo[i].tick = (DWORD)(-1);
		UINT nCount = 0;
		while (1) {
			DWORD dwExitCode = 0L;
			GetExitCodeThread(m_stTmInfo[i].handle, &dwExitCode);
			if (dwExitCode == STILL_ACTIVE) {
				if (nCount < (WAIT_TIME / SLEEP_TIME)) {
					nCount++;
					Sleep(SLEEP_TIME);
				}
				else {
					TerminateThread(m_stTmInfo[i].handle, dwExitCode);
					break;
				}
			}
			else break;
		}
	}
}

void CHncCommTimer::ClearTimers()
{
	std::map<int, CHncCommTimer*>::const_iterator iter;

	for (iter = gHncCommTimers.begin(); iter != gHncCommTimers.end(); iter++) {
		delete iter->second;
	}

	gHncCommTimers.clear();
}

void CHncCommTimer::KillTimer(int nDocID, DWORD dwID)
{
	std::map<int, CHncCommTimer*>::const_iterator iter = gHncCommTimers.find(nDocID);

	if (iter != gHncCommTimers.end()) iter->second->KillTimer(dwID);
}

void CHncCommTimer::SetTimer(int nDocID, DWORD dwID, DWORD dwInterval, TM_FUNC fnFunc, WPARAM wParam, LPARAM lParam)
{
	CHncCommTimer * pHCT;
	std::map<int, CHncCommTimer*>::const_iterator iter = gHncCommTimers.find(nDocID);

	if (iter == gHncCommTimers.end()) {
		pHCT = new CHncCommTimer;
		gHncCommTimers[nDocID] = pHCT;
	}
	else pHCT = iter->second;

	pHCT->SetTimer(dwID, dwInterval, fnFunc, wParam, lParam);
}

void CHncCommTimer::SetTimer(DWORD dwID, DWORD dwInterval, TM_FUNC fnFunc, WPARAM wParam, LPARAM lParam)
{
	if (dwID >= 0 && dwID < T_COUNT) {

		m_stTmInfo[dwID].tick = GetTickCount();
		if (m_stTmInfo[dwID].tick > 0) {
			m_stTmInfo[dwID].timming = dwInterval;
			m_stTmInfo[dwID].func = fnFunc;
			m_stTmInfo[dwID].wparam = wParam;
			m_stTmInfo[dwID].lparam = lParam;
			m_stTmInfo[dwID].stopped = false;

			m_stTmInfo[dwID].handle = (HANDLE)_beginthreadex(NULL, 0, TimerThread, (LPVOID)(new HNC_TM_PARAM((WPARAM)this, (LPARAM)dwID)), 0, 0);
		}
	}
}


*/