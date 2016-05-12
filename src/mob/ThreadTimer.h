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

#ifndef _THREAD_TIMER_H_
#define _THREAD_TIMER_H_

#include <thread>
#include <atomic>

#define INT_MISSING_CHECK	2000
#define INT_SEND_SIGNAL		5000
#define INT_WEB_POLLING		5000

#define WEB_POLLING_URL	"http://www.arcookie.com/cmd.json"

class CThreadTimer
{
public:
	CThreadTimer(int nInterval, bool bOnce) { _SetTimer(nInterval, bOnce); }
	~CThreadTimer();

	void SetTimer() { _SetTimer(m_nInterval.load(), m_bOnce.load()); }
	void KillTimer();
	void ThreadProc();

	virtual void run() = 0;

private:
	std::thread			m_Thread;
	std::atomic_bool	m_bOnce;
	std::atomic_int		m_nCount;
	std::atomic_int		m_nInterval;

	void _SetTimer(int nInterval, bool bOnce);
};

class CMissingCheckTimer : public CThreadTimer {
public:
	CMissingCheckTimer() : CThreadTimer(INT_MISSING_CHECK, true) {}

	virtual void run();
};

class CSignalTimer : public CThreadTimer {
public:
	CSignalTimer() : CThreadTimer(INT_SEND_SIGNAL, false) {}

	virtual void run();
};

class CWebPollingTimer : public CThreadTimer {
public:
	CWebPollingTimer() : CThreadTimer(INT_WEB_POLLING, true) {}

	virtual void run();
};

extern CSignalTimer * gpSignalTimer;
extern CWebPollingTimer * gpWebPollingTimer;
extern CMissingCheckTimer * gpMissingCheckTimer;

#endif /* _THREAD_TIMER_H_ */
