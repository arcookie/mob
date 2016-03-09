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
#include <string>
#include <qcc/StringUtil.h>

#include "mob.h"
#include "Global.h"
#include "Sender.h"
#include "AlljoynMob.h"

CSender::CSender(CAlljoynMob * pMob, BusAttachment& bus, const char* sPath) : m_pMob(pMob), BusObject(sPath), m_pMobSignalMember(NULL)
{
	QStatus status;

	/* Add the mob interface to this object */
	const InterfaceDescription* mobIntf = bus.GetInterface(MOB_SERVICE_INTERFACE_NAME);
	assert(mobIntf);
	AddInterface(*mobIntf);

	/* Store the mob signal member away so it can be quickly looked up when signals are sent */
	m_pMobSignalMember = mobIntf->GetMember("Mob");
	assert(m_pMobSignalMember);

	/* Register signal handler */
	status = bus.RegisterSignalHandler(this,
		static_cast<MessageReceiver::SignalHandler>(&CSender::OnRecvData),
		m_pMobSignalMember,
		NULL);

	if (ER_OK != status) {
		printf("Failed to register signal handler for CSender::Mob (%s)\n", QCC_StatusText(status));
	}
}

QStatus CSender::_Send(SessionId nSID, const char * sSvrName, int nChain, const char * pData, int nLength)
{
	uint8_t flags = 0;
	QStatus status = ER_FAIL;
	int l = (nLength > 0 ? nLength : 0) + sizeof(int);
	char * pBuf = new char[l];

	if (pBuf) {
		((int*)pBuf)[0] = nChain;

		if (nLength > 0) memcpy(pBuf + sizeof(int), pData, nLength);

		MsgArg mobArg("ay", l, pBuf);

		status = Signal(sSvrName, nSID, *m_pMobSignalMember, &mobArg, 1, 0, flags);

		delete[] pBuf;
	}

	return status;
}

QStatus CSender::SendData(const char * sSvrName, int nAID, int nAction, SessionId wid, const char * msg, int nLength, const char * pExtra, int nExtLen)
{
	TRAIN_HEADER th;
	uint8_t flags = 0;

	TRAIN_HEADER(th.marks);

	th.aid = nAID;
	th.wid = wid;
	th.action = nAction;
	th.chain = time(NULL);

	if (pExtra) memcpy(th.extra, pExtra, nExtLen);

	QStatus status;
	MsgArg mobArg("ay", sizeof(TRAIN_HEADER), &th);

	if ((status = Signal(sSvrName, wid, *m_pMobSignalMember, &mobArg, 1, 0, flags)) == ER_OK && nLength > 0) {
		int l = nLength > SEND_BUF ? SEND_BUF : nLength;
		const char * p = msg;

		while ((status = _Send(wid, sSvrName, th.chain, p, l)) == ER_OK) {
			if (nLength > SEND_BUF) {
				nLength -= SEND_BUF;
				p += SEND_BUF;
				l = nLength > SEND_BUF ? SEND_BUF : nLength;
			}
			else break;
		}
		_Send(wid, sSvrName, th.chain, 0, -1);
	}

	return status;
}
