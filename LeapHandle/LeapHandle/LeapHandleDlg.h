
// HncProbeDlg.h : ��� ����
//

#pragma once


// CHncProbeDlg ��ȭ ����
class CHncProbeDlg : public CDialogEx
{
// �����Դϴ�.
public:
	CHncProbeDlg(CWnd* pParent = NULL);	// ǥ�� �������Դϴ�.

// ��ȭ ���� �������Դϴ�.
	enum { IDD = IDD_HNCPROBE_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV �����Դϴ�.
	virtual void OnCancel() {}
	virtual void OnOK() {}


// �����Դϴ�.
protected:
	HICON m_hIcon;

	// ������ �޽��� �� �Լ�
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
};
