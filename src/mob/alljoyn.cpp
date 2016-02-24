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

#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/Init.h>
#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/ProxyBusObject.h>
#include <qcc/Log.h>
#include <qcc/String.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <time.h>
#include <vector>
#include <map>
#include "mob.h"
#include "util.h"
#include "mob_alljoyn.h"

#define SEND_BUF		(8192 - sizeof(int) * 2)

#define TRAIN_MARK_1	0x34533454
#define TRAIN_MARK_2	0x34531454
#define TRAIN_MARK_3	0x32533454
#define TRAIN_MARK_4	0x34533054
#define TRAIN_MARK_5	0x54531454
#define TRAIN_MARK_6	0x32533414
#define TRAIN_MARK_END	6

#define TRAIN_HEADER(a)	int __o__[TRAIN_MARK_END] = {TRAIN_MARK_1,TRAIN_MARK_2,TRAIN_MARK_3,TRAIN_MARK_4,TRAIN_MARK_5,TRAIN_MARK_6,}; memcpy((char *)(a), (char *)__o__, sizeof(__o__));

#define IS_TRAIN_HEADER(a)	\
	(((int*)a)[0] == TRAIN_MARK_1 && ((int*)a)[1] == TRAIN_MARK_2 && ((int*)a)[2] == TRAIN_MARK_3 && \
	((int*)a)[3] == TRAIN_MARK_4 && ((int*)a)[4] == TRAIN_MARK_5 && ((int*)a)[5] == TRAIN_MARK_6)

typedef struct {
	int marks[TRAIN_MARK_END];
	int aid;  // action id
	int wid;  // doc id
	int action; // 0 data, 1 file list 2 file list req 3 file
	int chain;
} TRAIN_HEADER;

class TRAIN {
public:
	TRAIN() {
		length = 0;
		body = NULL;
	}
	~TRAIN() {
		if (body) delete body;
	}

	int aid;  // action id
	int wid;  // doc id
	int action; // 0 data, 1 file list 2 file list req 3 file
	int length;
	char * body;
};

typedef struct {
	char uri[MAX_URI];
	int mtime;
	long long fsize;

} FILE_SEND_ITEM;

class WORKS {
public:
	WORKS(int u, int sn, const char * b, const char * d){
		uid = u;
		snum = sn;
		base = b;
		data = d;
	}

	int uid;
	int snum;
	std::string base;
	const char * data;
};

using namespace ajn;

/* constants. */
static const char* MOB_SERVICE_INTERFACE_NAME = "org.alljoyn.bus.arcookie.mob";
static const char* MOB_SERVICE_OBJECT_PATH = "/mobService";
static const SessionPort MOB_PORT = 27;

/* static data. */
static qcc::String s_advertisedName;
static qcc::String s_sessionHost;
static SessionId s_sessionId = 0;
static volatile sig_atomic_t s_interrupt = false;
static int s_doc_id = 0;
static int s_user_id = 0;
static qcc::String s_user_password;

void CDECL_CALL SigIntHandler(int sig)
{
    QCC_UNUSED(sig);
    s_interrupt = true;
}

BOOL asVector(LPCTSTR sText, int nLength, LPCTSTR cDelimit, std::vector<std::string> & stlRes)
{
	stlRes.clear();

	char *nexttoken = NULL;
	char *text = (char *)malloc(nLength + 1);

	if (text) {
		memcpy(text, sText, nLength);

		text[nLength] = 0;

		char *token = strtok_s(text, cDelimit, &nexttoken);

		while (token != NULL) {
			if (strlen(token) > 0) stlRes.push_back(token);
			token = strtok_s(NULL, cDelimit, &nexttoken);
		}

		free(text);
	}

	return !stlRes.empty();
}

/* Bus object */
class CAlljoynMob;

class CSender : public BusObject {
  public:

