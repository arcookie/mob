
// LeapHandle.h : PROJECT_NAME ���� ���α׷��� ���� �� ��� �����Դϴ�.
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH�� ���� �� ������ �����ϱ� ���� 'stdafx.h'�� �����մϴ�."
#endif

#include "resource.h"		// �� ��ȣ�Դϴ�.


// CHncProbeApp:
// �� Ŭ������ ������ ���ؼ��� LeapHandle.cpp�� �����Ͻʽÿ�.
//

class CHncProbeApp : public CWinApp
{
public:
	CHncProbeApp();

// �������Դϴ�.
public:
	virtual BOOL InitInstance();

// �����Դϴ�.

	DECLARE_MESSAGE_MAP()
};

extern CHncProbeApp theApp;