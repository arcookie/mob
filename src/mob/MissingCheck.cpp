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


void CSender::MissingCheck()
{
	std::map<qcc::String, qcc::String> miss;
	{
		qcc::String s;
		mReceives::iterator miter;
		vReceives::iterator viter;

		for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {
			s = "";
			for (viter = miter->second.begin(); viter != miter->second.end(); viter++) {
				if ((*viter)->auto_inc_start != (*viter)->auto_inc_end && (*viter)->auto_inc_end > 1 && std::find_if(miter->second.begin(), miter->second.end(), find_id((*viter)->joiner, (*viter)->auto_inc_end - 1)) == miter->second.end()) {
					if (s != "") s += ",";
					s += qcc::I32ToString((*viter)->auto_inc_end - 1);
				}
			}
			if (!s.empty()) miss[(*viter)->joiner] = s;
		}
	}
	{
		std::map<qcc::String, qcc::String>::iterator iter;

		for (iter = miss.begin(); iter != miss.end(); iter++) {
			if (!iter->second.empty()) m_pMob->SendData(iter->first.data(), time(NULL), ACT_MISSING, m_pMob->GetSessionID(), iter->second.data(), iter->second.size());
		}
	}
}

void CSender::MissingCheck(const char * sJoiner, int nSNum)
{
	BOOL bFind = FALSE;
	mReceives::iterator miter;
	vReceives::iterator viter;
	qcc::String s = qcc::I32ToString(nSNum);

	for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {
		for (viter = miter->second.begin(); viter != miter->second.end(); viter++) {
			if ((*viter)->auto_inc_start <= nSNum && (*viter)->auto_inc_end >= nSNum && (*viter)->joiner == sJoiner) {
				bFind = TRUE;
				break;
			}
		}
	}
	if (bFind) m_pMob->SendData(sJoiner, time(NULL), ACT_NO_MISSING, m_pMob->GetSessionID(), s.data(), s.size()); 
	else m_pMob->SendData(sJoiner, time(NULL), ACT_MISSING, m_pMob->GetSessionID(), s.data(), s.size());
}

void CALLBACK fnMissingCheck(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR idEvent, DWORD /*dwTime*/)
{
	KillTimer(NULL, idEvent);
	
	gpMob->MissingCheck();
}

BOOL CSender::StartMissingCheck()
{
	mReceives::iterator miter;
	vReceives::iterator viter;

	for (miter = m_mReceives.begin(); miter != m_mReceives.end(); miter++) {
		for (viter = miter->second.begin(); viter != miter->second.end(); viter++) {
			if ((*viter)->auto_inc_start != (*viter)->auto_inc_end && (*viter)->auto_inc_end > 1 && std::find_if(miter->second.begin(), miter->second.end(), find_id((*viter)->joiner, (*viter)->auto_inc_end - 1)) == miter->second.end()) {
				SetTimer(NULL, TM_MISSING_CHECK, INT_MISSING_CHECK, &fnMissingCheck);
				return TRUE;
			}
		}
	}
	return FALSE;
}

