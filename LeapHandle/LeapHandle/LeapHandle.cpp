
// LeapHandle.cpp : 응용 프로그램에 대한 클래스 동작을 정의합니다.
//

#include "stdafx.h"
#include "LeapHandle.h"
#include "LeapHandleDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CLeapHandleApp

BEGIN_MESSAGE_MAP(CLeapHandleApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CLeapHandleApp 생성

CLeapHandleApp::CLeapHandleApp()
{
	// TODO: 여기에 생성 코드를 추가합니다.
	// InitInstance에 모든 중요한 초기화 작업을 배치합니다.
}


// 유일한 CLeapHandleApp 개체입니다.

CLeapHandleApp theApp;


// CLeapHandleApp 초기화

BOOL CLeapHandleApp::InitInstance()
{
	HANDLE	hMutex = CreateMutex(NULL, TRUE, L"PreventSecondInstanceOfLeapHandle");
	if (GetLastError() == ERROR_ALREADY_EXISTS) return FALSE;

	CWinApp::InitInstance();

	CLeapHandleDlg dlg;
	m_pMainWnd = &dlg;

	dlg.DoModal();

	if (hMutex)
	{
		ReleaseMutex(hMutex);
		CloseHandle(hMutex);
		hMutex = NULL;
	}

	return FALSE;
}

