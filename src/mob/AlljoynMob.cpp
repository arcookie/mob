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
#include <ShlObj.h>
#include <alljoyn/Init.h>
#include "mob.h"
#include "MobClient.h"
#include "MobServer.h"
#include "MobBusListener.h"

qcc::String gWPath;
HANDLE gMutex = NULL;
CAlljoynMob * gpMob = NULL;

CAlljoynMob::CAlljoynMob() 
{
	m_pBus = NULL;
	m_pSender = NULL;
	m_nSessionID = 0;
}

CAlljoynMob::~CAlljoynMob() 
{
	if (m_pBus) {
		delete m_pBus;
		m_pBus = NULL;
	}
}

QStatus CAlljoynMob::Init(const char * sJoinName)
{
	QStatus status = AllJoynInit();

#ifdef ROUTER
	if (ER_OK == status) {
		status = AllJoynRouterInit();
		if (ER_OK != status) {
			AllJoynShutdown();
		}
	}
#endif

	if ((m_pBus = new BusAttachment("mob", true)) != NULL) {

		/* Create org.alljoyn.bus.arcookie.mob interface */
		InterfaceDescription* mobIntf = NULL;

		status = m_pBus->CreateInterface(MOB_SERVICE_INTERFACE_NAME, mobIntf);

		if (ER_OK == status) {
			mobIntf->AddSignal("mob", "ay", "data", 0);
			mobIntf->Activate();
		}
		else {
			printf("Failed to create interface \"%s\" (%s)\n", MOB_SERVICE_INTERFACE_NAME, QCC_StatusText(status));
		}

		if (ER_OK == status) {
			m_BusListener.SetMob(this);
			m_pBus->RegisterBusListener(m_BusListener);
		}

		if (ER_OK == status) {
			status = m_pBus->Start();

			if (ER_OK == status) {
				printf("BusAttachment started.\n");
			}
			else {
				printf("Start of BusAttachment failed (%s).\n", QCC_StatusText(status));
			}
		}

		/* Create the bus object that will be used to send and receive signals */
		m_pSender = new CSender(this, *m_pBus, MOB_SERVICE_OBJECT_PATH);

		if (ER_OK == status) {
			status = m_pBus->RegisterBusObject(*m_pSender);

			if (ER_OK == status) {
				printf("RegisterBusObject succeeded.\n");
			}
			else {
				printf("RegisterBusObject failed (%s).\n", QCC_StatusText(status));
			}
		}

		if (ER_OK == status) {
			status = m_pBus->Connect();

			if (ER_OK == status) {
				printf("Connect to '%s' succeeded.\n", m_pBus->GetConnectSpec().c_str());
			}
			else {
				printf("Failed to connect to '%s' (%s).\n", m_pBus->GetConnectSpec().c_str(), QCC_StatusText(status));
			}
		}
	}
	return status;
}

void CAlljoynMob::MissingCheck()
{
	m_pSender->MissingCheck();
}

qcc::String GetVirtualStorePath()
{
	CHAR buffer[MAX_PATH];

	SHGetSpecialFolderPath(NULL, buffer, CSIDL_LOCAL_APPDATA, 0);

	return qcc::String(buffer) + "\\mob\\";
}

void remove_dir(qcc::String wFile)
{
	HANDLE				hFile;
	WIN32_FIND_DATA		nFileSizeLow;
	qcc::String			sFile;

	if ((hFile = FindFirstFile((wFile + "*.*").data(), &nFileSizeLow)) != INVALID_HANDLE_VALUE){
		do {
			sFile = nFileSizeLow.cFileName;
			if (!sFile.empty() && sFile != "." && sFile != ".."){
				sFile = wFile + sFile;
				if (nFileSizeLow.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
					remove_dir(sFile + "\\");
					RemoveDirectory(sFile.data());
				}
				else {
					SetFileAttributes(sFile.data(), FILE_ATTRIBUTE_NORMAL);
					DeleteFile(sFile.data());
				}
			}
		} while (FindNextFile(hFile, &nFileSizeLow));

		FindClose(hFile);
	}
}

/** Take input from stdin and send it as a mob message, continue until an error or
* SIGINT occurs, return the result status. */
int alljoyn_connect(const char * advertisedName, const char * joinName)
{
	gWPath = GetVirtualStorePath();

	CreateDirectory(gWPath.data(), FALSE);

	gMutex = CreateMutex(NULL, TRUE, "PreverntSecondInstanceOfMob");
	if (GetLastError() != ERROR_ALREADY_EXISTS) remove_dir(gWPath);

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

	if (gMutex) {
		ReleaseMutex(gMutex);
		CloseHandle(gMutex);
	}
}

int alljoyn_session_id()
{
	return gpMob->GetSessionID();
}

const char * alljoyn_join_name()
{
	return gpMob->GetJoinName();
}

const char * get_writable_path()
{
	return gWPath.data();
}

int set_timer(int id, int elapse, TIMERPROC func)
{
	return SetTimer(NULL, id, elapse, func);
}

