#pragma once

#include <thread>
#include <atomic>

class CThreadTimer
{
public:
	CThreadTimer(int nInterval, bool bOnce) { SetTimer(nInterval, bOnce); }
	~CThreadTimer();

	void SetTimer(int nInterval, bool bOnce);
	void KillTimer();

	void run();
private:
	std::thread			m_Thread;
	std::atomic_bool	m_bOnce;
	std::atomic_int		m_nCount;
	std::atomic_int		m_nInterval;
};

