#include "stdafx.h"
#include "HttpProtocol.h"
#include <io.h>
#include <stdio.h>
#include <string>

// 格林威治时间的星期转换
char *week[] = {		
	"Sun,",  
	"Mon,",
	"Tue,",
	"Wed,",
	"Thu,",
	"Fri,",
	"Sat,",
};
 
// 格林威治时间的月份转换
char *month[] = 
{	
	"Jan",  
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec",
};


UINT CHttpProtocol::ClientNum = 0;
CCriticalSection CHttpProtocol::m_critSect;		// 排斥区初始化
HANDLE	CHttpProtocol::None = NULL;
CCriticalSection CHttpProtocol::m_csEnvVar;     // 环境变量排斥区初始化.
HWND CHttpProtocol::m_hwndDlg = NULL;

CHttpProtocol::CHttpProtocol(void)
{
	m_pListenThread = NULL;	
	m_hwndDlg = NULL;
	m_pData = NULL;
	m_nBufferSize = 0;
	m_nWritePos = 0;

	stats.dwConNums = 0;
	stats.dwRecv = 0;
	stats.dwSend = 0;
	stats.dwTransRate = 0;
	stats.dwElapsedTime = 0;
}

CHttpProtocol::~CHttpProtocol(void)
{
	if(m_pData != NULL) 
	{
		delete[] m_pData;
		m_pData = NULL;
	}

	m_nBufferSize = 0;
	m_nWritePos = 0;
}

