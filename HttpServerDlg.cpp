// HttpServerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "HttpServer.h"
#include "HttpServerDlg.h"
#include <process.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About
class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CHttpServerDlg dialog
CHttpServerDlg::CHttpServerDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CHttpServerDlg::IDD, pParent)
	, m_dwReceived(0)
	, m_dwTransferred(0)
{
	//{{AFX_DATA_INIT(CHttpServerDlg)
	m_nPort = 8000;
	m_strRootDir = _T("");
	pServerInfoDlg = NULL;
	m_dwConNums = 0;
	m_dwTransRate = 0;
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

CHttpServerDlg::~CHttpServerDlg()
{
	if(pServerInfoDlg != NULL)
	{
		delete pServerInfoDlg;
		pServerInfoDlg = NULL;
	}
}

void CHttpServerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CHttpServerDlg)
	DDX_Control(pDX, IDC_LOG, m_ListLog);
	DDX_Control(pDX, IDC_LIST_FILEREQUEST, m_FileReqListCtrl);
	DDX_Text(pDX, IDC_PORT, m_nPort);
	DDV_MinMaxUInt(pDX, m_nPort, 0, 65535);
	DDX_Text(pDX, IDC_ROOTDIR, m_strRootDir);
	DDX_Text(pDX, IDC_CONNNUMS, m_dwConNums);
	DDX_Text(pDX, IDC_TRANSRATE, m_dwTransRate);
	DDX_Text(pDX, IDC_RECEIVED, m_dwReceived);
	DDX_Text(pDX, IDC_TRANSFERRED, m_dwTransferred);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CHttpServerDlg, CDialog)
	//{{AFX_MSG_MAP(CHttpServerDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_START_STOP, OnStartStop)
	ON_BN_CLICKED(IDC_CLEAR_BUT, OnClearBut)
	ON_BN_CLICKED(IDC_CLEAR_FILEREQ_BUT, OnClearFilereqBut)
	ON_BN_CLICKED(IDC_OPEN_LOG, OnOpenLog)
	//}}AFX_MSG_MAP
	ON_MESSAGE(LOG_MSG, AddLog)
	ON_MESSAGE(DATA_MSG, ShowData)
	ON_MESSAGE(LIST_MSG, AddList)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CHttpServerDlg message handlers

BOOL CHttpServerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	m_bStart = false;
	pHttpProtocol = NULL;

	GetDlgItem(IDC_CLEAR_BUT)->EnableWindow(FALSE);
	GetDlgItem(IDC_CLEAR_FILEREQ_BUT)->EnableWindow(FALSE);
	GetDlgItem(IDC_OPEN_LOG)->EnableWindow(FALSE);
	UpdateData(FALSE);

	//CListCtrl初始化
	InitFileReqList();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CHttpServerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CHttpServerDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CHttpServerDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

