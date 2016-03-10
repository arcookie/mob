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

#include "MobClient.h"

volatile sig_atomic_t s_interrupt = false;

void CDECL_CALL SigIntHandler(int /*sig*/)
{
	s_interrupt = true;
}

QStatus CMobClient::Init(const char * sSvrName)
{
	/* Install SIGINT handler. */
	signal(SIGINT, SigIntHandler);

	QStatus status = CAlljoynMob::Init(NULL);

	if (ER_OK == status) {
		/* Begin discovery on the well-known name of the service to be called */
		status = m_pBus->FindAdvertisedName(sSvrName);

		if (status == ER_OK) {
			printf("org.alljoyn.Bus.FindAdvertisedName ('%s') succeeded.\n", sSvrName);
		}
		else {
			printf("org.alljoyn.Bus.FindAdvertisedName ('%s') failed (%s).\n", sSvrName, QCC_StatusText(status));
		}
	}

	if (ER_OK == status) {
		unsigned int count = 0;

		while (!m_bJoinComplete && !s_interrupt) {
			if (0 == (count++ % 100)) {
				printf("Waited %u seconds for JoinSession completion.\n", count / 100);
			}

#ifdef _WIN32
			Sleep(10);
#else
			usleep(10 * 1000);
#endif
		}
	}

	return (m_bJoinComplete && !s_interrupt ? ER_OK : ER_ALLJOYN_JOINSESSION_REPLY_CONNECT_FAILED);
}
