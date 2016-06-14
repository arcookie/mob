
// LeapHandle.cpp : ���� ���α׷��� ���� Ŭ���� ������ �����մϴ�.
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


// CLeapHandleApp ����

CLeapHandleApp::CLeapHandleApp()
{
	// TODO: ���⿡ ���� �ڵ带 �߰��մϴ�.
	// InitInstance�� ��� �߿��� �ʱ�ȭ �۾��� ��ġ�մϴ�.
}


// ������ CLeapHandleApp ��ü�Դϴ�.

CLeapHandleApp theApp;


// CLeapHandleApp �ʱ�ȭ

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

