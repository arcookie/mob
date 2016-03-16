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


#define FIND_IN_RECV(a,b,c,...)	{mReceives::iterator a; mReceive::iterator b; sReceive::iterator c; \
	for (a = m_mReceives.begin(); a != m_mReceives.end(); a++) { for (b = a->second.begin(); b != a->second.end(); b++) {\
	for (c = b->second.begin(); c != b->second.end(); c++) { __VA_ARGS__ }}}}

void CSender::MissingCheck(const char * sJoiner /* NULL */, int nSNum /* -1 */)
{
	int n = 0;
	RECEIVE rcv;
	std::map<qcc::String, qcc::String> miss;

	FIND_IN_RECV(iter, _iter, __iter, 
		if (!(*__iter)->data.empty() && (*__iter)->snum > 1) {
			rcv.snum_end = rcv.snum = (*__iter)->snum - 1;
			if (_iter->second.find(&rcv) == _iter->second.end()) {
				if (miss[_iter->first] != "") miss[_iter->first] += ",";
				miss[_iter->first] += qcc::I32ToString((*__iter)->snum - 1);
			}
			else if (sJoiner && _iter->first == sJoiner && n < (*__iter)->snum) n = (*__iter)->snum;
		}
	);

	if (sJoiner) {
		while (++n <= nSNum) {
			if (miss[sJoiner] != "") miss[sJoiner] += ",";
			miss[sJoiner] += qcc::I32ToString(n);
		}
	}

	if (!miss.empty()) {
		std::map<qcc::String, qcc::String>::iterator iter;

		for (iter = miss.begin(); iter != miss.end(); iter++) {
			m_pMob->SendData(iter->first.data(), time(NULL), ACT_MISSING, m_pMob->GetSessionID(), iter->second.data(), iter->second.size());
		}
	}
	else if (sJoiner) m_pMob->SendData(sJoiner, time(NULL), ACT_NO_MISSING, m_pMob->GetSessionID(), 0, 0);
}

void CALLBACK fnMissingCheck(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR idEvent, DWORD /*dwTime*/)
{
	KillTimer(NULL, idEvent);
	
	gpMob->MissingCheck();
}

BOOL CSender::SetMissingTimer()
{
	RECEIVE rcv;

	FIND_IN_RECV(iter, _iter, __iter,
		if (!(*__iter)->data.empty() && (*__iter)->snum > 1) {
			rcv.snum_end = rcv.snum = (*__iter)->snum - 1;
			if (_iter->second.find(&rcv) == _iter->second.end()) {
				SetTimer(NULL, TM_MISSING_CHECK, INT_MISSING_CHECK, &fnMissingCheck);
				return TRUE;
			}
		}
	);

	return FALSE;
}
