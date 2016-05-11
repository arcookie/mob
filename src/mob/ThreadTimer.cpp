#include <map>
#include "ThreadTimer.h"


void CThreadTimer::run()
{
	do {
		m_nCount = 0;
		while (m_nCount++ < m_nInterval) {
			_sleep(100);
		}
	//	if (m_nInterval == m_nCount) run();
	//	else break;
	} while (!m_bOnce.load() || m_nInterval != m_nCount);
}

void CThreadTimer::SetTimer(int nInterval, bool bOnce)
{
	if (!m_Thread.joinable()) m_Thread = std::thread(&CThreadTimer::run, this);
	else {
		m_nInterval = nInterval;
		m_nCount = 0;
		m_bOnce = bOnce;
	}
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

std::map<int, CThreadTimer*> gTimers;

void SetTimer(int nUID, int nInterval, bool bOnce) 
{
	std::map<int, CThreadTimer*>::iterator iter;

	if ((iter = gTimers.find(nUID)) != gTimers.end()) {
		iter->second->SetTimer(nInterval, bOnce);
	}
	else {
		gTimers[nUID] = new CThreadTimer(nInterval, bOnce);
 	}
}

void KillTimer(int nUID)
{
	std::map<int, CThreadTimer*>::iterator iter;

	if ((iter = gTimers.find(nUID)) != gTimers.end()) {
		iter->second->KillTimer();
	}
}