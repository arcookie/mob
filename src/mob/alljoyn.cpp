/*
 * Copyright AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
static const char* CHAT_SERVICE_INTERFACE_NAME = "org.alljoyn.bus.samples.chat";
static const char* NAME_PREFIX = "org.alljoyn.bus.samples.chat.";
static const char* CHAT_SERVICE_OBJECT_PATH = "/chatService";
static const SessionPort CHAT_PORT = 27;

/* static data. */
static ajn::BusAttachment* s_bus = NULL;
static qcc::String s_advertisedName;
static qcc::String s_joinName;
static qcc::String s_sessionHost;
static SessionId s_sessionId = 0;
static bool s_joinComplete = false;
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
class ChatObject : public BusObject {
  public:

    ChatObject(BusAttachment& bus, const char* path) : BusObject(path), chatSignalMember(NULL)
    {
        QStatus status;

        /* Add the chat interface to this object */
        const InterfaceDescription* chatIntf = bus.GetInterface(CHAT_SERVICE_INTERFACE_NAME);
        assert(chatIntf);
        AddInterface(*chatIntf);

        /* Store the Chat signal member away so it can be quickly looked up when signals are sent */
        chatSignalMember = chatIntf->GetMember("Chat");
        assert(chatSignalMember);

        /* Register signal handler */
        status =  bus.RegisterSignalHandler(this,
                                            static_cast<MessageReceiver::SignalHandler>(&ChatObject::OnRecvData),
                                            chatSignalMember,
                                            NULL);

        if (ER_OK != status) {
            printf("Failed to register signal handler for ChatObject::Chat (%s)\n", QCC_StatusText(status));
        }
    }

	~ChatObject(){
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

			MsgArg chatArg("ay", l, pBuf);

			status = Signal(NULL, s_sessionId, *chatSignalMember, &chatArg, 1, 0, flags);

			delete[] pBuf;
		}

		return status;
	}

    /** Send a Chat signal */
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
		MsgArg chatArg("ay", sizeof(TRAIN_HEADER), &th);

		if ((status = Signal(NULL, s_sessionId, *chatSignalMember, &chatArg, 1, 0, flags)) == ER_OK && nLength > 0) {
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

    /** Receive a signal from another Chat client */
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
	  const InterfaceDescription::Member* chatSignalMember;
};

