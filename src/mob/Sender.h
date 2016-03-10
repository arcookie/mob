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
#include "block.h"

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

#define TM_MISSING_CHECK	1
#define TM_SEND_SIGNAL		2
#define INT_MISSING_CHECK	2000
#define INT_SEND_SIGNAL		5000

typedef struct {
	int marks[TRAIN_MARK_END];
	int footprint;  // 
	int action; // 0 data, 1 file list 2 file list req 3 file
	int chain;
	char extra[TRAIN_EXTRA_LEN];
} TRAIN_HEADER;

typedef struct TRAIN {
	int footprint;  // 
	int action; // 0 data, 1 file list 2 file list req 3 file
	char extra[TRAIN_EXTRA_LEN];
	Block body;
};

typedef struct {
	char uri[MAX_URI];
	int mtime;
	long fsize;
} FILE_SEND_ITEM;

typedef struct {
	int session_id;
	int mtime;
	long fsize;
	qcc::String uri;
	qcc::String path;
	qcc::String joiner;
} FILE_RECV_ITEM;

typedef struct {
	int snum;
	int snum_prev;
	int auto_inc_start;
	int auto_inc_end;
	qcc::String joiner;
	qcc::String joiner_prev;
	std::string data;
} RECEIVE;

typedef struct {
	int snum;
	int auto_inc;
	qcc::String joiner;
	qcc::String base_table;
	const char * data;
} APPLY;

struct CompareAPPLY
{
	bool operator()(APPLY const& _Left, APPLY const& _Right) const
	{
		return _Left.joiner.compare(_Right.joiner) < 0;
	}
};

typedef struct CompareAPPLY				compApply;
typedef std::set<APPLY, compApply>		sApplies;

typedef struct {
	int snum_prev;
	qcc::String joiner_prev;
	sApplies applies;
} APPLIES;

typedef std::map<int, TRAIN>				mTrain;
typedef std::map<qcc::String, mTrain>		mTrains;
typedef std::vector<APPLIES>				vApplies;
typedef std::map<qcc::String, vApplies>		mApplies;
typedef std::vector<RECEIVE>				vReceives;
typedef std::map<qcc::String, vReceives>	mReceives;
typedef std::vector<FILE_RECV_ITEM>			vRecvFiles;

class CAlljoynMob;

class CSender : public BusObject {
public:

	CSender(CAlljoynMob * pMob, BusAttachment& bus, const char* sPath);

	QStatus _Send(SessionId sessionId, const char * sSvrName, int nChain, const char * pData, int nLength);

	void Apply(SessionId sessionId);
	BOOL PushApply(vApplies & applies, const char * sTable, const char * sJoinerPrev, int nSNumPrev, BOOL bFirst);
	QStatus SendFile(const char * sJoiner, int nFootPrint, int nAction, SessionId sessionId, LPCSTR sPath);
	QStatus SendData(const char * sJoiner, int nFootPrint, int nAction, SessionId sessionId, const char * msg, int nLength, const char * pExtra = NULL, int nExtLen = 0);

	void MissingCheck();
	void MissingCheck(const char * sJoiner, int nSNum);
	const char * GetLocalPath(SessionId sessionId, const char * pJoiner, const char * sURI);
	void Save(SessionId sessionId, const char * pJoiner, Block * pText, const char * pExtra, int nExtLen);
	void OnRecvData(const InterfaceDescription::Member* pMember, const char* srcPath, Message& msg);

	virtual void GetProp(const InterfaceDescription::Member* /*member*/, Message& /*msg*/) {}
	virtual void SetProp(const InterfaceDescription::Member* /*member*/, Message& /*msg*/) {}

private:
	CAlljoynMob *						m_pMob;
	mTrains								m_mTrain;
	mTrains								m_mStation;
	mReceives							m_mReceives;
	const InterfaceDescription::Member* m_pMobSignalMember;
};

#endif /* _SENDER_H_ */
