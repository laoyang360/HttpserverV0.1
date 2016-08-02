#if !defined(AFX_SERVERINFODLG_H__949F2C8A_494C_45EA_AA69_DA1F579BED7E__INCLUDED_)
#define AFX_SERVERINFODLG_H__949F2C8A_494C_45EA_AA69_DA1F579BED7E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ServerInfoDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CServerInfoDlg dialog

class CServerInfoDlg : public CDialog
{
// Construction
public:
	CServerInfoDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CServerInfoDlg();

public:
	char* m_AppKey;
	char* m_PortKey;
	char* m_DirKey;

// Dialog Data
	//{{AFX_DATA(CServerInfoDlg)
	enum { IDD = IDD_DIALOG_SEVERINFO };
	UINT	m_uiPort;
	CString	m_strWebDir;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CServerInfoDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CServerInfoDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SERVERINFODLG_H__949F2C8A_494C_45EA_AA69_DA1F579BED7E__INCLUDED_)
