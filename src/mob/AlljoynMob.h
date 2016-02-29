
#ifndef _ALLJOYN_MOB_H_
#define _ALLJOYN_MOB_H_

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

#include <alljoyn/BusAttachment.h>

#include "Sender.h"
#include "MobBusListener.h"

#define MOB_SERVICE_INTERFACE_NAME "org.alljoyn.bus.arcookie.mob"
#define MOB_SERVICE_OBJECT_PATH "/mobService"
#define MOB_PORT 27

class CSender;

class CAlljoynMob {
public:
	CAlljoynMob();
	~CAlljoynMob();

	virtual QStatus Init(const char * sJoinName);

	QStatus SendData(const char * sJoinName, int nAID, int nAction, SessionId wid, const char * msg, int nLength, const char * pExtra = NULL, int nExtLen = 0) { return m_pSender->SendData(sJoinName, nAID, nAction, wid, msg, nLength, pExtra, nExtLen); }

	const char * GetJoinName() { return m_sJoiner.data(); }
	void SetJoinName(const char * name) { m_sJoiner = name; }

	SessionId GetSessionID() { return m_nSessionID; }
	void SetSessionID(SessionId id) { m_nSessionID = id; }

	void EnableConcurrentCallbacks() { m_pBus->EnableConcurrentCallbacks(); }
	QStatus JoinSession(const char* sessionHost, SessionPort sessionPort, SessionListener* listener, SessionId& sessionId, SessionOpts& opts) {
		return m_pBus->JoinSession(sessionHost, sessionPort, listener, sessionId, opts);
	}
	QStatus SetLinkTimeout(SessionId sessionid, uint32_t& linkTimeout) { return m_pBus->SetLinkTimeout(sessionid, linkTimeout); }

protected:
	ajn::BusAttachment *	m_pBus;
	CSender*				m_pSender;
	qcc::String				m_sJoiner;
	SessionId				m_nSessionID;
	MobBusListener			m_BusListener;
};

extern CAlljoynMob * gpMob;

#endif /* _ALLJOYN_MOB_H_ */
