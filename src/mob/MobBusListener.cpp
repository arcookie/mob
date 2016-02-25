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

#include "MobBusListener.h"
#include "MobClient.h"


void MobBusListener::LostAdvertisedName(const char* name, TransportMask transport, const char* namePrefix)
{
	QCC_UNUSED(namePrefix);
	printf("Got LostAdvertisedName for %s from transport 0x%x\nsqlite> ", name, transport);
}
void MobBusListener::NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
{
	printf("NameOwnerChanged: name=%s, oldOwner=%s, newOwner=%s\nsqlite> ", busName, previousOwner ? previousOwner : "<none>",
		newOwner ? newOwner : "<none>");
}
bool MobBusListener::AcceptSessionJoiner(SessionPort sessionPort, const char* joiner, const SessionOpts& opts)
{
	if (sessionPort != MOB_PORT) {
		printf("Rejecting join attempt on non-mob session port %d\nsqlite> ", sessionPort);
		return false;
	}

	printf("Accepting join session request from %s (opts.proximity=%x, opts.traffic=%x, opts.transports=%x)\nsqlite> ",
		joiner, opts.proximity, opts.traffic, opts.transports);
	return true;
}

void MobBusListener::SessionMemberAdded(SessionId sessionId, const char* uniqueName) {
	printf("SessionMemberAdded with %s (id=%d)\nsqlite> ", uniqueName, sessionId);
}

void MobBusListener::SessionMemberRemoved(SessionId sessionId, const char* uniqueName) {
	printf("SessionMemberRemoved with %s (id=%d)\nsqlite> ", uniqueName, sessionId);
}


void MobBusListener::SetMob(CAlljoynMob * pMob)
{
	m_pBus = pMob->m_pBus;
	m_pMob = pMob;
}

void MobBusListener::FoundAdvertisedName(const char* name, TransportMask transport, const char* namePrefix)
{
	CMobClient * pMob = (CMobClient *)m_pMob;

	printf("FoundAdvertisedName(name='%s', transport = 0x%x, prefix='%s')\n", name, transport, namePrefix);

	if (!pMob->GetSessionHost()) {
		SessionId id;
		const char* convName = name + strlen(NAME_PREFIX);
		printf("Discovered mob conversation: \"%s\"\n", convName);

		/* Join the conversation */
		/* Since we are in a callback we must enable concurrent callbacks before calling a synchronous method. */
		pMob->SetSessionHost(name);
		m_pBus->EnableConcurrentCallbacks();
		SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
		QStatus status = m_pBus->JoinSession(name, MOB_PORT, this, id, opts);
		if (ER_OK == status) {
			m_pMob->SetSessionID(id);
			printf("Joined conversation \"%s\"\n", convName);
		}
		else {
			printf("JoinSession failed (status=%s)\n", QCC_StatusText(status));
		}
		uint32_t timeout = 20;
		status = m_pBus->SetLinkTimeout(m_pMob->GetSessionID(), timeout);
		if (ER_OK == status) {
			printf("Set link timeout to %d\nsqlite> ", timeout);
		}
		else {
			printf("Set link timeout failed\nsqlite> ");
		}
		pMob->SetJoinComplete(true);
	}
}

void MobBusListener::SessionJoined(SessionPort sessionPort, SessionId id, const char* joiner)
{
	QCC_UNUSED(sessionPort);

	m_pMob->SetJoinInfo(id, joiner);
	printf("SessionJoined with %s (id=%d)\n", joiner, id);
	m_pBus->EnableConcurrentCallbacks();
	uint32_t timeout = 20;
	QStatus status = m_pBus->SetLinkTimeout(m_pMob->GetSessionID(), timeout);
	if (ER_OK == status) {
		printf("Set link timeout to %d\nsqlite> ", timeout);
	}
	else {
		printf("Set link timeout failed\nsqlite> ");
	}
}