/**************************************************************
* 函数名: StartHttpSrv()
* 
* 功能: 开启服务器,创建、绑定、监听线程开启.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
bool CHttpProtocol::StartHttpSrv()
{

	WORD wVersionRequested = WINSOCK_VERSION;
	WSADATA wsaData;
	int nRet;

	// 启动Winsock
	nRet = WSAStartup(wVersionRequested, &wsaData);		// 加载成功返回0
	if (0 != nRet)
	{   
		// 错误处理
		AfxMessageBox("Initialize WinSock Failed");
		return false;
	}
	// 检测版本
	if (wsaData.wVersion != wVersionRequested)
	{    
		// 错误处理   
		AfxMessageBox("Wrong WinSock Version");
		return false;
	}
	
	m_hExit = CreateEvent(NULL, TRUE, FALSE, NULL);	
	if (m_hExit == NULL)
	{
		return false;
	}

	//创建套接字
	m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_listenSocket == INVALID_SOCKET)
	{
		// 异常处理
		CString *pStr = new CString;
		*pStr = "Could not create listen socket";
		SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
	
		return false;
	}

	SOCKADDR_IN sockAddr;
	LPSERVENT	lpServEnt;
	if ( 0 != m_nPort)
	{
		// 从主机字节顺序转为网络字节顺序赋给sin_port
		sockAddr.sin_port = htons(m_nPort);
	}
	else
	{	
		//获取已知http服务的端口，该服务在tcp协议下注册
		lpServEnt = getservbyname("http", "tcp");
		if (lpServEnt != NULL)
		{
			sockAddr.sin_port = lpServEnt->s_port;
		}
		else
		{
			sockAddr.sin_port = htons(HTTPPORT);	// 默认端口HTTPPORT＝80
		}
	}

	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = INADDR_ANY;  // 指定端口号上面的默认IP接口 

	// 初始化content-type和文件后缀对应关系的map
    CreateTypeMap();

	// 套接字绑定
	nRet = bind(m_listenSocket, (LPSOCKADDR)&sockAddr, sizeof(struct sockaddr));
	if (nRet == SOCKET_ERROR)
	{  
		// 绑定发生错误
		CString *pStr = new CString;
		pStr->Format("bind() error code: %d",GetLastError());
		SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
		closesocket(m_listenSocket);	// 断开链接
	
		return false;
	}

    // 套接字监听。为客户连接创建等待队列,队列最大长度SOMAXCONN在windows sockets头文件中定义
	nRet = listen(m_listenSocket, SOMAXCONN);
	if (nRet == SOCKET_ERROR)
	{   
		// 异常处理
		CString *pStr = new CString;
		*pStr = "listen() error";
		SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
		closesocket(m_listenSocket);	// 断开链接

		return false;
	}

	// 创建listening进程，接受客户机连接要求
	m_pListenThread = AfxBeginThread(ListenThread, this);
	
	if (NULL == m_pListenThread)
	{
		// 线程创建失败
		CString *pStr = new CString;
		*pStr = "Could not create listening thread" ;
		SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
		closesocket(m_listenSocket);	// 断开链接

		return false;
	}

	CString strIP, strTemp;
	char hostname[255];
	PHOSTENT hostinfo;
	// 获取计算机名
	if(gethostname(hostname, sizeof(hostname))==0)	// 得到主机名
	{
		// 由主机名得到主机的其他信息
		hostinfo = gethostbyname(hostname);
		if(hostinfo != NULL)
		{
			strIP = inet_ntoa(*(struct in_addr*)*(hostinfo->h_addr_list));
		}
	}
	
	// 显示web服务器正在启动
	CString *pStr = new CString;
	*pStr = "******** HttpServer is Starting now! *********";
	SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);

	// 显示web服务器的信息，包括主机名，IP以及端口号
	CString *pStr1 = new CString;
	pStr1->Format("%s", hostname); 
	*pStr1 = *pStr1 + "[" + strIP + "]" + "   Port ";
	strTemp.Format("%d", htons(sockAddr.sin_port));
	*pStr1 = *pStr1 + strTemp;
	SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr1, NULL);

	return true;

}

/**************************************************************
* 函数名: CreateTypeMap()
* 
* 功能: 关联扩展名及对应的类型.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpProtocol::CreateTypeMap()
{
	// 初始化map
    m_typeMap[".doc"]	= "application/msword";
	m_typeMap[".bin"]	= "application/octet-stream";
	m_typeMap[".dll"]	= "application/octet-stream";
	m_typeMap[".exe"]	= "application/octet-stream";
	m_typeMap[".pdf"]	= "application/pdf";
	m_typeMap[".ai"]	= "application/postscript";
	m_typeMap[".eps"]	= "application/postscript";
	m_typeMap[".ps"]	= "application/postscript";
	m_typeMap[".rtf"]	= "application/rtf";
	m_typeMap[".fdf"]	= "application/vnd.fdf";
	m_typeMap[".arj"]	= "application/x-arj";
	m_typeMap[".gz"]	= "application/x-gzip";
	m_typeMap[".class"]	= "application/x-java-class";
	m_typeMap[".js"]	= "application/x-javascript";
	m_typeMap[".lzh"]	= "application/x-lzh";
	m_typeMap[".lnk"]	= "application/x-ms-shortcut";
	m_typeMap[".tar"]	= "application/x-tar";
	m_typeMap[".hlp"]	= "application/x-winhelp";
	m_typeMap[".cert"]	= "application/x-x509-ca-cert";
	m_typeMap[".zip"]	= "application/zip";
	m_typeMap[".cab"]	= "application/x-compressed";
	m_typeMap[".arj"]	= "application/x-compressed";
	m_typeMap[".aif"]	= "audio/aiff";
	m_typeMap[".aifc"]	= "audio/aiff";
	m_typeMap[".aiff"]	= "audio/aiff";
	m_typeMap[".au"]	= "audio/basic";
	m_typeMap[".snd"]	= "audio/basic";
	m_typeMap[".mid"]	= "audio/midi";
	m_typeMap[".rmi"]	= "audio/midi";
	m_typeMap[".mp3"]	= "audio/mpeg";
	m_typeMap[".vox"]	= "audio/voxware";
	m_typeMap[".wav"]	= "audio/wav";
	m_typeMap[".ra"]	= "audio/x-pn-realaudio";
	m_typeMap[".ram"]	= "audio/x-pn-realaudio";
	m_typeMap[".bmp"]	= "image/bmp";
	m_typeMap[".gif"]	= "image/gif";
	m_typeMap[".jpeg"]	= "image/jpeg";
	m_typeMap[".jpg"]	= "image/jpeg";
	m_typeMap[".tif"]	= "image/tiff";
	m_typeMap[".tiff"]	= "image/tiff";
	m_typeMap[".xbm"]	= "image/xbm";
	m_typeMap[".wrl"]	= "model/vrml";
	m_typeMap[".htm"]	= "text/html";
	m_typeMap[".html"]	= "text/html";
	m_typeMap[".c"]		= "text/plain";
	m_typeMap[".cpp"]	= "text/plain";
	m_typeMap[".def"]	= "text/plain";
	m_typeMap[".h"]		= "text/plain";
	m_typeMap[".txt"]	= "text/plain";
	m_typeMap[".rtx"]	= "text/richtext";
	m_typeMap[".rtf"]	= "text/richtext";
	m_typeMap[".java"]	= "text/x-java-source";
	m_typeMap[".css"]	= "text/css";
	m_typeMap[".mpeg"]	= "video/mpeg";
	m_typeMap[".mpg"]	= "video/mpeg";
	m_typeMap[".mpe"]	= "video/mpeg";
	m_typeMap[".avi"]	= "video/msvideo";
	m_typeMap[".mov"]	= "video/quicktime";
	m_typeMap[".qt"]	= "video/quicktime";
	m_typeMap[".shtml"]	= "wwwserver/html-ssi";
	m_typeMap[".asa"]	= "wwwserver/isapi";
	m_typeMap[".asp"]	= "wwwserver/isapi";
	m_typeMap[".cfm"]	= "wwwserver/isapi";
	m_typeMap[".dbm"]	= "wwwserver/isapi";
	m_typeMap[".isa"]	= "wwwserver/isapi";
	m_typeMap[".plx"]	= "wwwserver/isapi";
	m_typeMap[".url"]	= "wwwserver/isapi";
	m_typeMap[".cgi"]	= "wwwserver/isapi";
	m_typeMap[".php"]	= "wwwserver/isapi";
	m_typeMap[".wcgi"]	= "wwwserver/isapi";

}

/**************************************************************
* 函数名: ListenThread(LPVOID param)
* 
* 功能: 监听线程,循环接收.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
UINT CHttpProtocol::ListenThread(LPVOID param)
{
	CHttpProtocol *pHttpProtocol = (CHttpProtocol *)param;

	SOCKET		socketClient;
	CWinThread*	pClientThread;
	SOCKADDR_IN	SockAddr;
	PREQUEST	pReq;
	int			nLen;
	DWORD		dwRet;

	// 初始化ClientNum，创建"no client"事件对象
	HANDLE		hNoClients;
	hNoClients = pHttpProtocol->InitClientCount();

	while(1)	// 循环等待,如有客户连接请求,则接受客户机连接要求
	{	
		nLen = sizeof(SOCKADDR_IN);		
		// 套接字等待链接,返回对应已接受的客户机连接的套接字
		socketClient = accept(pHttpProtocol->m_listenSocket, (LPSOCKADDR)&SockAddr, &nLen);
		if (socketClient == INVALID_SOCKET)
		{   
			break;
		}		
		// 将客户端网络地址转换为用点分割的IP地址
		CString *pstr = new CString;
		pstr->Format("%s Connecting on socket:%d", inet_ntoa(SockAddr.sin_addr), socketClient);
		SendMessage(pHttpProtocol->m_hwndDlg, LOG_MSG, (UINT)pstr, NULL);

        pReq = new REQUEST;
		if (pReq == NULL)
		{   
			// 处理错误
			CString *pStr = new CString;
			*pStr = "No memory for request";
			SendMessage(pHttpProtocol->m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
			continue;
		}
		pReq->hExit  = pHttpProtocol->m_hExit;
		pReq->Socket = socketClient;
		pReq->hFile = INVALID_HANDLE_VALUE;
		pReq->dwRecv = 0;
		pReq->dwSend = 0;
		//the number of milliseconds that have elapsed since the system was started.
		pReq->dwConnectTime = GetTickCount();  //add by ycy.
		pReq->pHttpProtocol = pHttpProtocol;

	    // 创建client进程，处理request
		pClientThread = AfxBeginThread(ClientThread, pReq);
		if (!pClientThread)
		{  
			// 线程创建失败,错误处理
			CString *pStr = new CString;
			*pStr = "Couldn't start client thread";
			SendMessage(pHttpProtocol->m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
			delete pReq;
		}
	} 
	//while
	// 等待线程结束
	WaitForSingleObject((HANDLE)pHttpProtocol->m_hExit, INFINITE);
    // 等待所有client进程结束
	dwRet = WaitForSingleObject(hNoClients, 5000);
	if (dwRet == WAIT_TIMEOUT) 
	{  
		// 超时返回，并且同步对象未退出
		CString *pStr = new CString;
		*pStr = "One or more client threads did not exit";
		SendMessage(pHttpProtocol->m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
	}
	pHttpProtocol->DeleteClientCount();

	return 0;
}

/**************************************************************
* 函数名: ClientThread(LPVOID param)
* 
* 功能: 客户线程，调用接收请求、分析请求、对不同类型的处理。
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
UINT CHttpProtocol::ClientThread(LPVOID param)
{
	int nRet;
	BYTE buf[1024];
	memset(buf,0,1024);
	PREQUEST pReq = (PREQUEST)param;
	CHttpProtocol *pHttpProtocol = (CHttpProtocol *)pReq->pHttpProtocol;

	pHttpProtocol->CountUp();			// 记数
	
	// 接收request data
	if (!pHttpProtocol->RecvRequest(pReq, buf, sizeof(buf)))
	{  
		pHttpProtocol->Disconnect(pReq);
		delete pReq;
		pHttpProtocol->CountDown();      
		return 0;  
	}
	
	// 分析request信息r
	nRet = pHttpProtocol->Analyze(pReq, buf);
	if (nRet != 0)
	{	
		// 处理错误
		CString *pStr = new CString;
		*pStr = "Error occurs when analyzing client request";
		SendMessage(pHttpProtocol->m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);

		pHttpProtocol->Disconnect(pReq);
		delete pReq;
		pHttpProtocol->CountDown();     
		return 0;
	}
	
	if(pReq->bHasExe)
	{
		//不同请求Method的处理方式.
		AnalyzeAndProcReqType(pReq);
	}

	pHttpProtocol->Disconnect(pReq);
	delete pReq;
	pHttpProtocol->CountDown();	// client数量减1

	return 0;
}

/**************************************************************
* 函数名: RecvRequest(PREQUEST pReq, LPBYTE pBuf, DWORD dwBufSize)
* 
* 功能: 接收请求，存入buf,便于后续处理。
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
bool CHttpProtocol::RecvRequest(PREQUEST pReq, LPBYTE pBuf, DWORD dwBufSize)
{
	WSABUF			wsabuf;			// 发送/接收缓冲区结构
	WSAOVERLAPPED	over;			// 指向调用重叠操作时指定的WSAOVERLAPPED结构
	DWORD			dwRecv;
	DWORD			dwFlags;
	DWORD			dwRet;
	HANDLE			hEvents[2];
	bool			fPending;
	int				nRet;
	memset(pBuf, 0, dwBufSize);		// 初始化缓冲区
    wsabuf.buf  = (char *)pBuf;
	wsabuf.len  = dwBufSize;		// 缓冲区的长度
	over.hEvent = WSACreateEvent();	// 创建一个新的事件对象
	dwFlags = 0;
	fPending = FALSE;  
	// 接收数据
	nRet = WSARecv(pReq->Socket, &wsabuf, 1, &dwRecv, &dwFlags, &over, NULL);

    if (nRet != 0)  
	{
    	// 错误代码WSA_IO_PENDING表示重叠操作成功启动
		// 重叠操作已经成功初始化，将在后续的时间完成.
		if (WSAGetLastError() != WSA_IO_PENDING)
		{   
			// 重叠操作未能成功
			CloseHandle(over.hEvent);
			return false;
		}
		else	
		{
			fPending = true;
		}
	}
	if (fPending)
	{	
		hEvents[0]  = over.hEvent;
		hEvents[1]  = pReq->hExit;
		dwRet = WaitForMultipleObjects(2, hEvents, FALSE, INFINITE);
		if (dwRet != 0)
		{
			CloseHandle(over.hEvent);                     
			return false;
		}
		// 重叠操作未完成
		if (!WSAGetOverlappedResult(pReq->Socket, &over, &dwRecv, FALSE, &dwFlags))
		{
			CloseHandle(over.hEvent);                    
			return false;
		}
	}
	pReq->dwRecv += dwRecv;	// 统计接收数量
	CloseHandle(over.hEvent);                           
	return true;
}
	
/**************************************************************
* 函数名: Analyze(PREQUEST pReq, LPBYTE pBuf)
* 
* 功能: 分析请求消息，存入结构体PREQUEST。
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
int  CHttpProtocol::Analyze(PREQUEST pReq, LPBYTE pBuf)
{
	char* pBuffer = new char[strlen((char*)pBuf)+1];                //便于后续处理.
	strcpy(pBuffer,(char*)pBuf);

	// 分析接收到的信息
	char szSeps[] = " \n";
	char *cpToken;

	// 防止非法请求
	if (strstr((const char *)pBuf, "..") != NULL)
	{
		strcpy(pReq->StatuCodeReason, "400");
		strcpy(pReq->szFileName, m_strRootDir);
		strcat(pReq->szFileName,"\\..");
		// Send "bad request" error
		SendError(pReq, HTTP_STATUSNO_BADREQUEST);

		//文件请求格式错误.
		SendMessage(m_hwndDlg,LIST_MSG,UINT(pReq),NULL);

		return 1;
	}

	// 判断ruquest的mothed	
	cpToken = strtok((char *)pBuf, szSeps);	// 缓存中字符串分解为一组标记串。	
	if (!_stricmp(cpToken, "GET"))			// GET命令
	{
		pReq->nMethod = METHOD_GET;
	}
	else if (!_stricmp(cpToken, "HEAD"))	// HEAD命令
	{
		pReq->nMethod = METHOD_HEAD;  
	}
	else if(!_stricmp(cpToken, "POST"))    // POST命令
	{
		pReq->nMethod = METHOD_POST;
	}
	else  
	{
        strcpy(pReq->StatuCodeReason, HTTP_STATUS_NOTIMPLEMENTED);
		
		// Send "not implemented" error
		SendError(pReq, HTTP_STATUSNO_NOTIMPLEMENTED);

		return 1;
	}
    
	// 获取Request-URI
	cpToken = strtok(NULL, szSeps);
	if (cpToken == NULL)   
	{
		strcpy(pReq->StatuCodeReason, HTTP_STATUS_BADREQUEST);

		// Send "bad request" error
		SendError(pReq, HTTP_STATUSNO_BADREQUEST);

		return 1;
	}

	m_strExePathName.Format("%s",cpToken);				//记录get或post请求的文件名. 
	string strExePathName(cpToken);
	
	//对请求为'/'的单独处理之,返回全路径.
	if("/" == m_strExePathName)
	{
		m_nBufferSize = 0;
		m_nWritePos = 0;
	
		pReq->bHasExe = false;
		
		char chDir[1024];
		memset(chDir,0,1024);
		strcpy(chDir,m_strRootDir.GetBuffer(0));
		string strDir(chDir);

		OpenDir(strExePathName,strDir);
		
		strcpy(pReq->szFileName,cpToken);
 		SendMessage(m_hwndDlg,LIST_MSG,UINT(pReq),NULL);

		CString strTmp;
		strTmp.Format("%s",m_pData);

		SendBuffer(pReq,(unsigned char*)m_pData,strlen(m_pData));
 		
	}  
	else                                                   //含有名称的单独处理.
	{
		pReq->bHasExe = true;

		int nPos = m_strExePathName.ReverseFind ('/');     //2013-1-16 '//'改为'/'
		m_strExeFileName = m_strExePathName.Mid(0,nPos);   //得到'/cgi'
		
		CString strCurrentPathTmp = m_strRootDir+m_strExeFileName;
		strcpy(pReq->m_chCurrentPath,strCurrentPathTmp.GetBuffer(0));
		
		//2013-1-13加post特殊处理.
		if(METHOD_POST == pReq->nMethod)
		{	
			// 记录Content-Length 长度.
			CString strContent(pBuffer);
			int iBeginExt  = strContent.Find("Content-Length: ");
			int iBegin = iBeginExt+strlen("Content-Length: ");
			int iEndExt = strContent.Find("Connection");
			int iEnd    = iEndExt-strlen("\r\n");
			
			CString strLen = strContent.Mid(iBegin,iEnd-iBegin);
			char* chLen = strLen.GetBuffer(0);
			pReq->contentLength = atoi(chLen);                        //获取参数的长度.
			
			//取得最后一行，参数数据.
			CString strBuffer(pBuffer);
			int iLineFeed = strBuffer.Find("\r\n\r\n");
			int iStart = iLineFeed + strlen("\r\n\r\n");
			int iStop = strBuffer.GetLength();
			
			CString strLastLine = strBuffer.Mid(iStart,iStop-iStart);
			strcpy(pReq->parameter,(char*)strLastLine.GetBuffer(0));   //获取参数
		}
		else
		{
			pReq->contentLength = 0;
			pReq->parameter[0]  ='\0';
		}
	}

	strcpy(pReq->szFileName, m_strRootDir);
	if (strlen(cpToken) > 1)
	{
		strcat(pReq->szFileName, cpToken);	// 把该文件名添加到结尾处形成路径
	}
	else
	{
		strcat(pReq->szFileName, "/index.html");
	}
	
	//增加文件类型的判断...
	CString strPostfix;
	CString strFileName(pReq->szFileName);
	int nBegin = strFileName.ReverseFind('.');
	int nLast = strFileName.GetLength();
	strPostfix = strFileName.Mid(nBegin,nLast);

	strcpy(pReq->postfix, strPostfix.GetBuffer(0));               //此处有bug

	delete []pBuffer;
	return 0;
}

//CGI执行程序
/*-------------------------------------------------------------------------------
---输入：pReq->szFileName,pReq->m_chCurrentPath,NULL,pReq->pOutMsg,pReq->parameter
---用新进程执行.exe程序,管道通信,并返回outMsg消息,用以send! <2013-1-10>
---更改了原有的冗余的进程通信机制.
----------------------------------------------------------------------------------*/
/**************************************************************
* 函数名: ExecCGI(PREQUEST pReq)
* 
* 功能: 对post方式及Get请求的exe/cgi进行处理，核心：进程通信.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
char* CHttpProtocol::ExecCGI(PREQUEST pReq)
{
	HANDLE hParentReadPipe,hParentWritePipe,hProcess;
	SECURITY_ATTRIBUTES SecuAttr;
	SecuAttr.bInheritHandle=true;
	SecuAttr.nLength=sizeof(SECURITY_ATTRIBUTES);
	SecuAttr.lpSecurityDescriptor=NULL;
	if(!CreatePipe(&hParentReadPipe,&hParentWritePipe,&SecuAttr,0))
	{
		CString *pStr = new CString;
		pStr->Format("管道创建失败，错误号：%d",GetLastError());
		SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
	
		return NULL;
	}

	STARTUPINFO InitInfo;
	PROCESS_INFORMATION ProInfo;
	memset(&InitInfo,0,sizeof(STARTUPINFO));

	InitInfo.cb=sizeof(STARTUPINFO);
	GetStartupInfo(&InitInfo);
	InitInfo.dwFlags=STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	InitInfo.wShowWindow=SW_HIDE;
	InitInfo.hStdInput = hParentReadPipe;
	InitInfo.hStdError=GetStdHandle(STD_ERROR_HANDLE);       //
	InitInfo.hStdOutput=hParentWritePipe;					 //
	InitInfo.dwXSize=0;
	InitInfo.dwYSize=0;
	
	if(!CreateProcess(pReq->szFileName,
				  NULL,
				  NULL,
				  NULL,
				  true,
				  NULL,
				  NULL,
				  pReq->m_chCurrentPath,					//CurrentDiectoryPath指明子进程的路径--需要改动!
				  &InitInfo,
				  &ProInfo))
	{
		hParentReadPipe = NULL;
		hParentWritePipe = NULL;
		CloseHandle(hParentReadPipe);
		CloseHandle(hParentWritePipe);

		CString* pStrError = new CString;
		pStrError->Format("进程创建失败，错误号:%d",GetLastError());
		SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStrError, NULL);
		return NULL;
	}
	else
	{
		CloseHandle(ProInfo.hThread);    //创建成功了以后关闭句柄?为什么?
		hProcess=ProInfo.hProcess;
	}

	//在这里写HTML表单传过来的POST数据
	if(*pReq->parameter != '\0')
	{
		DWORD dwWrite;
		if(!WriteFile(hParentWritePipe,pReq->parameter,strlen(pReq->parameter),&dwWrite,NULL))
		{
			CString* pStrError = new CString;
			pStrError->Format(_T("写入数据失败，错误号：%d"),GetLastError());
			SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStrError, NULL);
			return NULL;
		}
		
	}
	
	//在新程序进程执行的时候读数据
	ULONG nTotalPipeBytes = 0;
	char* pBuffer=new char[MaxCGIBufferSize];
	memset(pBuffer,0,MaxCGIBufferSize);
	DWORD dwRead = 0;

	while(true)
	{
		int nRtn;
		nRtn=WaitForSingleObject(hProcess,1000);
		//通过信号量等待进行的执行，如果等待超时则退出。
		if(nRtn==WAIT_TIMEOUT)
			break;

		//如果没有找到管道，则同样退出
		if(!PeekNamedPipe(hParentReadPipe,NULL,0,NULL,&nTotalPipeBytes,NULL))
			break;

		if(nTotalPipeBytes>0)
		{
			dwRead=0;
			if(ReadFile(hParentReadPipe,pBuffer,MaxCGIBufferSize,&dwRead,NULL)==NULL)
				break;
			if(dwRead>0)
			{
				strcat(pReq->pOutMsg,pBuffer);
			}
		}
		else
		{
			break;
		}
	}

	delete[] pBuffer;
	CloseHandle(ProInfo.hProcess);
	CloseHandle(hParentReadPipe);
	CloseHandle(hParentWritePipe);

	return pReq->pOutMsg;
}


/******************************************************************************
* 函数名: AnalyzeAndProcReqType(PREQUEST pReq)
* 
* 功能: 对不同的请求方式GET/HEAD/POST的方式处理,不同类型.exe/.cgi或其它类型处理。
*
* 档案历史: 2013-01-30 新生成
******************************************************************************/
void CHttpProtocol::AnalyzeAndProcReqType(PREQUEST pReq)
{
	CHttpProtocol *pHttpProtocol = (CHttpProtocol *)pReq->pHttpProtocol;

	if(	pReq->nMethod == METHOD_HEAD)
	{
		pHttpProtocol->SendHeader(pReq);
	}//end METHOD_HEAD
	else if( pReq->nMethod == METHOD_GET)
	{
		//1,扩展名为.exe/.cgi
		if(stricmp(pReq->postfix,".cgi")==0||stricmp(pReq->postfix,".exe")==0)  	//1,扩展名为.exe/.cgi
		{	
			//此处加上对Post信息的处理,涉及进程通信.		
			//调用CGI程序并得到运行结果,一定要给pOutMsg进行初始化
			memset(pReq->pOutMsg,0,1024*4);
			ExecCGI(pReq);
			
			pHttpProtocol->SendHeader(pReq);
			pHttpProtocol->SendFile(pReq);
			
		}
		else if(stricmp(pReq->postfix,".asp")==0)
		{
			//扩展处理,
		}
		else																	 //2,扩展名为除.exe/.cgi的其余
		{		
			//生成并返回头部
			pHttpProtocol->SendHeader(pReq);
			pHttpProtocol->SendFile(pReq);
		}
	}//end METHOD_GET
	else if( pReq->nMethod == METHOD_POST)
	{
		if(stricmp(pReq->postfix,".cgi")==0||stricmp(pReq->postfix,".exe")==0)  	//1,扩展名为.exe/.cgi
		{	
			//此处加上对Post信息的处理,涉及进程通信.
			//准备CGI环境变量,第一次都要对所有的变量进行初始化,读取得脏数据。
			//同时因为环境变量是全局共享的，所以此处应该处理并发连接		
			//先获取.exe/.cgi所在的文件名
			//路径名+文件名即为pCurrentPath当前路径.	
			m_csEnvVar.Lock();
			CString csPrepareEnv;
			if(pReq->contentLength > 0)
			{
				csPrepareEnv.Format("%s%d","CONTENT_LENGTH=",pReq->contentLength);
				putenv(csPrepareEnv.GetBuffer(0));
			}
				
			//调用CGI程序并得到运行结果,一定要给pOutMsg进行初始化
			memset(pReq->pOutMsg,0,1024*4);
			ExecCGI(pReq);
			
			pHttpProtocol->SendHeader(pReq);
			pHttpProtocol->SendFile(pReq);
			m_csEnvVar.Unlock();
			
		}
		else																	//2,扩展名为其他.
		{
			//post的处理请求&扩展名为非.cgi,非.exe

		}

	}//end METHOD_POST
	else
	{
		//除HEAD,GET,POST外的其他请求方式.[扩展用...]
		
	}
	//	else if(pReq->nMethod == METHOD_POST)
}

