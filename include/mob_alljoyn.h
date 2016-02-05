
#ifndef _MOB_ALLJOYN_H_
#define _MOB_ALLJOYN_H_

/*
** Make sure we can call this stuff from C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

	void alljoyn_disconnect(void);
	int alljoyn_connect(int argc, char** argv);

	int alljoyn_send(const char * sText);

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif /* _MOB_ALLJOYN_H_ */
