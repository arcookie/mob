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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSender

void CSender::MissingCheck(qcc::String sList /* "" */)
{
	int sn, sn_end;
	std::map<qcc::String, qcc::String> miss;
	mReceive::iterator miter; sReceive::reverse_iterator siter;

	for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {

		for (siter = miter->second.rbegin(); siter != miter->second.rend();) {
			if ((*siter)->data.z) {
				sn_end = -1;
				sn = (*siter++)->snum - 1;

				if (siter == miter->second.rend() && sn > 0) sn_end = 0;
				else if (siter != miter->second.rend() && sn != (*siter)->snum_end) sn_end = (*siter)->snum_end;

				if (sn_end >= 0) {
					while (sn > sn_end) {
						if (miss[miter->first] != "") miss[miter->first] += ",";
						miss[miter->first] += qcc::I32ToString(sn);
						--sn;
					}
				}
			}
			else break;
		}
	}

	if (!sList.empty()) {
		qcc::String joiner;
		std::vector<qcc::String> v;
		std::size_t p1 = 0, p2, p3;

		while ((p1 = sList.find(';', p1)) != std::string::npos && (p2 = sList.find('@', p1)) != std::string::npos) {
			if ((p3 = sList.find(';', p2)) != std::string::npos) joiner = sList.substr(p2 + 1, p3 - p2 - 1);
			else joiner = sList.substr(p2 + 1);
			sn = atoi(sList.substr(p1 + 1, p2 - p1 - 1).data());

			if (miss.find(joiner) == miss.end()) miss[joiner] = "";

			if ((siter = m_mReceives[joiner].rbegin()) == m_mReceives[joiner].rend()) {
				int n = 0;

				while (++n <= sn) {
					if (miss[joiner] != "") miss[joiner] += ",";
					miss[joiner] += qcc::I32ToString(n);
				}
			}
			else {
				int n = (*siter)->snum_end;

				while (++n <= sn) {
					if (miss[joiner] != "") miss[joiner] += ",";
					miss[joiner] += qcc::I32ToString(n);
				}
			}
			p1 = p2;
		}
	}

	std::map<qcc::String, qcc::String>::iterator iter;

	for (iter = miss.begin(); iter != miss.end(); iter++) {
		if (!iter->second.empty()) m_pMob->SendData(iter->first.data(), time(NULL), ACT_MISSING, m_pMob->GetSessionID(), iter->second.data(), iter->second.size() + 1);
		else m_pMob->SendData(iter->first.data(), time(NULL), ACT_NO_MISSING, m_pMob->GetSessionID(), 0, 0);
	}
}

void CALLBACK fnMissingCheck(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR idEvent, DWORD /*dwTime*/)
{
	KillTimer(NULL, idEvent);
	
	gpMob->MissingCheck();
}

BOOL CSender::SetMissingTimer()
{
	mReceive::iterator miter; sReceive::reverse_iterator siter;

	for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {
		for (siter = miter->second.rbegin(); siter != miter->second.rend(); ) {
			if ((*siter)->data.z) {
				int sn = (*siter++)->snum - 1;
				if ((siter == miter->second.rend() && sn > 0) || (siter != miter->second.rend() && sn != (*siter)->snum_end)) {
					// std::thread t(func, interval, );
					SetTimer(NULL, TM_MISSING_CHECK, INT_MISSING_CHECK, &fnMissingCheck);
					return TRUE;
				}
			}
			else break;
		}
	}

	return FALSE;
}