/**************************************************************
* 函数名: OnStartStop() 
* 
* 功能: 启动、重启、停止服务器按钮响应。
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpServerDlg::OnStartStop() 
{
	// TODO: Add your control notification handler code here

	CWnd* pWndButton = GetDlgItem(IDC_START_STOP);	

	if ( !m_bStart )
	{     
		pServerInfoDlg = new CServerInfoDlg;
		pServerInfoDlg->DoModal();

		//从设置对话框里获取端口，配置目录等信息.
		m_strRootDir = pServerInfoDlg->m_strWebDir;
		m_nPort = pServerInfoDlg->m_uiPort;
		UpdateData(FALSE);

		pHttpProtocol = new CHttpProtocol;
		pHttpProtocol->m_strRootDir = m_strRootDir;
		pHttpProtocol->m_nPort = m_nPort;
		pHttpProtocol->m_hwndDlg = m_hWnd;

		if (pHttpProtocol->StartHttpSrv())
		{
			GetDlgItem(IDC_CLEAR_BUT)->EnableWindow(TRUE);
			GetDlgItem(IDC_CLEAR_FILEREQ_BUT)->EnableWindow(TRUE);
			GetDlgItem(IDC_OPEN_LOG)->EnableWindow(TRUE);
			pWndButton->SetWindowText("停止服务器");
			SetIcon(AfxGetApp()->LoadIcon(IDI_ICON_START),FALSE);
			UpdateData(FALSE);
			m_bStart = true;
		}
		else
		{
			if(NULL != pHttpProtocol)
			{
				delete pHttpProtocol;
				pHttpProtocol = NULL;
			}
		}
	}
	else
	{
		pHttpProtocol->StopHttpSrv();	
		pWndButton->SetWindowText("重启服务器");
		SetIcon(AfxGetApp()->LoadIcon(IDI_ICON_STOP),FALSE);
		
		HTTPSTATS	stats;
		stats.dwRecv = 0;
		stats.dwSend = 0;
		stats.dwTransRate = 0;
		stats.dwConNums = 0;
		SendMessage(DATA_MSG, (UINT)&stats, NULL);

		m_ListLog.ResetContent();  //清除日志.
		m_FileReqListCtrl.DeleteAllItems();  //清除文件请求.

		SetDlgItemText(IDC_PORT,NULL);
		SetDlgItemText(IDC_ROOTDIR,NULL);
		
		if(NULL != pHttpProtocol)
		{
			delete pHttpProtocol;
			pHttpProtocol = NULL;
		}
		m_bStart = false;
	}

}

/**************************************************************
* 函数名: OnCancel() 
* 
* 功能: 退出服务器响应。
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpServerDlg::OnCancel() 
{
	// TODO: Add extra cleanup here
	if(m_bStart) //服务器正在运行，要有提示.2013-1-6
	{
		if(IDNO ==	MessageBox("服务器正在运行,确认退出？","退出确认",\
					MB_YESNO|MB_ICONEXCLAMATION|MB_ICONWARNING))
		{
			return;
		}
	}

 	if (m_bStart)	
	{
		pHttpProtocol->StopHttpSrv();
	}
	if(NULL != pHttpProtocol) //2012-11-30 
	{
		delete pHttpProtocol;
		pHttpProtocol = NULL;
	}

	m_bStart = false;
	CDialog::OnCancel();
}


/**************************************************************
* 函数名: AddLog(WPARAM wParam, LPARAM lParam)
* 
* 功能: 显示日志信息,LOG_MSG的消息响应函数，并存入日志文件。
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpServerDlg::AddLog(WPARAM wParam, LPARAM lParam)
{
	char szBuf[1024];
	CString *strTemp = (CString *)wParam; 

	CListBox* pListBox;
	pListBox = (CListBox*) GetDlgItem(IDC_LOG);

	SYSTEMTIME st;
	GetLocalTime(&st);
    wsprintf(szBuf,"[%04d-%02d-%02d %02d:%02d:%02d.%03d]   %s", st.wYear,st.wMonth,st.wDay, \
			st.wHour, st.wMinute, st.wSecond,st.wMilliseconds, *strTemp);

	pListBox->AddString(szBuf);

	//将访问记录存储在Log\HttpServerLog.txt中
	//获取当前文件目录
	CString strPathName = m_strRootDir;
	strPathName = strPathName + "\\HttpServerLog.txt";
	
	CFile myFile;
	CFileException e;
	if( !myFile.Open( strPathName,CFile::modeCreate|CFile::modeNoTruncate|CFile::modeReadWrite, &e))
	{
		#ifdef _DEBUG
			afxDump << "File could not be opened " << e.m_cause << "\n";
		#endif
		return;
	}
	
	//设定末尾插入元素
	myFile.SeekToEnd();
	char szBuffer[1024];
	memset(szBuffer,0,1024);
	sprintf(szBuffer,"%s\r\n",szBuf);
	myFile.Write(szBuffer,lstrlen(szBuffer));	
	myFile.Close();
}


/**************************************************************
* 函数名: GetMyFilePath()
* 
* 功能: 获取Debug目录,扩展用。
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
CString CHttpServerDlg::GetMyFilePath()
{
	char pro_path[MAX_PATH];
	GetModuleFileName(NULL,pro_path,MAX_PATH);
	CString str_path(pro_path);	
	int iPos=str_path.ReverseFind('\\')+1;
	
	CString myPath;
	myPath=str_path.Left(iPos);
	
	return myPath;
}

/**************************************************************
* 函数名: ShowData(WPARAM wParam, LPARAM lParam)
* 
* 功能: 显示接收和发送的数据流量,DATA_MSG的消息响应函数。
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpServerDlg::ShowData(WPARAM wParam, LPARAM lParam)
{
	PHTTPSTATS pStats = (PHTTPSTATS)wParam;
	m_dwReceived = pStats->dwRecv;
	m_dwTransferred = pStats->dwSend;
	m_dwTransRate = pStats->dwTransRate;
	m_dwConNums = pStats->dwConNums;

	UpdateData(false);
}

/**************************************************************
* 函数名: OnClearBut()
* 
* 功能: 清除日志文件.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpServerDlg::OnClearBut() 
{
	// TODO: Add your control notification handler code here
	CListBox  *listBox=(CListBox *)GetDlgItem(IDC_LOG);

	if (IDOK == MessageBox("确定清空日志文件吗?","Http服务器",MB_OKCANCEL))
	{
		listBox->ResetContent(); // 删除告警列表框所有内容
	}
	else
	{
		return ;
	}

	UpdateData(false);
}


void CHttpServerDlg::InitFileReqList()
{
	m_FileReqListCtrl.SetExtendedStyle(LVS_EX_GRIDLINES|LVS_EX_FULLROWSELECT|
		LVS_EX_ONECLICKACTIVATE);

	m_FileReqListCtrl.InsertColumn(0,"文件", LVCFMT_LEFT,430);
	m_FileReqListCtrl.InsertColumn(1,"请求次数",LVCFMT_LEFT,75);	
}

/**************************************************************
* 函数名: AddList(WPARAM wParam, LPARAM lParam)
* 
* 功能: 记录访问请求的文件及请求个数,LIST_MSG的消息响应函数。
*
* 档案历史: 2013-01-01 新生成
**************************************************************/
void CHttpServerDlg::AddList(WPARAM wParam, LPARAM lParam)
{
	REQUEST* pRequest = (REQUEST*)(wParam);
	char* szErrorReson = pRequest->StatuCodeReason;
	char* szFileName = pRequest->szFileName;

	char szBuf[256];
	char szDisplayName[_MAX_PATH+10];
	DWORD dwHitCount;
	int nIndex;	

	// 文件是否存在.
	if (0 != strcmp("404",szErrorReson) && 0 != strcmp("400",szErrorReson))
	{
		strcpy(szDisplayName, szFileName);
	}
	else
	{
		wsprintf(szDisplayName,"%s - %s",szErrorReson, szFileName);
	}

	//List中是否已经存在文件.
	LVFINDINFO info;	
	info.flags = LVFI_STRING;
	info.psz = szDisplayName;
	nIndex=m_FileReqListCtrl.FindItem(&info);
		
	LVITEM lvItem;

	//如果存在，则只是增加计数。
	if ( -1 != nIndex)
	{
		lvItem.mask = LVIF_TEXT;
		lvItem.iItem = nIndex;
		lvItem.iSubItem = 1;
		lvItem.pszText = szBuf;
		lvItem.cchTextMax = sizeof(szBuf);
		if(m_FileReqListCtrl.GetItem(&lvItem))
		{
			dwHitCount = atol(szBuf);
			dwHitCount++;
			wsprintf(szBuf,"%ld",dwHitCount);
			m_FileReqListCtrl.SetItem(&lvItem);
		}
	}	

	//在列表中增加一条新的记录。
	else
	{
		lvItem.mask = LVIF_TEXT;
		lvItem.iItem = 0;
		lvItem.iSubItem = 0;
		lvItem.pszText = szDisplayName;
		nIndex = m_FileReqListCtrl.InsertItem(&lvItem);  //注意此处插入的位置.
		
		lvItem.iItem = nIndex;
		lvItem.iSubItem = 1;
		lvItem.pszText = "1";
		m_FileReqListCtrl.SetItem(&lvItem);
	}
	UpdateData(FALSE);
}

