
#ifndef _MOB_ALLJOYN_H_
#define _MOB_ALLJOYN_H_

#define ACT_DATA		0
#define ACT_FLIST		1
#define ACT_FLIST_REQ	2
#define ACT_FILE		3
#define ACT_OMITTED		4
#define ACT_END			5

#define NAME_PREFIX	"org.alljoyn.bus.arcookie.mob."

/*
** Make sure we can call this stuff from C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

	void alljoyn_disconnect(void);
	int alljoyn_connect(const char * advertisedName, const char * joinName);

	int alljoyn_send(int nDocID, char * sText, int nLength);

	int alljoyn_session_id();
	const char * alljoyn_join_name();

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif /* _MOB_ALLJOYN_H_ */