/**************************************************************
* 函数名: SendHeader(PREQUEST pReq)
* 
* 功能: 回馈消息头部.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpProtocol::SendHeader(PREQUEST pReq)
{
    int n = FileExist(pReq);
	if(0 == n)		// 文件不存在，则返回
	{
		// Send "404 Not Found" error
		SendError(pReq, HTTP_STATUSNO_NOTFOUND);
		return;
	}

	char Header[2048] = " ";
	char curTime[50] = " ";
	GetCurentTime((char*)curTime);

	// 取得文件长度
    DWORD length;
	char ContenType[50] = " ";

	if(METHOD_POST == pReq->nMethod || ((METHOD_GET == pReq->nMethod)&&\
		(stricmp(pReq->postfix,".cgi")==0||stricmp(pReq->postfix,".exe")==0)))
	{	
		CString strTmp;
		strTmp.Format("%s",pReq->pOutMsg);
		int iBegin = strlen("Content-Type:");
		int iEnd = strTmp.Find("\r\n");
		
		CString strType;
		strType = strTmp.Mid(iBegin,iEnd-iBegin);
		strcpy(ContenType,strType.GetBuffer(0));

		memset(m_chMsgKernel,0,1024);
		CString strOutMsg;
		strOutMsg.Format("%s",pReq->pOutMsg);
		
		int nPos = strOutMsg.Find("\r\n");
		int nLast = strOutMsg.Find('\0');
		
		strOutMsg = strOutMsg.Mid(nPos+2,nLast-nPos-1);
		int nStart = strOutMsg.Find("\r\n");
		strOutMsg = strOutMsg.Mid(nStart+2,nLast-nStart-1);		         //跳过2个字节空行
		strcpy(m_chMsgKernel,strOutMsg.GetBuffer(0));
		length = strlen(m_chMsgKernel);             //post方式的特殊处理.
	}
	else
	{
		length = GetFileSize(pReq->hFile, NULL);
		// 取得文件的类型
		GetContenType(pReq, (char*)ContenType);
	}
	
	// 取得文件的last-modified时间
	char last_modified[60] = " ";
	GetLastModified(pReq->hFile, (char*)last_modified);	



	sprintf((char*)Header, "HTTP/1.0 %s\r\nDate: %s\r\nServer: %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nLast-Modified: %s\r\n\r\n",
			                    HTTP_STATUS_OK, 
								curTime,				// Date
								"My Http Server",       // Server
								ContenType,				// Content-Type
								length,					// Content-length
								last_modified);			// Last-Modified

    // 发送头部
	send(pReq->Socket, Header, strlen(Header), 0);	
}

/**************************************************************
* 函数名: FileExist(PREQUEST pReq)
* 
* 功能: 判断文件是否存在.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
int CHttpProtocol::FileExist(PREQUEST pReq)
{
	pReq->hFile = CreateFile(pReq->szFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	// 如果文件不存在，则返回出错信息
	if (pReq->hFile == INVALID_HANDLE_VALUE)
	{
		strcpy(pReq->StatuCodeReason, "404");
		return 0;
	}
	else 
	{
		strcpy(pReq->StatuCodeReason,HTTP_STATUS_OK);
		return 1;
	}
}

/**************************************************************
* 函数名: SendFile(PREQUEST pReq)
* 
* 功能: 发送回馈消息体.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpProtocol::SendFile(PREQUEST pReq)
{
	int bFileExist = FileExist(pReq);
	if(!bFileExist)			// 文件不存在，则返回
	{	
		//文件不存在的处理方式(new add).
		SendMessage(m_hwndDlg,LIST_MSG,UINT(pReq),NULL);
		return;
	}

	//考虑此处的转换,将文件信息在list列表框显示.
	char* pStr = new char[MAX_PATH];
	pStr = &pReq->szFileName[strlen(m_strRootDir)];
	strcpy(pReq->szFileName,pStr);
	SendMessage(m_hwndDlg,LIST_MSG,UINT(pReq),NULL);
	
	static BYTE buf[1024*4];
    memset(buf,0,1024*4);

	DWORD  dwRead = 0;
    BOOL   fRet;
	int flag = 1;

    // 读写数据直到完成
    while(1)
	{	
		// 从file中读入到buffer中
		if(METHOD_POST == pReq->nMethod || ((METHOD_GET == pReq->nMethod)&&\
		(stricmp(pReq->postfix,".cgi")==0||stricmp(pReq->postfix,".exe")==0)))
		{	
			strcpy((char*)buf,m_chMsgKernel);                  //post的处理.2013-1-14
			dwRead = strlen(m_chMsgKernel);
			// 将buffer内容传送给client
			if (!SendBuffer(pReq, buf, dwRead))	
			{
				break;
			}
			break;
		}	
		else
		{
			fRet = ReadFile(pReq->hFile, buf, sizeof(buf), &dwRead, NULL);
			
			if (!fRet)
			{	
				//2012-1-6用sendError替代
				// Send "500 Internal server" error
				SendError(pReq, HTTP_STATUSNO_SERVERERROR);	
				break;
			}
			// 将buffer内容传送给client
			if (!SendBuffer(pReq, buf, dwRead))	
			{
				break;
			}
			
			// 完成
			if (dwRead == 0)
			{	
				break;
			}
		}
		
		
		pReq->dwSend += dwRead;
	}
    // 关闭文件
	if (CloseHandle(pReq->hFile))
	{
		pReq->hFile = INVALID_HANDLE_VALUE;
	}
	else
	{
		CString *pStr = new CString;
		*pStr = "Error occurs when closing file";
		SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
	}
}

/**************************************************************
* 函数名: SendBuffer(PREQUEST pReq, LPBYTE pBuf, DWORD dwBufSize)
* 
* 功能: 实际调用send函数
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
bool CHttpProtocol::SendBuffer(PREQUEST pReq, LPBYTE pBuf, DWORD dwBufSize)
{   
	// 发送缓存中的内容
	WSABUF			wsabuf;
	WSAOVERLAPPED	over;
	DWORD			dwRecv;
	DWORD			dwFlags;
	DWORD			dwRet;
	HANDLE			hEvents[2];
	BOOL			fPending;
	int				nRet;
	wsabuf.buf  = (char *)pBuf;
	wsabuf.len  = dwBufSize;
	over.hEvent = WSACreateEvent();
	fPending = false;
	
	// 发送数据 
	nRet = WSASend(pReq->Socket, &wsabuf, 1, &dwRecv, 0, &over, NULL);
	if (nRet != 0)
	{
		// 错误出理
		if (WSAGetLastError() == WSA_IO_PENDING)
		{
			fPending = true;
		}
		else
		{	
			CString *pStr = new CString;
			pStr->Format("WSASend() error: %d", WSAGetLastError() );
			SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);

			CloseHandle(over.hEvent);
			return false;
		}
	}
	if (fPending)	// i/o未完成
	{	
		hEvents[0]  = over.hEvent;
		hEvents[1]  = pReq->hExit;
		dwRet = WaitForMultipleObjects(2, hEvents, FALSE, INFINITE);
		if (dwRet != 0)
		{
			CloseHandle(over.hEvent);
			return false;
		}
		// 重叠操作未完成
		if (!WSAGetOverlappedResult(pReq->Socket, &over, &dwRecv, FALSE, &dwFlags))
		{
			// 错误处理
			CString *pStr = new CString;
			pStr->Format("WSAGetOverlappedResult() error: %d", WSAGetLastError() );
			SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
			CloseHandle(over.hEvent);
			return false;
		}
	}

	pReq->dwSend += dwRecv;

	CloseHandle(over.hEvent);
	return true;
}

/**************************************************************
* 函数名: Disconnect(PREQUEST pReq)
* 
* 功能: 断开连接.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpProtocol::Disconnect(PREQUEST pReq)
{
	// 关闭套接字：释放所占有的资源
	int	nRet;
	CString *pStr = new CString;
	pStr->Format("Closing socket: %d", pReq->Socket);
	SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
	//delete pStr;

	nRet = closesocket(pReq->Socket);
	if (nRet == SOCKET_ERROR)
	{
		// 处理错误
		CString *pStr1 = new CString;
		pStr1->Format("closesocket() error: %d", WSAGetLastError() );
		SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr1, NULL);
	}


	stats.dwRecv += pReq->dwRecv;
	stats.dwSend += pReq->dwSend;
	stats.dwConNums  += 1;

	DWORD dwX;
	DWORD dwRate = 0L;
	stats.dwElapsedTime = (GetTickCount() - pReq->dwConnectTime);

	// Convert elapsed time to seconds
	dwX = stats.dwElapsedTime/1000L; 
	if (dwX > 1)
		dwX = (pReq->dwSend+pReq->dwRecv) / dwX;
	else
		dwX = (pReq->dwSend+pReq->dwRecv);  //近似
	if (dwRate > 0)
		dwRate = (dwRate+dwX)/2;    
	else
		dwRate = dwX;
	stats.dwTransRate = dwRate;
	SendMessage(m_hwndDlg, DATA_MSG, (UINT)&stats, NULL);

}

/**************************************************************
* 函数名: InitClientCount()
* 
* 功能: 创建事件对象，计数赋初值0.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
HANDLE CHttpProtocol::InitClientCount()
{
	ClientNum = 0;
	// 创建"no client"事件对象
	None = CreateEvent(NULL, TRUE, TRUE, NULL);	
	return None;
}

/**************************************************************
* 函数名: CountUp()
* 
* 功能: 互斥访问，计数加1.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpProtocol::CountUp()
{
	// 进入排斥区
	m_critSect.Lock();
	ClientNum++;
	// 离开排斥区
	m_critSect.Unlock();
	// 重置为无信号事件对象
	ResetEvent(None);
}

/**************************************************************
* 函数名: CountDown()
* 
* 功能: 互斥访问，计数减1.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpProtocol::CountDown()
{	
	// 进入排斥区
	m_critSect.Lock();
	if(ClientNum > 0)
	{
		ClientNum--;
	} 

	// 离开排斥区
	m_critSect.Unlock();
	if(ClientNum < 1)
	{
		// 重置为有信号事件对象
		SetEvent(None);
	}
}

void CHttpProtocol::DeleteClientCount()
{
	CloseHandle(None);
}

/**************************************************************
* 函数名: GetCurentTime(LPSTR lpszString)
* 
* 功能: 活动本地时间格式化.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpProtocol::GetCurentTime(LPSTR lpszString)
{  
	// 活动本地时间
	SYSTEMTIME st;
	GetLocalTime(&st);
	// 时间格式化
    wsprintf(lpszString, "%s %02d %s %d %02d:%02d:%02d GMT",week[st.wDayOfWeek], st.wDay,month[st.wMonth-1],
     st.wYear, st.wHour, st.wMinute, st.wSecond);
}

/**************************************************************
* 函数名: GetLastModified(HANDLE hFile, LPSTR lpszString)
* 
* 功能: 获取最近修改稿的时间，完成时间格式转换.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
bool CHttpProtocol::GetLastModified(HANDLE hFile, LPSTR lpszString)
{
	// 获得文件的last-modified 时间
	FILETIME ftCreate, ftAccess, ftWrite;
    SYSTEMTIME stCreate;
	FILETIME ftime;	
	// 获得文件的last-modified的UTC时间
    if (!GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite))   
	{
		return false;
	}
	FileTimeToLocalFileTime(&ftWrite,&ftime);
	// UTC时间转化成本地时间
    FileTimeToSystemTime(&ftime, &stCreate);	
	// 时间格式化
	wsprintf(lpszString, "%s %02d %s %d %02d:%02d:%02d GMT", week[stCreate.wDayOfWeek],
		stCreate.wDay, month[stCreate.wMonth-1], stCreate.wYear, stCreate.wHour,
		stCreate.wMinute, stCreate.wSecond);
	return true;
}

/**************************************************************
* 函数名: GetContenType(PREQUEST pReq, LPSTR type)
* 
* 功能: 获取内容类型对应map中第2个关键索引对应的值.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
bool CHttpProtocol::GetContenType(PREQUEST pReq, LPSTR type)
{   
	// 取得文件的类型
    CString cpToken;
    cpToken = strstr(pReq->szFileName, ".");
    strcpy(pReq->postfix, cpToken);
	// 遍历搜索该文件类型对应的content-type
	map<CString, char *>::iterator it = m_typeMap.find(pReq->postfix);
	if(it != m_typeMap.end()) 	
	{
		wsprintf(type,"%s",(*it).second);
	}
	return TRUE;
}

/**************************************************************
* 函数名: StopHttpSrv()
* 
* 功能: 停止服务器,关闭相关操作。
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpProtocol::StopHttpSrv()
{

	int nRet;
	SetEvent(m_hExit);
	nRet = closesocket(m_listenSocket);
	nRet = WaitForSingleObject((HANDLE)m_pListenThread, 10000);
	if (nRet == WAIT_TIMEOUT)
	{
		CString *pStr = new CString;
		*pStr = "TIMEOUT waiting for ListenThread";
		SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
		//delete pStr;
	}
	CloseHandle(m_hExit);

	CString *pStr1 = new CString;
	*pStr1 = "******** HttpServer is Stopping now! *********";
	SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr1, NULL);
}

/**************************************************************
* 函数名: SendError(PREQUEST pReq, UINT uError)
* 
* 功能: 发送错误报告信息。
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
void CHttpProtocol::SendError(PREQUEST pReq, UINT uError)
{
	int nRet = 0;
	static char szMsg[512];
	static char *szStatMsgs [] = {
									"200 OK",
									"201 Created",
									"202 Accepted",
									"204 No Content",
									"301 Moved Permanently",
									"302 Moved Temporarily",
									"304 Not Modified",
									"400 Bad Request",
									"401 Unauthorized",
									"403 Forbidden",
									"404 Not Found",
									"500 Internal Server Error",
									"501 Not Implemented",
									"502 Bad Gateway",
									"503 Service Unavailable"
								};

	#define NUMSTATMSGS sizeof(szStatMsgs) / sizeof(szStatMsgs[0])
	
	if (uError < 0 || uError > NUMSTATMSGS)
		return;
	
	wsprintf(szMsg, "<body><h1>%s</h1></body>",\
		szStatMsgs[uError]);

	SendBuffer(pReq,(unsigned char*)szMsg,strlen(szMsg));
	pReq->dwSend += strlen(szMsg);

}

/**************************************************************
* 函数名: Write(void* pData, int nSize)
* 
* 功能: 将配置根目录的文件写入m_pData中.
*
* 档案历史: 2013-01-30 新生成
**************************************************************/
int CHttpProtocol::Write(void* pData, int nSize) 
{
	ASSERT(pData && nSize > 0);
	int nLeft = m_nBufferSize - m_nWritePos;
	
	// 空间不够,继续分配.
	if(nLeft < nSize)
	{
		int nNewIncSize = UNIT_BUFFER_SIZE;
		while( nNewIncSize < nSize) 
		{
			nNewIncSize += UNIT_BUFFER_SIZE;
		}
		
		if( m_nBufferSize + nNewIncSize > MAX_BUFFER_SIZE)
		{
			// 超过最大长度.
			ASSERT(0);
			return 0;
		}
		
		char* pTmp = m_pData;
		m_pData = new char[m_nBufferSize + nNewIncSize];
		memset(m_pData,0,m_nBufferSize+nNewIncSize);
		if(m_pData != NULL)
		{
			memcpy(m_pData, pTmp, m_nWritePos);
			delete[] pTmp;
			m_nBufferSize += nNewIncSize;
		}
		else
		{
			// 无法分配到内存
			ASSERT(0);
			m_pData = pTmp;
			return 0;
		}
	}
	
	// 空间分配好了之后,复制数据
	memcpy(m_pData + m_nWritePos, pData, nSize);
	m_nWritePos += nSize;

	return nSize;
}

