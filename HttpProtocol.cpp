#include "stdafx.h"
#include "HttpProtocol.h"
#include <io.h>
#include <stdio.h>
#include <string>

// ��������ʱ�������ת��
char *week[] = {		
	"Sun,",  
	"Mon,",
	"Tue,",
	"Wed,",
	"Thu,",
	"Fri,",
	"Sat,",
};
 
// ��������ʱ����·�ת��
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
CCriticalSection CHttpProtocol::m_critSect;		// �ų�����ʼ��
HANDLE	CHttpProtocol::None = NULL;
CCriticalSection CHttpProtocol::m_csEnvVar;     // ���������ų�����ʼ��.
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
* ������: StartHttpSrv()
* 
* ����: ����������,�������󶨡������߳̿���.
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
bool CHttpProtocol::StartHttpSrv()
{

	WORD wVersionRequested = WINSOCK_VERSION;
	WSADATA wsaData;
	int nRet;

	// ����Winsock
	nRet = WSAStartup(wVersionRequested, &wsaData);		// ���سɹ�����0
	if (0 != nRet)
	{   
		// ������
		AfxMessageBox("Initialize WinSock Failed");
		return false;
	}
	// ���汾
	if (wsaData.wVersion != wVersionRequested)
	{    
		// ������   
		AfxMessageBox("Wrong WinSock Version");
		return false;
	}
	
	m_hExit = CreateEvent(NULL, TRUE, FALSE, NULL);	
	if (m_hExit == NULL)
	{
		return false;
	}

	//�����׽���
	m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_listenSocket == INVALID_SOCKET)
	{
		// �쳣����
		CString *pStr = new CString;
		*pStr = "Could not create listen socket";
		SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
	
		return false;
	}

	SOCKADDR_IN sockAddr;
	LPSERVENT	lpServEnt;
	if ( 0 != m_nPort)
	{
		// �������ֽ�˳��תΪ�����ֽ�˳�򸳸�sin_port
		sockAddr.sin_port = htons(m_nPort);
	}
	else
	{	
		//��ȡ��֪http����Ķ˿ڣ��÷�����tcpЭ����ע��
		lpServEnt = getservbyname("http", "tcp");
		if (lpServEnt != NULL)
		{
			sockAddr.sin_port = lpServEnt->s_port;
		}
		else
		{
			sockAddr.sin_port = htons(HTTPPORT);	// Ĭ�϶˿�HTTPPORT��80
		}
	}

	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = INADDR_ANY;  // ָ���˿ں������Ĭ��IP�ӿ� 

	// ��ʼ��content-type���ļ���׺��Ӧ��ϵ��map
    CreateTypeMap();

	// �׽��ְ�
	nRet = bind(m_listenSocket, (LPSOCKADDR)&sockAddr, sizeof(struct sockaddr));
	if (nRet == SOCKET_ERROR)
	{  
		// �󶨷�������
		CString *pStr = new CString;
		pStr->Format("bind() error code: %d",GetLastError());
		SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
		closesocket(m_listenSocket);	// �Ͽ�����
	
		return false;
	}

    // �׽��ּ�����Ϊ�ͻ����Ӵ����ȴ�����,������󳤶�SOMAXCONN��windows socketsͷ�ļ��ж���
	nRet = listen(m_listenSocket, SOMAXCONN);
	if (nRet == SOCKET_ERROR)
	{   
		// �쳣����
		CString *pStr = new CString;
		*pStr = "listen() error";
		SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
		closesocket(m_listenSocket);	// �Ͽ�����

		return false;
	}

	// ����listening���̣����ܿͻ�������Ҫ��
	m_pListenThread = AfxBeginThread(ListenThread, this);
	
	if (NULL == m_pListenThread)
	{
		// �̴߳���ʧ��
		CString *pStr = new CString;
		*pStr = "Could not create listening thread" ;
		SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
		closesocket(m_listenSocket);	// �Ͽ�����

		return false;
	}

	CString strIP, strTemp;
	char hostname[255];
	PHOSTENT hostinfo;
	// ��ȡ�������
	if(gethostname(hostname, sizeof(hostname))==0)	// �õ�������
	{
		// ���������õ�������������Ϣ
		hostinfo = gethostbyname(hostname);
		if(hostinfo != NULL)
		{
			strIP = inet_ntoa(*(struct in_addr*)*(hostinfo->h_addr_list));
		}
	}
	
	// ��ʾweb��������������
	CString *pStr = new CString;
	*pStr = "******** HttpServer is Starting now! *********";
	SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);

	// ��ʾweb����������Ϣ��������������IP�Լ��˿ں�
	CString *pStr1 = new CString;
	pStr1->Format("%s", hostname); 
	*pStr1 = *pStr1 + "[" + strIP + "]" + "   Port ";
	strTemp.Format("%d", htons(sockAddr.sin_port));
	*pStr1 = *pStr1 + strTemp;
	SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr1, NULL);

	return true;

}

