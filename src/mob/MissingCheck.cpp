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
#include <qcc/StringUtil.h>

#include "dbutil.h"
#include "Global.h"
#include "Sender.h"
#include "AlljoynMob.h"


void CSender::MissingCheck(const char * sJoiner /* NULL */, int nSNum /* -1 */)
{
	std::map<qcc::String, qcc::String> miss;
	std::map<qcc::String, qcc::String>::iterator iter;

	if (MissingCheck(miss, sJoiner, nSNum)) {
		for (iter = miss.begin(); iter != miss.end(); iter++) {
			if (!iter->second.empty()) m_pMob->SendData(iter->first.data(), time(NULL), ACT_MISSING, m_pMob->GetSessionID(), iter->second.data(), iter->second.size());
		}
	}
	else m_pMob->SendData(sJoiner, time(NULL), ACT_NO_MISSING, m_pMob->GetSessionID(), 0, 0);
}

void CALLBACK fnMissingCheck(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR idEvent, DWORD /*dwTime*/)
{
	KillTimer(NULL, idEvent);
	
	gpMob->MissingCheck();
}

BOOL CSender::MissingCheck(std::map<qcc::String, qcc::String> & sRes, const char * sJoiner /* NULL */, int nEnd /* -1 */)
{
	int n = 0;
	RECEIVE rcv;
	mReceives::iterator iter;
	mReceive::iterator _iter;
	sReceive::iterator __iter;

	sRes.clear();

	for (iter = m_mReceives.begin(); iter != m_mReceives.end(); iter++) {
		for (_iter = iter->second.begin(); _iter != iter->second.end(); _iter++) {
			for (__iter = _iter->second.begin(); __iter != _iter->second.end(); __iter++) {
				if (!(*__iter)->data.empty() && (*__iter)->snum > 1) {
					rcv.snum = (*__iter)->snum - 1;
					if (_iter->second.find(&rcv) == _iter->second.end()) {
						if (sRes[_iter->first] != "") sRes[_iter->first] += ",";
						sRes[_iter->first] += qcc::I32ToString((*__iter)->snum - 1);
					}
					else if (sJoiner && _iter->first == sJoiner && n < (*__iter)->snum) n = (*__iter)->snum;
				}
			}
		}
	}

	if (sJoiner) {
		while (++n <= nEnd) {
			if (sRes[sJoiner] != "") sRes[sJoiner] += ",";
			sRes[sJoiner] += qcc::I32ToString(n);
		}
	}

	return !sRes.empty();
}

BOOL CSender::SetMissingTimer()
{
	RECEIVE rcv;
	mReceives::iterator iter;
	mReceive::iterator _iter;
	sReceive::iterator __iter;

	for (iter = m_mReceives.begin(); iter != m_mReceives.end(); iter++) {
		for (_iter = iter->second.begin(); _iter != iter->second.end(); _iter++) {
			for (__iter = _iter->second.begin(); __iter != _iter->second.end(); __iter++) {
				if (!(*__iter)->data.empty() && (*__iter)->snum > 1) {
					rcv.snum = (*__iter)->snum - 1;
					if (_iter->second.find(&rcv) == _iter->second.end()) {
						SetTimer(NULL, TM_MISSING_CHECK, INT_MISSING_CHECK, &fnMissingCheck);
						return TRUE;
					}
				}
			}
		}
	}

	return FALSE;
}