    CSender(BusAttachment& bus, const char* path) : BusObject(path), mobSignalMember(NULL)
    {
        QStatus status;

        /* Add the mob interface to this object */
        const InterfaceDescription* mobIntf = bus.GetInterface(MOB_SERVICE_INTERFACE_NAME);
        assert(mobIntf);
        AddInterface(*mobIntf);

        /* Store the mob signal member away so it can be quickly looked up when signals are sent */
        mobSignalMember = mobIntf->GetMember("Mob");
        assert(mobSignalMember);

        /* Register signal handler */
        status =  bus.RegisterSignalHandler(this,
                                            static_cast<MessageReceiver::SignalHandler>(&CSender::OnRecvData),
                                            mobSignalMember,
                                            NULL);

        if (ER_OK != status) {
            printf("Failed to register signal handler for CSender::Mob (%s)\n", QCC_StatusText(status));
        }
    }

	~CSender(){
		std::vector<WORKS *>::iterator iter;

		for (iter = m_vWorks.begin(); iter != m_vWorks.end(); iter++) {
			delete (*iter);
		}
	}

	QStatus _Send(int nChain, const char * pData, int nLength) {
		uint8_t flags = 0;
		QStatus status = ER_FAIL;
		int l = (nLength > 0 ? nLength : 0) + sizeof(int);
		char * pBuf = new char[l];

		if (pBuf) {
			((int*)pBuf)[0] = nChain;
			((int*)pBuf)[1] = nLength;

			if (nLength > 0) memcpy(pBuf + sizeof(int), pData, nLength);

			MsgArg mobArg("ay", l, pBuf);

			status = Signal(NULL, s_sessionId, *mobSignalMember, &mobArg, 1, 0, flags);

			delete[] pBuf;
		}

		return status;
	}

    /** Send a mob signal */
	QStatus SendData(int nAID, int nAction, int wid, const char * msg, int nLength) {
		TRAIN_HEADER th;
		uint8_t flags = 0;

		TRAIN_HEADER(th.marks);

		th.aid = nAID;
		th.wid = wid;
		th.action = nAction;
		th.chain = time(NULL);
		th.chain |= s_user_id; // indivisualize using by user id

		QStatus status;
		MsgArg mobArg("ay", sizeof(TRAIN_HEADER), &th);

		if ((status = Signal(NULL, s_sessionId, *mobSignalMember, &mobArg, 1, 0, flags)) == ER_OK && nLength > 0) {
			int l = nLength > SEND_BUF ? SEND_BUF : nLength;
			const char * p = msg;

			while ((status = _Send(th.chain, p, l)) == ER_OK) {
				if (nLength > SEND_BUF) {
					nLength -= SEND_BUF;
					p += SEND_BUF;
					l = nLength > SEND_BUF ? SEND_BUF : nLength;
				}
				else break;
			}
			_Send(th.chain, 0, -1);
		}

		return status;
    }

	BOOL IsOmitted(int nDocID)
	{
		return FALSE;
	}

	void FixOmitted(int nDocID)
	{
		//
	}

	const std::string & Save(int nDocID, const char * sText, int nLength, std::string & out)
	{
		const char * s = strstr(sText, "/*|");
		const char * e;

		if (s) {
			s += 3;
			if ((e = strstr(s, "|*/")) != NULL) {
				std::vector<std::string> v;

				if (asVector(s, e-s, "|", v) && v.size() == 3) {
					m_vWorks.push_back(new WORKS(atoi(v[0].data()), atoi(v[1].data()), v[2].data(), sText));
				}
				// 누락이 있으면 누락 전송용 타이머 시작
				// 
				if (IsOmitted(nDocID)) FixOmitted(nDocID);
			}
		}

		// 충돌이 존재하면 롤백

		// 보냄과 동시에 정리.

		return out;
	}

