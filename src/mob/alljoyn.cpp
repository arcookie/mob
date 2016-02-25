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

#include <alljoyn/Init.h>
#include <time.h>
#include "mob.h"
#include "util.h"
#include "mob_alljoyn.h"
#include "Sender.h"
#include "MobClient.h"
#include "MobServer.h"

CAlljoynMob * gpMob = NULL;

/** Take input from stdin and send it as a mob message, continue until an error or
 * SIGINT occurs, return the result status. */
int alljoyn_connect(const char * advertisedName, const char * joinName)
{
	if (advertisedName) {
		gpMob = new CMobServer();
		return gpMob->Init(advertisedName);
	}
	else {
		gpMob = new CMobClient();
		return gpMob->Init(joinName);
	}
}

void alljoyn_disconnect(void)
{
	if (gpMob) {
		delete gpMob;
		gpMob = NULL;
	}

#ifdef ROUTER
	AllJoynRouterShutdown();
#endif
	AllJoynShutdown();
}

int alljoyn_send(int nDocID, char * sText, int nLength)
{
	time_t aid = time(NULL);
	int ret = gpMob->SendData(aid, ACT_DATA, nDocID, sText, nLength);

	if (ER_OK == ret) {
		int len = 0, l;
		char * p = sText;
		char * data = 0;
		char * p2;
		FILE_SEND_ITEM fsi;

		// ' inside of file:// most be urlencoded.
		while ((p = strstr(p, "file://")) != NULL) {
			p += 7;
			if ((p2 = strchr(p, '\'')) != NULL && (l = (p2 - p)) > 0) {
				if (l < MAX_URI) {
					memcpy(fsi.uri, p, l);
					fsi.uri[l] = 0;
					fsi.mtime = get_file_mtime(fsi.uri);
					fsi.fsize = get_file_length(fsi.uri);

					if (catmem(&data, &fsi, sizeof(FILE_SEND_ITEM)) == sizeof(FILE_SEND_ITEM)) len += sizeof(FILE_SEND_ITEM);
				}

				p = p2 + 1;
			}
			else p++;
		}
		if (len > 0) ret = gpMob->SendData(aid, ACT_FLIST, nDocID, data, len);
	}

	return ret;
}

int alljoyn_session_id()
{
	return gpMob->GetSessionID();
}

const char * alljoyn_join_name()
{
	return gpMob->GetJoinName();
}
