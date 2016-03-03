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

#ifndef _SENDER_H_
#define _SENDER_H_

#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusObject.h>
#include <vector>
#include <map>
#include "mob.h"

using namespace ajn;

#define SEND_BUF		(8192 - sizeof(int) * 2)
#define MAX_URI	1024

#define TRAIN_MARK_1	0x34533454
#define TRAIN_MARK_2	0x34531454
#define TRAIN_MARK_3	0x32533454
#define TRAIN_MARK_4	0x34533054
#define TRAIN_MARK_5	0x54531454
#define TRAIN_MARK_6	0x32533414
#define TRAIN_MARK_END	6

#define TRAIN_EXTRA_LEN	128

#define TRAIN_HEADER(a)	int __o__[TRAIN_MARK_END] = {TRAIN_MARK_1,TRAIN_MARK_2,TRAIN_MARK_3,TRAIN_MARK_4,TRAIN_MARK_5,TRAIN_MARK_6,}; memcpy((char *)(a), (char *)__o__, sizeof(__o__));

#define IS_TRAIN_HEADER(a)	\
	(((int*)a)[0] == TRAIN_MARK_1 && ((int*)a)[1] == TRAIN_MARK_2 && ((int*)a)[2] == TRAIN_MARK_3 && \
	((int*)a)[3] == TRAIN_MARK_4 && ((int*)a)[4] == TRAIN_MARK_5 && ((int*)a)[5] == TRAIN_MARK_6)

#define MOB_SERVICE_INTERFACE_NAME "org.alljoyn.bus.arcookie.mob"
#define MOB_SERVICE_OBJECT_PATH "/mobService"
#define MOB_PORT 27

typedef struct {
	int marks[TRAIN_MARK_END];
	int aid;  // action id
	int wid;  // doc id
	int action; // 0 data, 1 file list 2 file list req 3 file
	int chain;
	char extra[TRAIN_EXTRA_LEN];
} TRAIN_HEADER;

class TRAIN {
public:
	TRAIN() {
		length = 0;
		strInit(&body);
	}
	~TRAIN() {
		strFree(&body);
	}

	int aid;  // action id
	int wid;  // doc id
	int action; // 0 data, 1 file list 2 file list req 3 file
	int length;
	char extra[TRAIN_EXTRA_LEN];
	Block body;
};

typedef struct {
	char uri[MAX_URI];
	int mtime;
	long fsize;

} FILE_SEND_ITEM;

typedef struct {
	qcc::String uid;
	qcc::String uid_p;
	int snum;
	int snum_p;
	int sn_s;
	int sn_e;
	const char * data;
} RECEIVE;

typedef struct {
	qcc::String uid;
	int sn;
	int snum;
	qcc::String base;
	const char * data;
} APPLY;

struct CompareAPPLY
{
	bool operator()(APPLY const& _Left, APPLY const& _Right) const
	{
		return _Left.uid.compare(_Right.uid) < 0;
	}
};

typedef struct CompareAPPLY				compApply;
typedef std::set<APPLY, compApply>		sApplies;

typedef struct {
	int snum_p;
	qcc::String uid_p;
	sApplies applies;
} APPLIES;

typedef std::vector<APPLIES>				vApplies;
typedef std::map<qcc::String, vApplies>		mApplies;
typedef std::vector<RECEIVE>				vReceives;
typedef std::map<qcc::String, vReceives>	mReceives;

class CAlljoynMob;

class CSender : public BusObject {
public:

	CSender(CAlljoynMob * pMob, BusAttachment& bus, const char* path);
	~CSender();

	QStatus _Send(SessionId nSID, const char * sJoinName, int nChain, const char * pData, int nLength);

	void Apply(SessionId nSID);
	BOOL PushApply(vApplies & applies, const char * base, const char * uid_p, int snum_p, BOOL bFirst);
	QStatus SendData(const char * sJoinName, int nAID, int nAction, SessionId wid, const char * msg, int nLength, const char * pExtra = NULL, int nExtLen = 0);

	void MissingCheck();
	void Save(SessionId nSID, const char * sText, int nLength, const char * pExtra, int nExtLen);
	void OnRecvData(const InterfaceDescription::Member* member, const char* srcPath, Message& msg);

	virtual void GetProp(const InterfaceDescription::Member* /*member*/, Message& /*msg*/) {}
	virtual void SetProp(const InterfaceDescription::Member* /*member*/, Message& /*msg*/) {}

private:
	CAlljoynMob *						m_pMob;
	std::map<int, TRAIN>				m_mTrain;
	std::map<int, TRAIN>				m_mHangar;
	const InterfaceDescription::Member* m_pMobSignalMember;
	mReceives							m_mReceives;
};

#endif /* _SENDER_H_ */