/**************************************************************
* ������: CreateTypeMap()
* 
* ����: ������չ������Ӧ������.
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
void CHttpProtocol::CreateTypeMap()
{
	// ��ʼ��map
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
* ������: ListenThread(LPVOID param)
* 
* ����: �����߳�,ѭ������.
*
* ������ʷ: 2013-01-30 ������
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

	// ��ʼ��ClientNum������"no client"�¼�����
	HANDLE		hNoClients;
	hNoClients = pHttpProtocol->InitClientCount();

	while(1)	// ѭ���ȴ�,���пͻ���������,����ܿͻ�������Ҫ��
	{	
		nLen = sizeof(SOCKADDR_IN);		
		// �׽��ֵȴ�����,���ض�Ӧ�ѽ��ܵĿͻ������ӵ��׽���
		socketClient = accept(pHttpProtocol->m_listenSocket, (LPSOCKADDR)&SockAddr, &nLen);
		if (socketClient == INVALID_SOCKET)
		{   
			break;
		}		
		// ���ͻ��������ַת��Ϊ�õ�ָ��IP��ַ
		CString *pstr = new CString;
		pstr->Format("%s Connecting on socket:%d", inet_ntoa(SockAddr.sin_addr), socketClient);
		SendMessage(pHttpProtocol->m_hwndDlg, LOG_MSG, (UINT)pstr, NULL);

        pReq = new REQUEST;
		if (pReq == NULL)
		{   
			// �������
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

	    // ����client���̣�����request
		pClientThread = AfxBeginThread(ClientThread, pReq);
		if (!pClientThread)
		{  
			// �̴߳���ʧ��,������
			CString *pStr = new CString;
			*pStr = "Couldn't start client thread";
			SendMessage(pHttpProtocol->m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
			delete pReq;
		}
	} 
	//while
	// �ȴ��߳̽���
	WaitForSingleObject((HANDLE)pHttpProtocol->m_hExit, INFINITE);
    // �ȴ�����client���̽���
	dwRet = WaitForSingleObject(hNoClients, 5000);
	if (dwRet == WAIT_TIMEOUT) 
	{  
		// ��ʱ���أ�����ͬ������δ�˳�
		CString *pStr = new CString;
		*pStr = "One or more client threads did not exit";
		SendMessage(pHttpProtocol->m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
	}
	pHttpProtocol->DeleteClientCount();

	return 0;
}

/**************************************************************
* ������: ClientThread(LPVOID param)
* 
* ����: �ͻ��̣߳����ý������󡢷������󡢶Բ�ͬ���͵Ĵ���
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
UINT CHttpProtocol::ClientThread(LPVOID param)
{
	int nRet;
	BYTE buf[1024];
	memset(buf,0,1024);
	PREQUEST pReq = (PREQUEST)param;
	CHttpProtocol *pHttpProtocol = (CHttpProtocol *)pReq->pHttpProtocol;

	pHttpProtocol->CountUp();			// ����
	
	// ����request data
	if (!pHttpProtocol->RecvRequest(pReq, buf, sizeof(buf)))
	{  
		pHttpProtocol->Disconnect(pReq);
		delete pReq;
		pHttpProtocol->CountDown();      
		return 0;  
	}
	
	// ����request��Ϣr
	nRet = pHttpProtocol->Analyze(pReq, buf);
	if (nRet != 0)
	{	
		// �������
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
		//��ͬ����Method�Ĵ���ʽ.
		AnalyzeAndProcReqType(pReq);
	}

	pHttpProtocol->Disconnect(pReq);
	delete pReq;
	pHttpProtocol->CountDown();	// client������1

	return 0;
}

/**************************************************************
* ������: RecvRequest(PREQUEST pReq, LPBYTE pBuf, DWORD dwBufSize)
* 
* ����: �������󣬴���buf,���ں�������
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
bool CHttpProtocol::RecvRequest(PREQUEST pReq, LPBYTE pBuf, DWORD dwBufSize)
{
	WSABUF			wsabuf;			// ����/���ջ������ṹ
	WSAOVERLAPPED	over;			// ָ������ص�����ʱָ����WSAOVERLAPPED�ṹ
	DWORD			dwRecv;
	DWORD			dwFlags;
	DWORD			dwRet;
	HANDLE			hEvents[2];
	bool			fPending;
	int				nRet;
	memset(pBuf, 0, dwBufSize);		// ��ʼ��������
    wsabuf.buf  = (char *)pBuf;
	wsabuf.len  = dwBufSize;		// �������ĳ���
	over.hEvent = WSACreateEvent();	// ����һ���µ��¼�����
	dwFlags = 0;
	fPending = FALSE;  
	// ��������
	nRet = WSARecv(pReq->Socket, &wsabuf, 1, &dwRecv, &dwFlags, &over, NULL);

    if (nRet != 0)  
	{
    	// �������WSA_IO_PENDING��ʾ�ص������ɹ�����
		// �ص������Ѿ��ɹ���ʼ�������ں�����ʱ�����.
		if (WSAGetLastError() != WSA_IO_PENDING)
		{   
			// �ص�����δ�ܳɹ�
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
		// �ص�����δ���
		if (!WSAGetOverlappedResult(pReq->Socket, &over, &dwRecv, FALSE, &dwFlags))
		{
			CloseHandle(over.hEvent);                    
			return false;
		}
	}
	pReq->dwRecv += dwRecv;	// ͳ�ƽ�������
	CloseHandle(over.hEvent);                           
	return true;
}
	
/**************************************************************
* ������: Analyze(PREQUEST pReq, LPBYTE pBuf)
* 
* ����: ����������Ϣ������ṹ��PREQUEST��
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
int  CHttpProtocol::Analyze(PREQUEST pReq, LPBYTE pBuf)
{
	char* pBuffer = new char[strlen((char*)pBuf)+1];                //���ں�������.
	strcpy(pBuffer,(char*)pBuf);

	// �������յ�����Ϣ
	char szSeps[] = " \n";
	char *cpToken;

	// ��ֹ�Ƿ�����
	if (strstr((const char *)pBuf, "..") != NULL)
	{
		strcpy(pReq->StatuCodeReason, "400");
		strcpy(pReq->szFileName, m_strRootDir);
		strcat(pReq->szFileName,"\\..");
		// Send "bad request" error
		SendError(pReq, HTTP_STATUSNO_BADREQUEST);

		//�ļ������ʽ����.
		SendMessage(m_hwndDlg,LIST_MSG,UINT(pReq),NULL);

		return 1;
	}

	// �ж�ruquest��mothed	
	cpToken = strtok((char *)pBuf, szSeps);	// �������ַ����ֽ�Ϊһ���Ǵ���	
	if (!_stricmp(cpToken, "GET"))			// GET����
	{
		pReq->nMethod = METHOD_GET;
	}
	else if (!_stricmp(cpToken, "HEAD"))	// HEAD����
	{
		pReq->nMethod = METHOD_HEAD;  
	}
	else if(!_stricmp(cpToken, "POST"))    // POST����
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
    
	// ��ȡRequest-URI
	cpToken = strtok(NULL, szSeps);
	if (cpToken == NULL)   
	{
		strcpy(pReq->StatuCodeReason, HTTP_STATUS_BADREQUEST);

		// Send "bad request" error
		SendError(pReq, HTTP_STATUSNO_BADREQUEST);

		return 1;
	}

	m_strExePathName.Format("%s",cpToken);				//��¼get��post������ļ���. 
	string strExePathName(cpToken);
	
	//������Ϊ'/'�ĵ�������֮,����ȫ·��.
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
	else                                                   //�������Ƶĵ�������.
	{
		pReq->bHasExe = true;

		int nPos = m_strExePathName.ReverseFind ('/');     //2013-1-16 '//'��Ϊ'/'
		m_strExeFileName = m_strExePathName.Mid(0,nPos);   //�õ�'/cgi'
		
		CString strCurrentPathTmp = m_strRootDir+m_strExeFileName;
		strcpy(pReq->m_chCurrentPath,strCurrentPathTmp.GetBuffer(0));
		
		//2013-1-13��post���⴦��.
		if(METHOD_POST == pReq->nMethod)
		{	
			// ��¼Content-Length ����.
			CString strContent(pBuffer);
			int iBeginExt  = strContent.Find("Content-Length: ");
			int iBegin = iBeginExt+strlen("Content-Length: ");
			int iEndExt = strContent.Find("Connection");
			int iEnd    = iEndExt-strlen("\r\n");
			
			CString strLen = strContent.Mid(iBegin,iEnd-iBegin);
			char* chLen = strLen.GetBuffer(0);
			pReq->contentLength = atoi(chLen);                        //��ȡ�����ĳ���.
			
			//ȡ�����һ�У���������.
			CString strBuffer(pBuffer);
			int iLineFeed = strBuffer.Find("\r\n\r\n");
			int iStart = iLineFeed + strlen("\r\n\r\n");
			int iStop = strBuffer.GetLength();
			
			CString strLastLine = strBuffer.Mid(iStart,iStop-iStart);
			strcpy(pReq->parameter,(char*)strLastLine.GetBuffer(0));   //��ȡ����
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
		strcat(pReq->szFileName, cpToken);	// �Ѹ��ļ�����ӵ���β���γ�·��
	}
	else
	{
		strcat(pReq->szFileName, "/index.html");
	}
	
	//�����ļ����͵��ж�...
	CString strPostfix;
	CString strFileName(pReq->szFileName);
	int nBegin = strFileName.ReverseFind('.');
	int nLast = strFileName.GetLength();
	strPostfix = strFileName.Mid(nBegin,nLast);

	strcpy(pReq->postfix, strPostfix.GetBuffer(0));               //�˴���bug

	delete []pBuffer;
	return 0;
}

//CGIִ�г���
/*-------------------------------------------------------------------------------
---���룺pReq->szFileName,pReq->m_chCurrentPath,NULL,pReq->pOutMsg,pReq->parameter
---���½���ִ��.exe����,�ܵ�ͨ��,������outMsg��Ϣ,����send! <2013-1-10>
---������ԭ�е�����Ľ���ͨ�Ż���.
----------------------------------------------------------------------------------*/
/**************************************************************
* ������: ExecCGI(PREQUEST pReq)
* 
* ����: ��post��ʽ��Get�����exe/cgi���д������ģ�����ͨ��.
*
* ������ʷ: 2013-01-30 ������
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
		pStr->Format("�ܵ�����ʧ�ܣ�����ţ�%d",GetLastError());
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
				  pReq->m_chCurrentPath,					//CurrentDiectoryPathָ���ӽ��̵�·��--��Ҫ�Ķ�!
				  &InitInfo,
				  &ProInfo))
	{
		hParentReadPipe = NULL;
		hParentWritePipe = NULL;
		CloseHandle(hParentReadPipe);
		CloseHandle(hParentWritePipe);

		CString* pStrError = new CString;
		pStrError->Format("���̴���ʧ�ܣ������:%d",GetLastError());
		SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStrError, NULL);
		return NULL;
	}
	else
	{
		CloseHandle(ProInfo.hThread);    //�����ɹ����Ժ�رվ��?Ϊʲô?
		hProcess=ProInfo.hProcess;
	}

	//������дHTML����������POST����
	if(*pReq->parameter != '\0')
	{
		DWORD dwWrite;
		if(!WriteFile(hParentWritePipe,pReq->parameter,strlen(pReq->parameter),&dwWrite,NULL))
		{
			CString* pStrError = new CString;
			pStrError->Format(_T("д������ʧ�ܣ�����ţ�%d"),GetLastError());
			SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStrError, NULL);
			return NULL;
		}
		
	}
	
	//���³������ִ�е�ʱ�������
	ULONG nTotalPipeBytes = 0;
	char* pBuffer=new char[MaxCGIBufferSize];
	memset(pBuffer,0,MaxCGIBufferSize);
	DWORD dwRead = 0;

	while(true)
	{
		int nRtn;
		nRtn=WaitForSingleObject(hProcess,1000);
		//ͨ���ź����ȴ����е�ִ�У�����ȴ���ʱ���˳���
		if(nRtn==WAIT_TIMEOUT)
			break;

		//���û���ҵ��ܵ�����ͬ���˳�
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
* ������: AnalyzeAndProcReqType(PREQUEST pReq)
* 
* ����: �Բ�ͬ������ʽGET/HEAD/POST�ķ�ʽ����,��ͬ����.exe/.cgi���������ʹ���
*
* ������ʷ: 2013-01-30 ������
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
		//1,��չ��Ϊ.exe/.cgi
		if(stricmp(pReq->postfix,".cgi")==0||stricmp(pReq->postfix,".exe")==0)  	//1,��չ��Ϊ.exe/.cgi
		{	
			//�˴����϶�Post��Ϣ�Ĵ���,�漰����ͨ��.		
			//����CGI���򲢵õ����н��,һ��Ҫ��pOutMsg���г�ʼ��
			memset(pReq->pOutMsg,0,1024*4);
			ExecCGI(pReq);
			
			pHttpProtocol->SendHeader(pReq);
			pHttpProtocol->SendFile(pReq);
			
		}
		else if(stricmp(pReq->postfix,".asp")==0)
		{
			//��չ����,
		}
		else																	 //2,��չ��Ϊ��.exe/.cgi������
		{		
			//���ɲ�����ͷ��
			pHttpProtocol->SendHeader(pReq);
			pHttpProtocol->SendFile(pReq);
		}
	}//end METHOD_GET
	else if( pReq->nMethod == METHOD_POST)
	{
		if(stricmp(pReq->postfix,".cgi")==0||stricmp(pReq->postfix,".exe")==0)  	//1,��չ��Ϊ.exe/.cgi
		{	
			//�˴����϶�Post��Ϣ�Ĵ���,�漰����ͨ��.
			//׼��CGI��������,��һ�ζ�Ҫ�����еı������г�ʼ��,��ȡ�������ݡ�
			//ͬʱ��Ϊ����������ȫ�ֹ���ģ����Դ˴�Ӧ�ô���������		
			//�Ȼ�ȡ.exe/.cgi���ڵ��ļ���
			//·����+�ļ�����ΪpCurrentPath��ǰ·��.	
			m_csEnvVar.Lock();
			CString csPrepareEnv;
			if(pReq->contentLength > 0)
			{
				csPrepareEnv.Format("%s%d","CONTENT_LENGTH=",pReq->contentLength);
				putenv(csPrepareEnv.GetBuffer(0));
			}
				
			//����CGI���򲢵õ����н��,һ��Ҫ��pOutMsg���г�ʼ��
			memset(pReq->pOutMsg,0,1024*4);
			ExecCGI(pReq);
			
			pHttpProtocol->SendHeader(pReq);
			pHttpProtocol->SendFile(pReq);
			m_csEnvVar.Unlock();
			
		}
		else																	//2,��չ��Ϊ����.
		{
			//post�Ĵ�������&��չ��Ϊ��.cgi,��.exe

		}

	}//end METHOD_POST
	else
	{
		//��HEAD,GET,POST�����������ʽ.[��չ��...]
		
	}
	//	else if(pReq->nMethod == METHOD_POST)
}

/**************************************************************
* ������: SendHeader(PREQUEST pReq)
* 
* ����: ������Ϣͷ��.
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
void CHttpProtocol::SendHeader(PREQUEST pReq)
{
    int n = FileExist(pReq);
	if(0 == n)		// �ļ������ڣ��򷵻�
	{
		// Send "404 Not Found" error
		SendError(pReq, HTTP_STATUSNO_NOTFOUND);
		return;
	}

	char Header[2048] = " ";
	char curTime[50] = " ";
	GetCurentTime((char*)curTime);

	// ȡ���ļ�����
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
		strOutMsg = strOutMsg.Mid(nStart+2,nLast-nStart-1);		         //����2���ֽڿ���
		strcpy(m_chMsgKernel,strOutMsg.GetBuffer(0));
		length = strlen(m_chMsgKernel);             //post��ʽ�����⴦��.
	}
	else
	{
		length = GetFileSize(pReq->hFile, NULL);
		// ȡ���ļ�������
		GetContenType(pReq, (char*)ContenType);
	}
	
	// ȡ���ļ���last-modifiedʱ��
	char last_modified[60] = " ";
	GetLastModified(pReq->hFile, (char*)last_modified);	



	sprintf((char*)Header, "HTTP/1.0 %s\r\nDate: %s\r\nServer: %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nLast-Modified: %s\r\n\r\n",
			                    HTTP_STATUS_OK, 
								curTime,				// Date
								"My Http Server",       // Server
								ContenType,				// Content-Type
								length,					// Content-length
								last_modified);			// Last-Modified

    // ����ͷ��
	send(pReq->Socket, Header, strlen(Header), 0);	
}

/**************************************************************
* ������: FileExist(PREQUEST pReq)
* 
* ����: �ж��ļ��Ƿ����.
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
int CHttpProtocol::FileExist(PREQUEST pReq)
{
	pReq->hFile = CreateFile(pReq->szFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	// ����ļ������ڣ��򷵻س�����Ϣ
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
* ������: SendFile(PREQUEST pReq)
* 
* ����: ���ͻ�����Ϣ��.
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
void CHttpProtocol::SendFile(PREQUEST pReq)
{
	int bFileExist = FileExist(pReq);
	if(!bFileExist)			// �ļ������ڣ��򷵻�
	{	
		//�ļ������ڵĴ���ʽ(new add).
		SendMessage(m_hwndDlg,LIST_MSG,UINT(pReq),NULL);
		return;
	}

	//���Ǵ˴���ת��,���ļ���Ϣ��list�б����ʾ.
	char* pStr = new char[MAX_PATH];
	pStr = &pReq->szFileName[strlen(m_strRootDir)];
	strcpy(pReq->szFileName,pStr);
	SendMessage(m_hwndDlg,LIST_MSG,UINT(pReq),NULL);
	
	static BYTE buf[1024*4];
    memset(buf,0,1024*4);

	DWORD  dwRead = 0;
    BOOL   fRet;
	int flag = 1;

    // ��д����ֱ�����
    while(1)
	{	
		// ��file�ж��뵽buffer��
		if(METHOD_POST == pReq->nMethod || ((METHOD_GET == pReq->nMethod)&&\
		(stricmp(pReq->postfix,".cgi")==0||stricmp(pReq->postfix,".exe")==0)))
		{	
			strcpy((char*)buf,m_chMsgKernel);                  //post�Ĵ���.2013-1-14
			dwRead = strlen(m_chMsgKernel);
			// ��buffer���ݴ��͸�client
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
				//2012-1-6��sendError���
				// Send "500 Internal server" error
				SendError(pReq, HTTP_STATUSNO_SERVERERROR);	
				break;
			}
			// ��buffer���ݴ��͸�client
			if (!SendBuffer(pReq, buf, dwRead))	
			{
				break;
			}
			
			// ���
			if (dwRead == 0)
			{	
				break;
			}
		}
		
		
		pReq->dwSend += dwRead;
	}
    // �ر��ļ�
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
* ������: SendBuffer(PREQUEST pReq, LPBYTE pBuf, DWORD dwBufSize)
* 
* ����: ʵ�ʵ���send����
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
bool CHttpProtocol::SendBuffer(PREQUEST pReq, LPBYTE pBuf, DWORD dwBufSize)
{   
	// ���ͻ����е�����
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
	
	// �������� 
	nRet = WSASend(pReq->Socket, &wsabuf, 1, &dwRecv, 0, &over, NULL);
	if (nRet != 0)
	{
		// �������
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
	if (fPending)	// i/oδ���
	{	
		hEvents[0]  = over.hEvent;
		hEvents[1]  = pReq->hExit;
		dwRet = WaitForMultipleObjects(2, hEvents, FALSE, INFINITE);
		if (dwRet != 0)
		{
			CloseHandle(over.hEvent);
			return false;
		}
		// �ص�����δ���
		if (!WSAGetOverlappedResult(pReq->Socket, &over, &dwRecv, FALSE, &dwFlags))
		{
			// ������
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
* ������: Disconnect(PREQUEST pReq)
* 
* ����: �Ͽ�����.
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
void CHttpProtocol::Disconnect(PREQUEST pReq)
{
	// �ر��׽��֣��ͷ���ռ�е���Դ
	int	nRet;
	CString *pStr = new CString;
	pStr->Format("Closing socket: %d", pReq->Socket);
	SendMessage(m_hwndDlg, LOG_MSG, (UINT)pStr, NULL);
	//delete pStr;

	nRet = closesocket(pReq->Socket);
	if (nRet == SOCKET_ERROR)
	{
		// �������
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
		dwX = (pReq->dwSend+pReq->dwRecv);  //����
	if (dwRate > 0)
		dwRate = (dwRate+dwX)/2;    
	else
		dwRate = dwX;
	stats.dwTransRate = dwRate;
	SendMessage(m_hwndDlg, DATA_MSG, (UINT)&stats, NULL);

}

/**************************************************************
* ������: InitClientCount()
* 
* ����: �����¼����󣬼�������ֵ0.
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
HANDLE CHttpProtocol::InitClientCount()
{
	ClientNum = 0;
	// ����"no client"�¼�����
	None = CreateEvent(NULL, TRUE, TRUE, NULL);	
	return None;
}

/**************************************************************
* ������: CountUp()
* 
* ����: ������ʣ�������1.
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
void CHttpProtocol::CountUp()
{
	// �����ų���
	m_critSect.Lock();
	ClientNum++;
	// �뿪�ų���
	m_critSect.Unlock();
	// ����Ϊ���ź��¼�����
	ResetEvent(None);
}

/**************************************************************
* ������: CountDown()
* 
* ����: ������ʣ�������1.
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
void CHttpProtocol::CountDown()
{	
	// �����ų���
	m_critSect.Lock();
	if(ClientNum > 0)
	{
		ClientNum--;
	} 

	// �뿪�ų���
	m_critSect.Unlock();
	if(ClientNum < 1)
	{
		// ����Ϊ���ź��¼�����
		SetEvent(None);
	}
}

void CHttpProtocol::DeleteClientCount()
{
	CloseHandle(None);
}

/**************************************************************
* ������: GetCurentTime(LPSTR lpszString)
* 
* ����: �����ʱ���ʽ��.
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
void CHttpProtocol::GetCurentTime(LPSTR lpszString)
{  
	// �����ʱ��
	SYSTEMTIME st;
	GetLocalTime(&st);
	// ʱ���ʽ��
    wsprintf(lpszString, "%s %02d %s %d %02d:%02d:%02d GMT",week[st.wDayOfWeek], st.wDay,month[st.wMonth-1],
     st.wYear, st.wHour, st.wMinute, st.wSecond);
}

/**************************************************************
* ������: GetLastModified(HANDLE hFile, LPSTR lpszString)
* 
* ����: ��ȡ����޸ĸ��ʱ�䣬���ʱ���ʽת��.
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
bool CHttpProtocol::GetLastModified(HANDLE hFile, LPSTR lpszString)
{
	// ����ļ���last-modified ʱ��
	FILETIME ftCreate, ftAccess, ftWrite;
    SYSTEMTIME stCreate;
	FILETIME ftime;	
	// ����ļ���last-modified��UTCʱ��
    if (!GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite))   
	{
		return false;
	}
	FileTimeToLocalFileTime(&ftWrite,&ftime);
	// UTCʱ��ת���ɱ���ʱ��
    FileTimeToSystemTime(&ftime, &stCreate);	
	// ʱ���ʽ��
	wsprintf(lpszString, "%s %02d %s %d %02d:%02d:%02d GMT", week[stCreate.wDayOfWeek],
		stCreate.wDay, month[stCreate.wMonth-1], stCreate.wYear, stCreate.wHour,
		stCreate.wMinute, stCreate.wSecond);
	return true;
}

/**************************************************************
* ������: GetContenType(PREQUEST pReq, LPSTR type)
* 
* ����: ��ȡ�������Ͷ�Ӧmap�е�2���ؼ�������Ӧ��ֵ.
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
bool CHttpProtocol::GetContenType(PREQUEST pReq, LPSTR type)
{   
	// ȡ���ļ�������
    CString cpToken;
    cpToken = strstr(pReq->szFileName, ".");
    strcpy(pReq->postfix, cpToken);
	// �����������ļ����Ͷ�Ӧ��content-type
	map<CString, char *>::iterator it = m_typeMap.find(pReq->postfix);
	if(it != m_typeMap.end()) 	
	{
		wsprintf(type,"%s",(*it).second);
	}
	return TRUE;
}

/**************************************************************
* ������: StopHttpSrv()
* 
* ����: ֹͣ������,�ر���ز�����
*
* ������ʷ: 2013-01-30 ������
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
* ������: SendError(PREQUEST pReq, UINT uError)
* 
* ����: ���ʹ��󱨸���Ϣ��
*
* ������ʷ: 2013-01-30 ������
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
* ������: Write(void* pData, int nSize)
* 
* ����: �����ø�Ŀ¼���ļ�д��m_pData��.
*
* ������ʷ: 2013-01-30 ������
**************************************************************/
int CHttpProtocol::Write(void* pData, int nSize) 
{
	ASSERT(pData && nSize > 0);
	int nLeft = m_nBufferSize - m_nWritePos;
	
	// �ռ䲻��,��������.
	if(nLeft < nSize)
	{
		int nNewIncSize = UNIT_BUFFER_SIZE;
		while( nNewIncSize < nSize) 
		{
			nNewIncSize += UNIT_BUFFER_SIZE;
		}
		
		if( m_nBufferSize + nNewIncSize > MAX_BUFFER_SIZE)
		{
			// ������󳤶�.
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
			// �޷����䵽�ڴ�
			ASSERT(0);
			m_pData = pTmp;
			return 0;
		}
	}
	
	// �ռ�������֮��,��������
	memcpy(m_pData + m_nWritePos, pData, nSize);
	m_nWritePos += nSize;

	return nSize;
}