int CHttpProtocol::WriteString(const char* pszString)
{
	return Write((void *)pszString, strlen(pszString));
}

/**************************************************************************
* 函数名: OpenDir(const std::string &strUrl, const std::string &strFilePath)
* 
* 功能: 便利根目录文件,并Html格式化处理.
*
* 档案历史: 2013-01-30 新生成
***************************************************************************/
BOOL CHttpProtocol::OpenDir(const std::string &strUrl, const std::string &strFilePath)
{
//	ASSERT(m_pData == NULL);

	char buffer[_MAX_PATH + 100] = {0};
	char sizeBuf[_MAX_PATH + 100] = {0};

	// 1. 输出HTML头,并指定UTF-8编码格式
	WriteString("<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"/></head>");
	WriteString("<body>");

	// 2. 输出路径
	//(1). 输出第一项 根目录
	WriteString("<A href=\"/\">/</A>");

	//(2). 其它目录
	std::string::size_type st = 1;
	std::string::size_type stNext = 1;
	while( (stNext = strUrl.find(L'/', st)) != std::string::npos)
	{
		std::string strDirName =  strUrl.substr(st, stNext - st + 1);
		std::string strSubUrl  = strUrl.substr(0, stNext + 1);

		WriteString("&nbsp;|&nbsp;");

		WriteString("<A href=\"");
		WriteString(strSubUrl.c_str());
		WriteString("\">");
		WriteString(strDirName.c_str());
		WriteString("</A>");
		
		// 下一个目录
		st = stNext + 1;
	}
	WriteString("<br /><hr />");

	// 3. 列出当前目录下的所有文件
	std::string strFullName;
	strFullName = strFilePath;
	if(strFullName[strFullName.length()] != '\\')
	{
		strFullName += '\\'; // 如果不是以'\\'结尾的文件路径,则补齐. 
	}
	strFullName += "*";

	std::string strFiles(""); // 普通文件写在这个字符串中.

	_finddata_t fileinfo;
	long  findRet = _findfirst(strFullName.c_str(), &fileinfo);
	if( -1 != findRet )
	{
		do 
		{
			// 跳过 . 文件
			if( stricmp((char*)(fileinfo.name), ".") == 0 || 0 == stricmp((char*)(fileinfo.name), "..") || \
				0 == stricmp((char*)(fileinfo.name), "HttpServerLog.txt") )
			{
				continue;
			}

			// 跳过系统文件和隐藏文件
			if( fileinfo.attrib & _A_SYSTEM || fileinfo.attrib & _A_HIDDEN )
			{
				continue;
			}

			// 输出子目录或者
			if( fileinfo.attrib & _A_SUBDIR )
			{
				// 如果是子目录,直接写入

				// 最后修改时间  bug
				CTime time(fileinfo.time_write );
				CString strTime = time.Format("%a %b %d %H:%M:%S %Y\n");
				WriteString(strTime.GetBuffer(0));

				// 目录名需要转换为UTF8编码
				wsprintf(buffer, "%s/", fileinfo.name);
				std::string fileurl = strUrl.c_str();
				std::string filename = buffer;

				WriteString("&nbsp;&nbsp;");
				WriteString("<A href=\"");
				WriteString(fileurl.c_str());
				WriteString(filename.c_str());
				WriteString("\">");
				WriteString(filename.c_str());
				WriteString("</A>");

				// 写入目录标志
				WriteString("&nbsp;&nbsp;[DIR]");

				// 换行
				WriteString("<br />");
			}
			else
			{
				// 普通文件,写入到一个缓冲的字符串string变量内,循环外再合并.这样,所有的目录都在前面,文件在后面
				CTime time(fileinfo.time_write );
				CString strTime = time.Format("%a %b %d %H:%M:%S %Y\n");
				string strBuffer(strTime.GetBuffer(0));
				strFiles += strBuffer;

				// 文件名转换为UTF8编码再写入
				std::string filename(fileinfo.name);
				std::string fileurl = strUrl;

				strFiles += "&nbsp;&nbsp;";
				strFiles += "<A href=\"";
				strFiles += fileurl;
				strFiles += filename;
				strFiles += "\">";
				strFiles += filename;
				strFiles += "</A>";

				// 文件大小
				double filesize = 0;
				if( fileinfo.size >= G_BYTES)
				{
					filesize = (fileinfo.size * 1.0) / G_BYTES;
					sprintf(sizeBuf, "%.2f&nbsp;GB", filesize);
				}
				else if( fileinfo.size >= M_BYTES ) // MB
				{
					filesize = (fileinfo.size * 1.0) / M_BYTES;
					sprintf(sizeBuf, "%.2f&nbsp;MB", filesize);
				}
				else if( fileinfo.size >= K_BYTES ) //KB
				{
					filesize = (fileinfo.size * 1.0) / K_BYTES;
					sprintf(sizeBuf, "%.2f&nbsp;KB", filesize);
				}
				else // Bytes
				{
					sprintf(sizeBuf, "%lld&nbsp;Bytes", fileinfo.size);
				}
			
				strFiles += "&nbsp;&nbsp;";
				strFiles += sizeBuf;

				// 换行
				strFiles += "<br />";
			}
		} while ( -1 != _findnext(findRet, &fileinfo));
		
		_findclose(findRet);
	}

	// 把文件字符串写入到 Content 中.
	if(strFiles.size() > 0)
	{
		WriteString(strFiles.c_str());
	}

	// 4. 输出结束标志.
	WriteString("</body></html>");

	return TRUE;
}

