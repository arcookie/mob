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
