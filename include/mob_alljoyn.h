
#ifndef _MOB_ALLJOYN_H_
#define _MOB_ALLJOYN_H_

#define ACT_DATA		0
#define ACT_FLIST		1
#define ACT_FLIST_REQ	2
#define ACT_FILE		3

/*
** Make sure we can call this stuff from C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

	typedef int(*fnSendHandler)(const char * sText, int nLength);

	void alljoyn_set_handler(fnSendHandler fnProc);
	void alljoyn_disconnect(void);
	int alljoyn_connect(int argc, char** argv);

	int alljoyn_send(int nDocID, char * sText, int nLength);

	int alljoyn_is_server();

	int alljoyn_doc_id();
	int alljoyn_user_id();
	const char * alljoyn_user_password();

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif /* _MOB_ALLJOYN_H_ */
