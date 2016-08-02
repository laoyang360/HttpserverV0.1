// ServerInfoDlg.cpp : implementation file
//

#include "stdafx.h"
#include "HttpServer.h"
#include "ServerInfoDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CServerInfoDlg dialog


CServerInfoDlg::CServerInfoDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CServerInfoDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CServerInfoDlg)
	m_uiPort = 80;
	m_strWebDir = _T("");
	//}}AFX_DATA_INIT
	m_AppKey = new char[100];
	m_DirKey = new char[100];
	m_PortKey = new char[100];
	memset(m_AppKey,0,100);
	memset(m_DirKey,0,100);
	memset(m_PortKey,0,100);
}

CServerInfoDlg::~CServerInfoDlg()
{
	if(NULL != m_AppKey)
	{
		m_AppKey = NULL;
	}
	if(NULL != m_DirKey)
	{
		m_DirKey = NULL;
	}
	if(NULL != m_PortKey)
	{
		m_PortKey = NULL;
	}
}

void CServerInfoDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CServerInfoDlg)
	DDX_Text(pDX, IDC_EDIT_PORT, m_uiPort);
	DDX_Text(pDX, IDC_EDIT_ROOTWEB, m_strWebDir);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CServerInfoDlg, CDialog)
	//{{AFX_MSG_MAP(CServerInfoDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CServerInfoDlg message handlers

/**************************************************************
* 函数名: OnOK() 
* 
* 功能: 配置确认按钮响应函数，确认配置目录，并写入win.ini中。
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CServerInfoDlg::OnOK() 
{
	// TODO: Add extra validation here
	//将端口port&路径dir信息写入win.ini
	//保存端口号
	UpdateData(TRUE);
	char szText[256];
	GetDlgItemText(IDC_EDIT_PORT, szText, sizeof(szText));
	WriteProfileString(m_AppKey, m_PortKey, szText);
	
	//保存传输文件路径
	GetDlgItemText(IDC_EDIT_ROOTWEB, szText, sizeof(szText));
	WriteProfileString(m_AppKey, m_DirKey, szText);    
	
	CDialog::OnOK();
}

BOOL CServerInfoDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	strcpy(m_AppKey,"HTTPSRVM");
	strcpy(m_PortKey,"PORT");
	strcpy(m_DirKey,"ROOTDIR");
	
	char szText[256];
	memset(szText,0,256);
	int port = 0;

	port = GetProfileInt(m_AppKey, m_PortKey, 0);  	//读取端口号
	if(0 != port)
	{
		m_uiPort = port;
		UpdateData(false);
	}
	
 	// 读取传输文件路径
 	GetProfileString(m_AppKey, m_DirKey, "", szText, sizeof(szText));
 	SetDlgItemText(IDC_ROOTDIR, szText);
 	m_strWebDir.Format("%s", szText);
	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
