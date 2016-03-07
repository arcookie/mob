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
#include "Sender.h"
#include "AlljoynMob.h"

int get_file_mtime(const char * path)
{
	HANDLE fh;

	if ((fh = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE) {
		FILETIME modtime;
		SYSTEMTIME stUTC;
		char buf[32];

		GetFileTime(fh, NULL, NULL, &modtime);

		CloseHandle(fh);

		FileTimeToSystemTime(&modtime, &stUTC);

		sprintf(buf, "%02d%02d%02d%02d%02d", stUTC.wMonth, stUTC.wDay, stUTC.wHour, stUTC.wMinute, stUTC.wSecond);

		return atoi(buf);
	}

	return 0;
}

long get_file_length(const char * path)
{
	FILE *fp;
	long sz;

	if ((fp = fopen(path, "rb")) != NULL) {
		fseek(fp, 0, SEEK_END);
		sz = ftell(fp);
		fclose(fp);
		return sz;
	}
	return 0L;
}

CSender::CSender(CAlljoynMob * pMob, BusAttachment& bus, const char* path) : m_pMob(pMob), BusObject(path), m_pMobSignalMember(NULL)
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

QStatus CSender::_Send(SessionId nSID, const char * sJoinName, int nChain, const char * pData, int nLength)
{
	uint8_t flags = 0;
	QStatus status = ER_FAIL;
	int l = (nLength > 0 ? nLength : 0) + sizeof(int);
	char * pBuf = new char[l];

	if (pBuf) {
		((int*)pBuf)[0] = nChain;

		if (nLength > 0) memcpy(pBuf + sizeof(int), pData, nLength);

		MsgArg mobArg("ay", l, pBuf);

		status = Signal(sJoinName, nSID, *m_pMobSignalMember, &mobArg, 1, 0, flags);

		delete[] pBuf;
	}

	return status;
}

QStatus CSender::SendData(const char * sJoinName, int nAID, int nAction, SessionId wid, const char * msg, int nLength, const char * pExtra, int nExtLen)
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

	if ((status = Signal(sJoinName, wid, *m_pMobSignalMember, &mobArg, 1, 0, flags)) == ER_OK && nLength > 0) {
		int l = nLength > SEND_BUF ? SEND_BUF : nLength;
		const char * p = msg;

		while ((status = _Send(wid, sJoinName, th.chain, p, l)) == ER_OK) {
			if (nLength > SEND_BUF) {
				nLength -= SEND_BUF;
				p += SEND_BUF;
				l = nLength > SEND_BUF ? SEND_BUF : nLength;
			}
			else break;
		}
		_Send(wid, sJoinName, th.chain, 0, -1);
	}

	return status;
}

void CALLBACK fnSendSignal(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR idEvent, DWORD /*dwTime*/)
{
	KillTimer(NULL, idEvent);

	mob_signal_db(alljoyn_session_id());
}

int alljoyn_send(SessionId nSID, const char * pJoiner, int nAction, char * sText, int nLength, const char * pExtra, int nExtLen)
{
	time_t aid = time(NULL);
	int ret = gpMob->SendData(pJoiner, aid, nAction, nSID, sText, nLength, pExtra, nExtLen);

	if (sText && ER_OK == ret) {
		int l;
		char * p = sText;
		Block data;
		char * p2;
		FILE_SEND_ITEM fsi;

		blkInit(&data);

		while ((p = strstr(p, "file://")) != NULL) {
			p += 7;
			if ((p2 = strchr(p, '\'')) != NULL && (l = (p2 - p)) > 0) {
				if (l < MAX_URI) {
					memcpy(fsi.uri, p, l);
					fsi.uri[l] = 0;
					fsi.mtime = get_file_mtime(fsi.uri);
					fsi.fsize = get_file_length(fsi.uri);

					memCat(&data, (char *)&fsi, sizeof(FILE_SEND_ITEM));
				}

				p = p2 + 1;
			}
			else p++;
		}
		if (data.nUsed > 0) {
			ret = gpMob->SendData(pJoiner, aid, ACT_FLIST, nSID, data.z, data.nUsed);
			SetTimer(NULL, TM_SEND_SIGNAL, INT_SEND_SIGNAL, &fnSendSignal);
		}

		blkFree(&data);
	}

	return ret;
}
