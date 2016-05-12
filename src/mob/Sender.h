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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// defines

#define TRAIN_MARK_1	0x34533454
#define TRAIN_MARK_2	0x34531454
#define TRAIN_MARK_3	0x32533454
#define TRAIN_MARK_4	0x34533054
#define TRAIN_MARK_5	0x54531454
#define TRAIN_MARK_6	0x32533414
#define TRAIN_MARK_END	6

#define TRAIN_EXTRA_LEN	sizeof(FILE_SEND_ITEM)

#define TRAIN_HEADER(a)	int __o__[TRAIN_MARK_END] = {TRAIN_MARK_1,TRAIN_MARK_2,TRAIN_MARK_3,TRAIN_MARK_4,TRAIN_MARK_5,TRAIN_MARK_6,}; memcpy((char *)(a), (char *)__o__, sizeof(__o__));

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// typedef

typedef struct {
	char uri[MAX_PATH];
	int mtime;
	long fsize;
} FILE_SEND_ITEM;

typedef struct {
	int marks[TRAIN_MARK_END];
	int footprint;
	int action;
	int chain;
	char extra[TRAIN_EXTRA_LEN];
} TRAIN_HEADER;

typedef struct {
	int footprint;
	int action;
	qcc::String path;
	char extra[TRAIN_EXTRA_LEN];
	Block body;
} TRAIN;

class SKEY {
public:
	SKEY() {}
	SKEY(int s, const char * j) { snum = s; joiner = j; }

	bool operator==(const SKEY &other) const { return snum == other.snum && joiner == other.joiner; }
	bool operator!=(const SKEY &other) const { return snum != other.snum || joiner != other.joiner; }

	int snum;
	qcc::String joiner;
};

class RECEIVE{
public:
	RECEIVE() {}

	void set(int sn, int sn_end) { snum = sn; snum_end = sn_end; }
	 
	int snum;
	int snum_end;
	SKEY prev;
	qcc::String base_table;
	Block data;
};

struct CompareRECEIVE
{
	bool operator()(RECEIVE const * _Left, RECEIVE const * _Right) const
	{
		return  _Left->snum_end < _Right->snum;
	}
};

typedef std::map<int, TRAIN>				mTrain;
typedef std::map<qcc::String, mTrain>		mTrains;
typedef std::set<RECEIVE*, CompareRECEIVE>	sReceive;  // key to compare is snum range
typedef std::map<qcc::String, sReceive>		mReceive;  // key is joiner name

class CAlljoynMob;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSender

class CSender : public BusObject {

public:
	virtual void GetProp(const InterfaceDescription::Member* /*member*/, Message& /*msg*/) {}
	virtual void SetProp(const InterfaceDescription::Member* /*member*/, Message& /*msg*/) {}

private:
	CAlljoynMob *	m_pMob;
	mReceive		m_mReceives;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Sender.cpp

public:
	CSender(CAlljoynMob * pMob, BusAttachment& bus, const char* sPath);
	QStatus SendFile(const char * sJoiner, int nFootPrint, int nAction, SessionId sessionId, FILE_SEND_ITEM * pFSI);
	QStatus SendData(const char * sJoiner, int nFootPrint, int nAction, SessionId sessionId, const char * msg, int nLength, const char * pExtra = NULL, int nExtLen = 0);

private:
	const InterfaceDescription::Member* m_pMobSignalMember;

	QStatus _Send(SessionId sessionId, const char * sSvrName, int nChain, const char * pData, int nLength);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// MissingCheck.cpp

public:
	BOOL SetMissingTimer();
	void MissingCheck(qcc::String sList = "");

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Receiver.cpp

public:
	~CSender();
	void SaveFileInfo(SessionId sessionId, const char * pJoiner, const char * sPath, const FILE_SEND_ITEM * pFSI);
	void Save(SessionId sessionId, const char * pJoiner, Block * pText, const char * pExtra, int nExtLen);
	void OnDataEnd(int footprint, const char * pJoiner);
	void OnRecvData(const InterfaceDescription::Member* pMember, const char* srcPath, Message& msg);

private:
	mTrains	m_mTrain;
	mTrains	m_mStation;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Applier.cpp

public:
	void Apply(SessionId sessionId, const char * pJoiner);
};

extern void file_uri_replace(SessionId sessionId, const char * pJoiner, std::string & data);

#endif /* _SENDER_H_ */
