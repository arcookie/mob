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

#ifndef _MOB_CLIENT_H_
#define _MOB_CLIENT_H_

#include "AlljoynMob.h"

class CMobClient : public CAlljoynMob {
public:
	CMobClient(const char * sSvrName) : CAlljoynMob(sSvrName) { m_bJoinComplete = false; }

	virtual QStatus Connect();

	void SetSessionHost(const char * sSessionHost) { m_sSessionHost = sSessionHost; }
	void SetJoinComplete(bool bJoinComplete) { m_bJoinComplete = bJoinComplete; }
	const char * GetSessionHost() { return (m_sSessionHost.empty() ? NULL : m_sSessionHost.data()); }

private:
	qcc::String m_sSessionHost;
	bool		m_bJoinComplete;
};

#endif /* _MOB_CLIENT_H_ */
