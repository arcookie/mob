
// LeapHandle.h : PROJECT_NAME ���� ���α׷��� ���� �� ��� �����Դϴ�.
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH�� ���� �� ������ �����ϱ� ���� 'stdafx.h'�� �����մϴ�."
#endif

#include "resource.h"		// �� ��ȣ�Դϴ�.


// CLeapHandleApp:
// �� Ŭ������ ������ ���ؼ��� LeapHandle.cpp�� �����Ͻʽÿ�.
//

class CLeapHandleApp : public CWinApp
{
public:
	CLeapHandleApp();

// �������Դϴ�.
public:
	virtual BOOL InitInstance();

// �����Դϴ�.

	DECLARE_MESSAGE_MAP()
};

extern CLeapHandleApp theApp;