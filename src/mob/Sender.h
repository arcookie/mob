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

using namespace ajn;

#define ACT_DATA		0
#define ACT_FLIST		1
#define ACT_FLIST_REQ	2
#define ACT_FILE		3
#define ACT_MISSING		4
#define ACT_END			5

#define SEND_BUF		(8192 - sizeof(int) * 2)
#define MAX_URI	1024

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

#define MOB_SERVICE_INTERFACE_NAME "org.alljoyn.bus.arcookie.mob"
#define MOB_SERVICE_OBJECT_PATH "/mobService"
#define MOB_PORT 27

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

typedef struct {
	std::string uid;
	std::string uid_p;
	int snum_s;
	int snum_e;
	int snum_p;
	std::string base;
	const char * data;
} RECEIVE;

typedef struct {
	std::string uid;
	int snum;
	std::string base;
	const char * data;
} APPLY;

typedef std::vector<APPLY> vApplies;
typedef std::vector<RECEIVE> vReceives;

class CAlljoynMob;

class CSender : public BusObject {
public:

	CSender(CAlljoynMob * pMob, BusAttachment& bus, const char* path);
	~CSender();

	QStatus _Send(const char * sJoinName, int nChain, const char * pData, int nLength);

	void Apply(int nDocID);
	QStatus SendData(const char * sJoinName, int nAID, int nAction, int wid, const char * msg, int nLength);

	void MissingCheck();
	void Save(int nDocID, const char * sText, int nLength);
	/** Receive a signal from another mob client */
	void OnRecvData(const InterfaceDescription::Member* member, const char* srcPath, Message& msg);
	virtual void GetProp(const InterfaceDescription::Member* /*member*/, Message& /*msg*/) {}
	virtual void SetProp(const InterfaceDescription::Member* /*member*/, Message& /*msg*/) {}

private:
	CAlljoynMob *						m_pMob;
	std::map<int, TRAIN>				m_mTrain;
	std::map<int, TRAIN>				m_mHangar;
	const InterfaceDescription::Member* m_pMobSignalMember;
	vReceives							m_vReceives;
};

#endif /* _SENDER_H_ */
