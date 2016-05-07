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
#include <iostream>
#include <thread>
#include <mutex>

#include "curl/curl.h"

std::mutex m;//you can use std::lock_guard if you want to be exception safe

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
	return written;
}

void makeACallFromPhoneBooth()
{
	m.lock();//man gets a hold of the phone booth door and locks it. The other men wait outside

	CURL *curl_handle;
	static const char *pagefilename = "page.out";
	FILE *pagefile;

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

	/* open the file */
	pagefile = fopen(pagefilename, "wb");
	if (pagefile) {

		/* write the page body to this file handle */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);

		/* get it! */
		curl_easy_perform(curl_handle);

		/* close the header file */
		fclose(pagefile);
	}

	/* cleanup curl stuff */
	curl_easy_cleanup(curl_handle);

	m.unlock();//man lets go of the door handle and unlocks the door
}

int main()
{
	curl_global_init(CURL_GLOBAL_ALL);

	std::thread man1(makeACallFromPhoneBooth);
	std::thread man2(makeACallFromPhoneBooth);

	man1.join();
	man2.join();

	return 0;
}