    /** Receive a signal from another mob client */
    void OnRecvData(const InterfaceDescription::Member* member, const char* srcPath, Message& msg)
    {
        QCC_UNUSED(member);
        QCC_UNUSED(srcPath);

		uint8_t * data;
		size_t size;
		std::map<int, TRAIN>::iterator iter;

		msg->GetArg(0)->Get("ay", &size, &data);

		if (IS_TRAIN_HEADER(data) && size == sizeof(TRAIN_HEADER)) {
			TRAIN_HEADER * pTH = (TRAIN_HEADER *)data;

			m_mTrain[pTH->chain].action = pTH->action;
			m_mTrain[pTH->chain].aid = pTH->aid;
			m_mTrain[pTH->chain].wid = pTH->wid;
		}
		else if ((iter = m_mTrain.find(((int*)data)[0])) != m_mTrain.end()){
			if (size == sizeof(int)) {
				switch (iter->second.action) {
				case ACT_OMITTED:
					// undo DB 에서 uid, snum 을 찾아 타킷 발송.
					break;
				case ACT_DATA:
					printf("%s:(%d) %ssqlite>", msg->GetSender(), iter->second.length, iter->second.body);
					m_mHangar[iter->second.aid].action = iter->second.action;
					m_mHangar[iter->second.aid].wid = iter->second.wid;
					memcpy(m_mHangar[iter->second.aid].body, iter->second.body, iter->second.length);
					m_mHangar[iter->second.aid].length = iter->second.length;
					// 
					break;
				case ACT_FLIST:
					if (sizeof(FILE_SEND_ITEM) % iter->second.length == 0) {
						int n = 0, len=0;
						char * data = 0;
						FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.body;

						while (n < iter->second.length) {
//								find pFSI->uri and cpy

							if (catmem(&data, pFSI, sizeof(FILE_SEND_ITEM)) == sizeof(FILE_SEND_ITEM)) len += sizeof(FILE_SEND_ITEM);

							n += sizeof(FILE_SEND_ITEM);
							pFSI++;
						}
						if (len > 0) SendData(iter->second.aid, ACT_FLIST_REQ, iter->second.wid, data, len); // special target most be assigned.
					}
					break;
				case ACT_FLIST_REQ:
					if (sizeof(FILE_SEND_ITEM) % iter->second.length == 0) {
						int n = 0;
						FILE_SEND_ITEM * pFSI = (FILE_SEND_ITEM *)iter->second.body;

						while (n < iter->second.length) {
							// load pFSI->uri as mem
							char * fmem = 0;
							int fsize = 0;
							SendData(iter->second.aid, ACT_FILE, iter->second.wid, fmem, fsize);// special target most be assigned.
							n += sizeof(FILE_SEND_ITEM);
							pFSI++;
						}
						SendData(iter->second.aid, ACT_END, iter->second.wid, 0, 0);// special target most be assigned.
					}
					break;
				case ACT_FILE:
					// save as file of data and register in file list table(or map).
					break;
				case ACT_END:
					{
						std::map<int, TRAIN>::iterator _iter;

						if ((_iter = m_mHangar.find(iter->second.aid)) != m_mHangar.end()){
							std::string s;

							// file:// 를 테이블을 이용하여 변환하여 DB 에 반영
							// apply 하고 머지후 전달할것.

							Save(_iter->second.wid, _iter->second.body, _iter->second.length, s);

							std::vector<WORKS *>::iterator __iter;

							for (__iter = m_vWorks.begin(); __iter != m_vWorks.end(); __iter++) {
								mob_apply(_iter->second.wid, (*__iter)->uid, (*__iter)->snum, (*__iter)->data);
							}
							m_mHangar.erase(_iter);
						}
					}
					break;
				}
				m_mTrain.erase(iter);
			}
			else {
				catmem(&(iter->second.body), (void *)(((int*)data) + 1), size - sizeof(int));
				iter->second.length += size - sizeof(int);
			}
		}
    }

	virtual void GetProp(const InterfaceDescription::Member* member, Message& msg) 
	{
		QCC_UNUSED(member);
		QCC_UNUSED(msg);
	}

