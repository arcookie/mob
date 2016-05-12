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

void __cdecl SigIntHandler(int /*sig*/)
{
	mob_set_interrupt(1);
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

	alljoyn_init(argc, argv);

	sqlite3 * db = mob_open_db(":memory:");

	mob_connect();

	const int bufSize = 1024;
	char buf[bufSize];

	while (gets(buf) && !mob_get_interrupt()) {
		if (sqlite3_exec(db, buf, 0, 0, NULL) == SQLITE_OK) {
			mob_sync_db(db);
			printf("%s\n", buf);
		}
	}

	mob_disconnect();

	return 0;
}
