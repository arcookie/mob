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

#include "MobServer.h"

QStatus CMobServer::Connect()
{
	QStatus status = CAlljoynMob::Connect();

	if (ER_OK == status) {
		status = m_pBus->RequestName(m_sSvrName.data(), DBUS_NAME_FLAG_DO_NOT_QUEUE);

		if (ER_OK == status) {
			printf("RequestName('%s') succeeded.\n", m_sSvrName.data());
		}
		else {
			printf("RequestName('%s') failed (status=%s).\n", m_sSvrName.data(), QCC_StatusText(status));
		}
	}

	const TransportMask SERVICE_TRANSPORT_TYPE = TRANSPORT_ANY;

	if (ER_OK == status) {
		SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, SERVICE_TRANSPORT_TYPE);
		SessionPort sp = MOB_PORT;

		status = m_pBus->BindSessionPort(sp, opts, m_BusListener);

		if (ER_OK == status) {
			printf("BindSessionPort succeeded.\n");
		}
		else {
			printf("BindSessionPort failed (%s).\n", QCC_StatusText(status));
		}
	}

	if (ER_OK == status) {
		status = m_pBus->AdvertiseName(m_sSvrName.data(), SERVICE_TRANSPORT_TYPE);

		if (ER_OK == status) {
			printf("Advertisement of the service name '%s' succeeded.\n", m_sSvrName.data());
		}
		else {
			printf("Failed to advertise name '%s' (%s).\n", m_sSvrName.data(), QCC_StatusText(status));
		}
	}
	return status;
}
