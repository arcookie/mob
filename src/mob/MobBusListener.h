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

#ifndef _MOB_BUS_LISTENER_H_
#define _MOB_BUS_LISTENER_H_

#include <alljoyn/BusAttachment.h>

using namespace ajn;

class CAlljoynMob;

class MobBusListener : public BusListener, public SessionPortListener, public SessionListener {
public:
	void SessionJoined(SessionPort sessionPort, SessionId id, const char* joiner);
	void FoundAdvertisedName(const char* name, TransportMask transport, const char* namePrefix);
	bool AcceptSessionJoiner(SessionPort sessionPort, const char* joiner, const SessionOpts& opts);

	void SetMob(CAlljoynMob * pMob) { m_pMob = pMob; }
	void LostAdvertisedName(const char* name, TransportMask transport, const char* /*namePrefix*/) {
		printf("Got LostAdvertisedName for %s from transport 0x%x\nsqlite> ", name, transport);
	}
	void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)	{
		printf("NameOwnerChanged: name=%s, oldOwner=%s, newOwner=%s\nsqlite> ", busName, previousOwner ? previousOwner : "<none>",
			newOwner ? newOwner : "<none>");
	}
	virtual void SessionMemberAdded(SessionId sessionId, const char* uniqueName) {
		printf("SessionMemberAdded with %s (id=%d)\nsqlite> ", uniqueName, sessionId);
	}
	virtual void SessionMemberRemoved(SessionId sessionId, const char* uniqueName) {
		printf("SessionMemberRemoved with %s (id=%d)\nsqlite> ", uniqueName, sessionId);
	}

private:
	CAlljoynMob *			m_pMob;
};

#endif /* _MOB_BUS_LISTENER_H_ */