class MyBusListener : public BusListener, public SessionPortListener, public SessionListener {
    void FoundAdvertisedName(const char* name, TransportMask transport, const char* namePrefix)
    {
        printf("FoundAdvertisedName(name='%s', transport = 0x%x, prefix='%s')\n", name, transport, namePrefix);

        if (s_sessionHost.empty()) {
            const char* convName = name + strlen(NAME_PREFIX);
            printf("Discovered chat conversation: \"%s\"\n", convName);

            /* Join the conversation */
            /* Since we are in a callback we must enable concurrent callbacks before calling a synchronous method. */
            s_sessionHost = name;
            s_bus->EnableConcurrentCallbacks();
            SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
            QStatus status = s_bus->JoinSession(name, CHAT_PORT, this, s_sessionId, opts);
            if (ER_OK == status) {
                printf("Joined conversation \"%s\"\n", convName);
            } else {
                printf("JoinSession failed (status=%s)\n", QCC_StatusText(status));
            }
            uint32_t timeout = 20;
            status = s_bus->SetLinkTimeout(s_sessionId, timeout);
            if (ER_OK == status) {
                printf("Set link timeout to %d\nsqlite> ", timeout);
            } else {
                printf("Set link timeout failed\nsqlite> ");
            }
            s_joinComplete = true;
        }
    }
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
        if (sessionPort != CHAT_PORT) {
            printf("Rejecting join attempt on non-chat session port %d\nsqlite> ", sessionPort);
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
        s_bus->EnableConcurrentCallbacks();
        uint32_t timeout = 20;
        QStatus status = s_bus->SetLinkTimeout(s_sessionId, timeout);
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
};

/* More static data. */
static ChatObject* s_chatObj = NULL;
static MyBusListener s_busListener;

#ifdef __cplusplus
extern "C" {
#endif

/** Send usage information to stdout and exit with EXIT_FAILURE. */
static void Usage()
{
    printf("Usage: chat [-h] [-s <name>] | [-j <name>]\n");
    exit(EXIT_FAILURE);
}

/** Parse the the command line arguments. If a problem occurs exit via Usage(). */
static void ParseCommandLine(int argc, char** argv)
{
	std::vector<std::string> v;

	/* Parse command line args */
    for (int i = 1; i < argc; ++i) {
        if (0 == ::strcmp("-s", argv[i])) {
            if ((++i < argc) && (argv[i][0] != '-')) {
				if (asVector(argv[i], strlen(argv[i]), ":", v)) {
					s_advertisedName = NAME_PREFIX;
					s_advertisedName += v[0].data();
					s_doc_id = atoi(v[1].data());
					s_user_id = atoi(v[2].data());
					s_user_password = atoi(v[3].data());
				}
            } else {
                printf("Missing parameter for \"-s\" option\n");
                Usage();
            }
        } else if (0 == ::strcmp("-j", argv[i])) {
            if ((++i < argc) && (argv[i][0] != '-')) {
				if (asVector(argv[i], strlen(argv[i]), ":", v)) {
					s_joinName = NAME_PREFIX;
					s_joinName += v[0].data();
					s_doc_id = atoi(v[1].data());
					s_user_id = atoi(v[2].data());
					s_user_password = atoi(v[3].data());
				}
			}
			else {
                printf("Missing parameter for \"-j\" option\n");
                Usage();
            }
        } else if (0 == ::strcmp("-h", argv[i])) {
            Usage();
        } else {
            printf("Unknown argument \"%s\"\n", argv[i]);
            Usage();
        }
    }
}

/** Validate the data obtained from the command line. If invalid exit via Usage(). */
void ValidateCommandLine()
{
    /* Validate command line */
    if (s_advertisedName.empty() && s_joinName.empty()) {
        printf("Must specify either -s or -j\n");
        Usage();
    } else if (!s_advertisedName.empty() && !s_joinName.empty()) {
        printf("Cannot specify both -s  and -j\n");
        Usage();
    }
}

/** Create the interface, report the result to stdout, and return the result status. */
QStatus CreateInterface(void)
{
    /* Create org.alljoyn.bus.samples.chat interface */
    InterfaceDescription* chatIntf = NULL;
    QStatus status = s_bus->CreateInterface(CHAT_SERVICE_INTERFACE_NAME, chatIntf);

    if (ER_OK == status) {
        chatIntf->AddSignal("Chat", "ay",  "data", 0);
        chatIntf->Activate();
    } else {
        printf("Failed to create interface \"%s\" (%s)\n", CHAT_SERVICE_INTERFACE_NAME, QCC_StatusText(status));
    }

    return status;
}

/** Start the message bus, report the result to stdout, and return the status code. */
QStatus StartMessageBus(void)
{
    QStatus status = s_bus->Start();

    if (ER_OK == status) {
        printf("BusAttachment started.\n");
    } else {
        printf("Start of BusAttachment failed (%s).\n", QCC_StatusText(status));
    }

    return status;
}

/** Register the bus object and connect, report the result to stdout, and return the status code. */
QStatus RegisterBusObject(void)
{
    QStatus status = s_bus->RegisterBusObject(*s_chatObj);

    if (ER_OK == status) {
        printf("RegisterBusObject succeeded.\n");
    } else {
        printf("RegisterBusObject failed (%s).\n", QCC_StatusText(status));
    }

    return status;
}

/** Connect, report the result to stdout, and return the status code. */
QStatus ConnectBusAttachment(void)
{
    QStatus status = s_bus->Connect();

    if (ER_OK == status) {
        printf("Connect to '%s' succeeded.\n", s_bus->GetConnectSpec().c_str());
    } else {
        printf("Failed to connect to '%s' (%s).\n", s_bus->GetConnectSpec().c_str(), QCC_StatusText(status));
    }

    return status;
}

/** Request the service name, report the result to stdout, and return the status code. */
QStatus RequestName(void)
{
    QStatus status = s_bus->RequestName(s_advertisedName.c_str(), DBUS_NAME_FLAG_DO_NOT_QUEUE);

    if (ER_OK == status) {
        printf("RequestName('%s') succeeded.\n", s_advertisedName.c_str());
    } else {
        printf("RequestName('%s') failed (status=%s).\n", s_advertisedName.c_str(), QCC_StatusText(status));
    }

    return status;
}

/** Create the session, report the result to stdout, and return the status code. */
QStatus CreateSession(TransportMask mask)
{
    SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, mask);
    SessionPort sp = CHAT_PORT;
    QStatus status = s_bus->BindSessionPort(sp, opts, s_busListener);

    if (ER_OK == status) {
        printf("BindSessionPort succeeded.\n");
    } else {
        printf("BindSessionPort failed (%s).\n", QCC_StatusText(status));
    }

    return status;
}

/** Advertise the service name, report the result to stdout, and return the status code. */
QStatus AdvertiseName(TransportMask mask)
{
    QStatus status = s_bus->AdvertiseName(s_advertisedName.c_str(), mask);

    if (ER_OK == status) {
        printf("Advertisement of the service name '%s' succeeded.\n", s_advertisedName.c_str());
    } else {
        printf("Failed to advertise name '%s' (%s).\n", s_advertisedName.c_str(), QCC_StatusText(status));
    }

    return status;
}

/** Begin discovery on the well-known name of the service to be called, report the result to
   stdout, and return the result status. */
QStatus FindAdvertisedName(void)
{
    /* Begin discovery on the well-known name of the service to be called */
    QStatus status = s_bus->FindAdvertisedName(s_joinName.c_str());

    if (status == ER_OK) {
        printf("org.alljoyn.Bus.FindAdvertisedName ('%s') succeeded.\n", s_joinName.c_str());
    } else {
        printf("org.alljoyn.Bus.FindAdvertisedName ('%s') failed (%s).\n", s_joinName.c_str(), QCC_StatusText(status));
    }

    return status;
}

/** Wait for join session to complete, report the event to stdout, and return the result status. */
QStatus WaitForJoinSessionCompletion(void)
{
    unsigned int count = 0;

    while (!s_joinComplete && !s_interrupt) {
        if (0 == (count++ % 100)) {
            printf("Waited %u seconds for JoinSession completion.\n", count / 100);
        }

#ifdef _WIN32
        Sleep(10);
#else
        usleep(10 * 1000);
#endif
    }

    return s_joinComplete && !s_interrupt ? ER_OK : ER_ALLJOYN_JOINSESSION_REPLY_CONNECT_FAILED;
}

/** Take input from stdin and send it as a chat message, continue until an error or
 * SIGINT occurs, return the result status. */
int alljoyn_connect(int argc, char** argv)
{
	/* Install SIGINT handler. */
	signal(SIGINT, SigIntHandler);

	ParseCommandLine(argc, argv);
	ValidateCommandLine();

	QStatus status = AllJoynInit();

#ifdef ROUTER
	if (ER_OK == status) {
		status = AllJoynRouterInit();
		if (ER_OK != status) {
			AllJoynShutdown();
		}
	}
#endif

	if (ER_OK == status) {
		/* Create message bus */
		s_bus = new BusAttachment("chat", true);

		if (s_bus) {

			if (ER_OK == status) {
				status = CreateInterface();
			}

			if (ER_OK == status) {
				s_bus->RegisterBusListener(s_busListener);
			}

			if (ER_OK == status) {
				status = StartMessageBus();
			}

			/* Create the bus object that will be used to send and receive signals */
			s_chatObj = new ChatObject(*s_bus, CHAT_SERVICE_OBJECT_PATH);

			if (ER_OK == status) {
				status = RegisterBusObject();
			}

			if (ER_OK == status) {
				status = ConnectBusAttachment();
			}

			/* Advertise or discover based on command line options */
			if (!s_advertisedName.empty()) {
				/*
				* Advertise this service on the bus.
				* There are three steps to advertising this service on the bus.
				* 1) Request a well-known name that will be used by the client to discover
				*    this service.
				* 2) Create a session.
				* 3) Advertise the well-known name.
				*/
				if (ER_OK == status) {
					status = RequestName();
				}

				const TransportMask SERVICE_TRANSPORT_TYPE = TRANSPORT_ANY;

				if (ER_OK == status) {
					status = CreateSession(SERVICE_TRANSPORT_TYPE);
				}

				if (ER_OK == status) {
					status = AdvertiseName(SERVICE_TRANSPORT_TYPE);
				}
			}
			else {
				if (ER_OK == status) {
					status = FindAdvertisedName();
				}

				if (ER_OK == status) {
					status = WaitForJoinSessionCompletion();
				}
			}
		}
	}
	else {
		status = ER_OUT_OF_MEMORY;
	}

	return (int)status;
}

void alljoyn_disconnect(void)
{
	if (s_bus) {
		/* Cleanup */
		delete s_bus;
		s_bus = NULL;
	}

#ifdef ROUTER
	AllJoynRouterShutdown();
#endif
	AllJoynShutdown();
}

int alljoyn_send(int nDocID, char * sText, int nLength)
{
	time_t aid = time(NULL);
	int ret = s_chatObj->SendData(aid, ACT_DATA, nDocID, sText, nLength);

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
		if (len > 0) ret = s_chatObj->SendData(aid, ACT_FLIST, nDocID, data, len);
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

#ifdef __cplusplus
}
#endif
