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

#include "Global.h"
#include "AlljoynMob.h"
#include "ThreadTimer.h"
#include "curl/curl.h"
#include "json/json.h"

void CThreadTimer::ThreadProc()
{
	do {
		m_nCount = 0;

		while (m_nCount < m_nInterval) {
			m_nCount += 100;
			Sleep(100);
		}

		if (m_nInterval > 0 && m_nInterval <= m_nCount) run();
		else break;

	} while (!m_bOnce.load() || m_nInterval > m_nCount);
}

void CThreadTimer::_SetTimer(int nInterval, bool bOnce)
{
	m_bOnce = bOnce;
	m_nInterval = nInterval;
	if (!m_Thread.joinable()) m_Thread = std::thread(&CThreadTimer::ThreadProc, this);
	else m_nCount = 0;
}

void CThreadTimer::KillTimer()
{
	if (m_Thread.joinable()) {
		m_nInterval = 0;
		m_nCount = 0;
		m_bOnce = true;
	}
}

CThreadTimer::~CThreadTimer()
{
	if (m_Thread.joinable()) {
		KillTimer();
		m_Thread.join();
	}
}

CSignalTimer * gpSignalTimer = NULL;
CWebPollingTimer * gpWebPollingTimer = NULL;
CMissingCheckTimer * gpMissingCheckTimer = NULL;

void CMissingCheckTimer::run()
{
	gpMob->MissingCheck();
}

void CSignalTimer::run()
{
	if (!gpMob->SendSignal()) KillTimer();
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	std::string * pCmd = (std::string *)stream;

	int n = pCmd->size();

	*pCmd += (const char *)ptr;

	return pCmd->size() - n;
}

void CWebPollingTimer::run()
{
	m[2].lock();//man gets a hold of the phone booth door and locks it. The other men wait outside

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

	m[2].unlock();//man lets go of the door handle and unlocks the door
}
