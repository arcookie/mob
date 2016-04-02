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

void CSender::MissingCheck(const char * sJoiner /* NULL */, int nSNum /* -1 */)
{
	mReceive::iterator b; sReceive::reverse_iterator c;

	for (b = m_mReceives.begin(); b != m_mReceives.end(); b++) {
		qcc::String miss;

		for (c = b->second.rbegin(); c != b->second.rend(); c++) {
			if ((*c)->data.z) {
				int sn_end = -1;
				int sn = (*c++)->snum - 1;

				if (c == b->second.rend() && sn > 0) sn_end = 0;
				else if(c != b->second.rend() && sn != (*c)->snum_end) sn_end = (*c)->snum_end;

				if (sn_end >= 0) {
					while (sn > sn_end) {
						if (miss != "") miss += ",";
						miss += qcc::I32ToString(sn);
						--sn;
					}
				}
			}
			else break;
		}

		if (sJoiner && b->first == sJoiner && nSNum > 0) {
			if ((c = b->second.rbegin()) == b->second.rend()) {
				int n = 0;

				while (++n <= nSNum) {
					if (miss != "") miss += ",";
					miss += qcc::I32ToString(n);
				}
			}
			else {
				int n = (*c)->snum_end;

				while (++n <= nSNum) {
					if (miss != "") miss += ",";
					miss += qcc::I32ToString(n);
				}
			}
		}
		if (!miss.empty()) m_pMob->SendData(b->first.data(), time(NULL), ACT_MISSING, m_pMob->GetSessionID(), miss.data(), miss.size());
		else if (sJoiner && b->first == sJoiner) m_pMob->SendData(sJoiner, time(NULL), ACT_NO_MISSING, m_pMob->GetSessionID(), 0, 0);
	}
}

void CALLBACK fnMissingCheck(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR idEvent, DWORD /*dwTime*/)
{
	KillTimer(NULL, idEvent);
	
	gpMob->MissingCheck();
}

BOOL CSender::SetMissingTimer()
{
	mReceive::iterator b; sReceive::reverse_iterator c; 

	for (b = m_mReceives.begin(); b != m_mReceives.end(); b++) {
		for (c = b->second.rbegin(); c != b->second.rend(); c++) {
			if ((*c)->data.z) {
				int sn = (*c++)->snum - 1; 
				if ((c == b->second.rend() && sn > 0) || (c != b->second.rend() && sn != (*c)->snum_end)) {
					SetTimer(NULL, TM_MISSING_CHECK, INT_MISSING_CHECK, &fnMissingCheck);
					return TRUE;
				}
			}
			else break;
		}
	}

	return FALSE;
}