/**************************************************************
* 函数名: OnClearFilereqBut() 
* 
* 功能: 清空文件请求操作。
*
* 档案历史: 2013-01-01 新生成
**************************************************************/
void CHttpServerDlg::OnClearFilereqBut() 
{
	// TODO: Add your control notification handler code here
	CListCtrl  *listCtrl=(CListCtrl *)GetDlgItem(IDC_LIST_FILEREQUEST);
	
	if (IDOK == MessageBox("确定清空文件请求吗?","Http服务器",MB_OKCANCEL))
	{
		listCtrl->DeleteAllItems(); // 删除告警列表框所有内容
	}
	else
	{
		return ;  
	}
	
	UpdateData(false);
}


/**************************************************************
* 函数名: OnOpenLog() 
* 
* 功能: 打开并访问日志文件操作。
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpServerDlg::OnOpenLog() 
{
	// TODO: Add your control notification handler code here
	//将访问记录存储在HttpServerLog.txt中
	//获取当前文件目录

//	CString strDebugName = GetMyFilePath();
//	int xPos = strDebugName.Find("\\Debug");
	CString strPathName = m_strRootDir;
//	strPathName = strDebugName.Left(xPos);
	strPathName = strPathName + "\\HttpServerLog.txt";
	
	CFile myFile;
	CFileException e;
	if( !myFile.Open( strPathName,CFile::modeRead, &e ) )
	{
		#ifdef _DEBUG
		afxDump << "File could not be opened " << e.m_cause << "\n";
		#endif
		return;
	}

	//打开HttpServerLog.txt文档
	ShellExecute(m_hWnd, "open", strPathName.GetBuffer(0), NULL, NULL, SW_SHOWNORMAL);

	myFile.Close();
}