	virtual void SetProp(const InterfaceDescription::Member* member, Message& msg)
	{
		QCC_UNUSED(member);
		QCC_UNUSED(msg);
	}

  private:
	  std::vector<WORKS *>			m_vWorks;
	  std::map<int, TRAIN>			m_mTrain;
	  std::map<int, TRAIN>			m_mHangar;
	  const InterfaceDescription::Member* mobSignalMember;
};

class MobBusListener : public BusListener, public SessionPortListener, public SessionListener {
	void FoundAdvertisedName(const char* name, TransportMask transport, const char* namePrefix);
    void LostAdvertisedName(const char* name, TransportMask transport, const char* namePrefix)
    {
        QCC_UNUSED(namePrefix);
        printf("Got LostAdvertisedName for %s from transport 0x%x\nsqlite> ", name, transport);
    }
    void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
    {
        printf("NameOwnerChanged: name=%s, oldOwner=%s, newOwner=%s\nsqlite> ", busName, previousOwner ? previousOwner : "<none>",
               newOwner ? newOwner : "<none>");
    }
    bool AcceptSessionJoiner(SessionPort sessionPort, const char* joiner, const SessionOpts& opts)
    {
        if (sessionPort != MOB_PORT) {
            printf("Rejecting join attempt on non-mob session port %d\nsqlite> ", sessionPort);
            return false;
        }

        printf("Accepting join session request from %s (opts.proximity=%x, opts.traffic=%x, opts.transports=%x)\nsqlite> ",
               joiner, opts.proximity, opts.traffic, opts.transports);
        return true;
    }

    void SessionJoined(SessionPort sessionPort, SessionId id, const char* joiner)
    {
        QCC_UNUSED(sessionPort);

        s_sessionId = id;
        printf("SessionJoined with %s (id=%d)\n", joiner, id);
		m_pBus->EnableConcurrentCallbacks();
        uint32_t timeout = 20;
		QStatus status = m_pBus->SetLinkTimeout(s_sessionId, timeout);
        if (ER_OK == status) {
            printf("Set link timeout to %d\nsqlite> ", timeout);
        } else {
            printf("Set link timeout failed\nsqlite> ");
        }
    }
	virtual void SessionMemberAdded(SessionId sessionId, const char* uniqueName) {
		printf("SessionMemberAdded with %s (id=%d)\nsqlite> ", uniqueName, sessionId);
	}

	/**
	* Called by the bus when a member of a multipoint session is removed.
	*
	* @param sessionId     Id of session whose member(s) changed.
	* @param uniqueName    Unique name of member who was removed.
	*/
	virtual void SessionMemberRemoved(SessionId sessionId, const char* uniqueName) {
		printf("SessionMemberRemoved with %s (id=%d)\nsqlite> ", uniqueName, sessionId);
	}

public:
	void SetMob(CAlljoynMob * pMob);

private:
	CAlljoynMob *			m_pMob;
	ajn::BusAttachment *	m_pBus;
};

class CAlljoynMob {
public:
	CAlljoynMob() {
		m_pBus = NULL;
		m_pSender = NULL;
		m_bJoinComplete = false;
	}

	~CAlljoynMob() {
		if (m_pBus) {
			/* Cleanup */
			delete m_pBus;
			m_pBus = NULL;
		}
	}

	void SetJoinComplete(bool joinComplete) { m_bJoinComplete = joinComplete; }

	virtual QStatus Init(const char * sJoinName) {
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
			m_pSender = new CSender(*m_pBus, MOB_SERVICE_OBJECT_PATH);

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

	QStatus SendData(int nAID, int nAction, int wid, const char * msg, int nLength) {
		return m_pSender->SendData(nAID, nAction, wid, msg, nLength);
	}

	ajn::BusAttachment *	m_pBus;
	CSender*				m_pSender;
	MobBusListener			m_BusListener;
	bool					m_bJoinComplete;
};

class CMobClient : public CAlljoynMob {
public:
	CMobClient() {}