int CHttpProtocol::WriteString(const char* pszString)
{
	return Write((void *)pszString, strlen(pszString));
}

/**************************************************************************
* ������: OpenDir(const std::string &strUrl, const std::string &strFilePath)
* 
* ����: ������Ŀ¼�ļ�,��Html��ʽ������.
*
* ������ʷ: 2013-01-30 ������
***************************************************************************/
BOOL CHttpProtocol::OpenDir(const std::string &strUrl, const std::string &strFilePath)
{
//	ASSERT(m_pData == NULL);

	char buffer[_MAX_PATH + 100] = {0};
	char sizeBuf[_MAX_PATH + 100] = {0};

	// 1. ���HTMLͷ,��ָ��UTF-8�����ʽ
	WriteString("<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"/></head>");
	WriteString("<body>");

	// 2. ���·��
	//(1). �����һ�� ��Ŀ¼
	WriteString("<A href=\"/\">/</A>");

	//(2). ����Ŀ¼
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
		
		// ��һ��Ŀ¼
		st = stNext + 1;
	}
	WriteString("<br /><hr />");

	// 3. �г���ǰĿ¼�µ������ļ�
	std::string strFullName;
	strFullName = strFilePath;
	if(strFullName[strFullName.length()] != '\\')
	{
		strFullName += '\\'; // ���������'\\'��β���ļ�·��,����. 
	}
	strFullName += "*";

	std::string strFiles(""); // ��ͨ�ļ�д������ַ�����.

	_finddata_t fileinfo;
	long  findRet = _findfirst(strFullName.c_str(), &fileinfo);
	if( -1 != findRet )
	{
		do 
		{
			// ���� . �ļ�
			if( stricmp((char*)(fileinfo.name), ".") == 0 || 0 == stricmp((char*)(fileinfo.name), "..") || \
				0 == stricmp((char*)(fileinfo.name), "HttpServerLog.txt") )
			{
				continue;
			}

			// ����ϵͳ�ļ��������ļ�
			if( fileinfo.attrib & _A_SYSTEM || fileinfo.attrib & _A_HIDDEN )
			{
				continue;
			}

			// �����Ŀ¼����
			if( fileinfo.attrib & _A_SUBDIR )
			{
				// �������Ŀ¼,ֱ��д��

				// ����޸�ʱ��  bug
				CTime time(fileinfo.time_write );
				CString strTime = time.Format("%a %b %d %H:%M:%S %Y\n");
				WriteString(strTime.GetBuffer(0));

				// Ŀ¼����Ҫת��ΪUTF8����
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

				// д��Ŀ¼��־
				WriteString("&nbsp;&nbsp;[DIR]");

				// ����
				WriteString("<br />");
			}
			else
			{
				// ��ͨ�ļ�,д�뵽һ��������ַ���string������,ѭ�����ٺϲ�.����,���е�Ŀ¼����ǰ��,�ļ��ں���
				CTime time(fileinfo.time_write );
				CString strTime = time.Format("%a %b %d %H:%M:%S %Y\n");
				string strBuffer(strTime.GetBuffer(0));
				strFiles += strBuffer;

				// �ļ���ת��ΪUTF8������д��
				std::string filename(fileinfo.name);
				std::string fileurl = strUrl;

				strFiles += "&nbsp;&nbsp;";
				strFiles += "<A href=\"";
				strFiles += fileurl;
				strFiles += filename;
				strFiles += "\">";
				strFiles += filename;
				strFiles += "</A>";

				// �ļ���С
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

				// ����
				strFiles += "<br />";
			}
		} while ( -1 != _findnext(findRet, &fileinfo));
		
		_findclose(findRet);
	}

	// ���ļ��ַ���д�뵽 Content ��.
	if(strFiles.size() > 0)
	{
		WriteString(strFiles.c_str());
	}

	// 4. ���������־.
	WriteString("</body></html>");

	return TRUE;
}

