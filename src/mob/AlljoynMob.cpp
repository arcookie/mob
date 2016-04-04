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

#include <Shlwapi.h>
#include "mob.h"
#include "Global.h"
#include "MobClient.h"
#include "MobServer.h"
#include "MobBusListener.h"


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAlljoynMob

CAlljoynMob::CAlljoynMob(const char * sSvrName)
{
	m_pBus = NULL;
	m_nSNum = 0;
	m_pSender = NULL;
	m_nSessionID = 0;
	m_pMainDB = NULL;
	m_pBackDB = NULL;
	m_pUndoDB = NULL;
	m_sSvrName = sSvrName;
}

CAlljoynMob::~CAlljoynMob() 
{
	if (m_pBus) {
		delete m_pBus;
		m_pBus = NULL;
	}
	CloseDB();
}

void CAlljoynMob::CloseDB()
{
	if (m_pMainDB) {
		sqlite3_exec(m_pMainDB, "DETACH aux;", 0, 0, 0);
		sqlite3_close(m_pMainDB);
		m_pMainDB = NULL;
	}
	if (m_pBackDB) {
		sqlite3_close(m_pBackDB);
		m_pBackDB = NULL;
	}
	if (m_pUndoDB) {
		sqlite3_close(m_pUndoDB);
		m_pUndoDB = NULL;
	}
}

sqlite3 * CAlljoynMob::OpenDB(const char *zFilename)
{
	CloseDB();

	if (sqlite3_open(zFilename, &m_pMainDB) == SQLITE_OK) {
		qcc::String b_path = get_unique_path(".db3");

		sqlite3_exec(m_pMainDB, "PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);

		if (sqlite3_open(b_path.data(), &m_pBackDB) == SQLITE_OK) {
			sqlite3_exec(m_pBackDB, "PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);
			EXECUTE_SQL_V(m_pMainDB, ("ATTACH %Q as aux;", b_path.data()));
		}
		if (sqlite3_open(get_unique_path(".db3").data(), &m_pUndoDB) == SQLITE_OK)
			sqlite3_exec(m_pUndoDB, "CREATE TABLE works (num INTEGER PRIMARY KEY AUTOINCREMENT DEFAULT 1, joiner CHAR(16), snum INT DEFAULT 1, base_table VARCHAR(64), undo TEXT, redo TEXT);PRAGMA synchronous=OFF;PRAGMA journal_mode=OFF;", 0, 0, 0);

		return m_pMainDB;
	}

	return NULL;
}

BOOL CAlljoynMob::SendSignal()
{
	BOOL bSend = FALSE;
	sqlite3_stmt *pStmt = NULL;
	qcc::String s = m_pBus->GetUniqueName();

	QUERY_SQL_V(m_pMainDB, pStmt, ("SELECT MAX(snum) AS n FROM works WHERE joiner = %Q;", s.data()),
		SYNC_SIGNAL ss;

		strcpy_s(ss.joiner, sizeof(ss.joiner), s.data());
		ss.snum = sqlite3_column_int(pStmt, 0);
		mSignals::iterator iter;

		for (iter = m_mSignals.begin(); iter != m_mSignals.end(); iter++) {
			if (iter->second) {
				alljoyn_send(m_nSessionID, iter->first.data(), ACT_SIGNAL, 0, 0, (const char *)&ss, sizeof(SYNC_SIGNAL));
				bSend = TRUE;
			}
		}

		break;
	);
	return bSend;
}

void CAlljoynMob::SetSignal(const char * sJoiner, bool bSignal) 
{ 
	if (sJoiner) m_mSignals[sJoiner] = bSignal;
	else {
		mSignals::iterator iter;

		for (iter = m_mSignals.begin(); iter != m_mSignals.end(); iter++) {
			iter->second = bSignal;
		}
	}
}

QStatus CAlljoynMob::Connect()
{
	QStatus status = ER_FAIL;

	if ((m_pBus = new BusAttachment("mob", true)) != NULL) {

		/* Create org.alljoyn.bus.arcookie.mob interface */
		InterfaceDescription* mobIntf = NULL;

		status = m_pBus->CreateInterface(MOB_SERVICE_INTERFACE_NAME, mobIntf);

		if (ER_OK == status) {
			mobIntf->AddSignal("Mob", "ay", "data", 0);
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
