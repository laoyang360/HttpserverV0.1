// HttpServerDlg.h : header file
//

#if !defined(AFX_HTTPSERVERDLG_H__777A72AE_ACA4_4E53_951E_011E1FF1CC7F__INCLUDED_)
#define AFX_HTTPSERVERDLG_H__777A72AE_ACA4_4E53_951E_011E1FF1CC7F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CHttpServerDlg dialog

#include "HttpProtocol.h"
#include "ServerInfoDlg.h"


class CHttpServerDlg : public CDialog
{
// Construction
public:
	CHttpServerDlg(CWnd* pParent = NULL);	// standard constructor
	~CHttpServerDlg();

public:
	// Dialog Data
	//{{AFX_DATA(CHttpServerDlg)
	enum { IDD = IDD_HTTPSERVER_DIALOG };
	CListBox	m_ListLog;
	CListCtrl	m_FileReqListCtrl;
	UINT	m_nPort;
	CString	m_strRootDir;
	DWORD	m_dwConNums;
	DWORD	m_dwTransRate;
	DWORD   m_dwReceived;	
	DWORD   m_dwTransferred;
	//}}AFX_DATA
	
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CHttpServerDlg)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL
	
	// Implementation
protected:
	HICON m_hIcon;	
	// Generated message map functions
	//{{AFX_MSG(CHttpServerDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnStartStop();
	virtual void OnCancel();
	afx_msg void OnClearBut();
	afx_msg void OnClearFilereqBut();
	afx_msg void OnOpenLog();
	//}}AFX_MSG
	afx_msg void AddLog(WPARAM wParam, LPARAM lParam);
	afx_msg void ShowData(WPARAM wParam, LPARAM lParam);
	afx_msg void AddList(WPARAM wParam, LPARAM lParam);
	
	DECLARE_MESSAGE_MAP()

public:
	CString GetMyFilePath();
	void InitFileReqList();

public:
	CServerInfoDlg *pServerInfoDlg;	
	CHttpProtocol  *pHttpProtocol;
	bool m_bStart;
};


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_HTTPSERVERDLG_H__777A72AE_ACA4_4E53_951E_011E1FF1CC7F__INCLUDED_)