	virtual QStatus Init(const char * sJoinName) {
		QStatus status = CAlljoynMob::Init(NULL);

		if (ER_OK == status) {
			/* Begin discovery on the well-known name of the service to be called */
			status = m_pBus->FindAdvertisedName(sJoinName);

			if (status == ER_OK) {
				printf("org.alljoyn.Bus.FindAdvertisedName ('%s') succeeded.\n", sJoinName);
			}
			else {
				printf("org.alljoyn.Bus.FindAdvertisedName ('%s') failed (%s).\n", sJoinName, QCC_StatusText(status));
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
};

class CMobServer : public CAlljoynMob {
public:
	CMobServer() {}
	virtual QStatus Init(const char * sAdvertiseName) {
		QStatus status = CAlljoynMob::Init(NULL);

		if (ER_OK == status) {
			status = m_pBus->RequestName(sAdvertiseName, DBUS_NAME_FLAG_DO_NOT_QUEUE);

			if (ER_OK == status) {
				printf("RequestName('%s') succeeded.\n", sAdvertiseName);
			}
			else {
				printf("RequestName('%s') failed (status=%s).\n", sAdvertiseName, QCC_StatusText(status));
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
			status = m_pBus->AdvertiseName(sAdvertiseName, SERVICE_TRANSPORT_TYPE);

			if (ER_OK == status) {
				printf("Advertisement of the service name '%s' succeeded.\n", sAdvertiseName);
			}
			else {
				printf("Failed to advertise name '%s' (%s).\n", sAdvertiseName, QCC_StatusText(status));
			}
		}
		return status;
	}
};

CAlljoynMob * gpMob = NULL;

void MobBusListener::SetMob(CAlljoynMob * pMob) 
{
	m_pBus = pMob->m_pBus;
	m_pMob = pMob; 
}

void MobBusListener::FoundAdvertisedName(const char* name, TransportMask transport, const char* namePrefix)
{
	printf("FoundAdvertisedName(name='%s', transport = 0x%x, prefix='%s')\n", name, transport, namePrefix);

	if (s_sessionHost.empty()) {
		const char* convName = name + strlen(NAME_PREFIX);
		printf("Discovered mob conversation: \"%s\"\n", convName);

		/* Join the conversation */
		/* Since we are in a callback we must enable concurrent callbacks before calling a synchronous method. */
		s_sessionHost = name;
		m_pBus->EnableConcurrentCallbacks();
		SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
		QStatus status = m_pBus->JoinSession(name, MOB_PORT, this, s_sessionId, opts);
		if (ER_OK == status) {
			printf("Joined conversation \"%s\"\n", convName);
		}
		else {
			printf("JoinSession failed (status=%s)\n", QCC_StatusText(status));
		}
		uint32_t timeout = 20;
		status = m_pBus->SetLinkTimeout(s_sessionId, timeout);
		if (ER_OK == status) {
			printf("Set link timeout to %d\nsqlite> ", timeout);
		}
		else {
			printf("Set link timeout failed\nsqlite> ");
		}
		m_pMob->SetJoinComplete(true);
	}
}

/** Take input from stdin and send it as a mob message, continue until an error or
 * SIGINT occurs, return the result status. */
int alljoyn_connect(const char * advertisedName, const char * joinName)
{
	/* Install SIGINT handler. */
	signal(SIGINT, SigIntHandler);

	if (advertisedName) {
		s_doc_id = 1;
		s_user_id = 1;
		gpMob = new CMobServer();
		return gpMob->Init(advertisedName);
	}
	else {
		s_doc_id = 1;
		s_user_id = 2;
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

int alljoyn_doc_id()
{
	return s_doc_id;
}

int alljoyn_user_id()
{
	return s_doc_id;
}

const char * alljoyn_user_password()
{
	return s_user_password.c_str();
}

int alljoyn_is_server()
{
	return s_advertisedName.empty() ? 0 : 1;
}
