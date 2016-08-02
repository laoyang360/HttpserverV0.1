#ifndef _HTTPPROTOCOL_H
#define _HTTPPROTOCOL_H


#pragma once
#pragma warning(disable : 4786)    //����STL�еľ���.


#include <winsock2.h>
#include <afxmt.h>

#define HTTPPORT 80
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_POST 2

#define MaxCGIBufferSize 1024

#define HTTP_STATUS_OK				"200 OK"
#define HTTP_STATUS_CREATED			"201 Created"
#define HTTP_STATUS_ACCEPTED		"202 Accepted"
#define HTTP_STATUS_NOCONTENT		"204 No Content"
#define HTTP_STATUS_MOVEDPERM		"301 Moved Permanently"
#define HTTP_STATUS_MOVEDTEMP		"302 Moved Temporarily"
#define HTTP_STATUS_NOTMODIFIED		"304 Not Modified"
#define HTTP_STATUS_BADREQUEST		"400 Bad Request"
#define HTTP_STATUS_UNAUTHORIZED	"401 Unauthorized"
#define HTTP_STATUS_FORBIDDEN		"403 Forbidden"
#define HTTP_STATUS_NOTFOUND		"404 File can not found!"
#define HTTP_STATUS_SERVERERROR		"500 Internal Server Error"
#define HTTP_STATUS_NOTIMPLEMENTED	"501 Not Implemented"
#define HTTP_STATUS_BADGATEWAY		"502 Bad Gateway"
#define HTTP_STATUS_UNAVAILABLE		"503 Service Unavailable"


//���������Ϣ��
#define HTTP_STATUSNO_OK				0
#define HTTP_STATUSNO_CREATED			1
#define HTTP_STATUSNO_ACCEPTED	    	2
#define HTTP_STATUSNO_NOCONTENT	    	3
#define HTTP_STATUSNO_MOVEDPERM	    	4
#define HTTP_STATUSNO_MOVEDTEMP		    5
#define HTTP_STATUSNO_NOTMODIFIED		6
#define HTTP_STATUSNO_BADREQUEST		7
#define HTTP_STATUSNO_UNAUTHORIZED   	8
#define HTTP_STATUSNO_FORBIDDEN	    	9
#define HTTP_STATUSNO_NOTFOUND	    	10
#define HTTP_STATUSNO_SERVERERROR		11
#define HTTP_STATUSNO_NOTIMPLEMENTED	12
#define HTTP_STATUSNO_BADGATEWAY		13
#define HTTP_STATUSNO_UNAVAILABLE		14

#define G_BYTES (1024 * 1024 * 1024) // 1GB
#define M_BYTES (1024 * 1024)		 // 1MB
#define K_BYTES 1024				 // 1KB

#define UNIT_BUFFER_SIZE 1024 * 10		 // HTTP���ڴ������ٶ�
#define MAX_BUFFER_SIZE	 1024 * 1024 * 5 // HTTP����ڴ� 5M


// ���ӵ�Client����Ϣ
typedef struct REQUEST
{
	HANDLE		hExit;
	SOCKET		Socket;                // �����socket
	int			nMethod;               // �����ʹ�÷�����GET��HEAD
	DWORD		dwRecv;                // �յ����ֽ���
	DWORD		dwSend;                // ���͵��ֽ���
	HANDLE		hFile;                 // �������ӵ��ļ�
	DWORD       dwConnectTime;         // ����ʱ��(new add 2013-1-1)
	char		szFileName[_MAX_PATH]; // �ļ������·��: E:\FileSrcDir/testcgi.html
	char		postfix[10];           // �洢��չ��      .html/.cgi/.exe
	char        StatuCodeReason[100];  // ͷ����status cod�Լ�reason-phrase
	bool        permitted;             // �û�Ȩ���ж�
	char *      authority;             // �û��ṩ����֤��Ϣ
	char        key[1024];             // ��ȷ��֤��Ϣ

	int			contentLength;		   // �洢Content-Length����
	char		parameter[1024];	   // �������ֵ����Ҫ��Э���˵��,�洢����,eg: sex=male%C4%D0&name=xxxx&password=ssss
	char		m_chCurrentPath[100];  // post ��exe/cgi��ǰ��·��(ȫ)
	char        pOutMsg[1024*4];       // ����Post�����Ϣ.
	bool        bHasExe;			   // true,����չ��; false,'/'.

	void*		pHttpProtocol;		   // ָ����CHttpProtocol��ָ��

}REQUEST, *PREQUEST;

typedef struct HTTPSTATS
{
	DWORD	dwRecv;               // �յ��ֽ���
	DWORD	dwSend;               // �����ֽ���
	DWORD	dwElapsedTime;        // ����ĳ���ʱ��[ms]
	DWORD   dwConNums;            // ���Ӹ���
	DWORD   dwTransRate;          // ��������
}HTTPSTATS, *PHTTPSTATS;


#include <map>
#include <string>
using namespace std;


class CHttpProtocol
{
public:
	CHttpProtocol(void);
	~CHttpProtocol(void);

public:
	void DeleteClientCount();
	void CountDown();
	void CountUp();
	HANDLE InitClientCount();
	
	void StopHttpSrv();
	bool StartHttpSrv();

	static UINT ListenThread(LPVOID param);
	static UINT ClientThread(LPVOID param);

	bool RecvRequest(PREQUEST pReq, LPBYTE pBuf, DWORD dwBufSize);
	int	 Analyze(PREQUEST pReq, LPBYTE pBuf);
	void Disconnect(PREQUEST pReq);
	void CreateTypeMap();
	void SendHeader(PREQUEST pReq);
	int  FileExist(PREQUEST pReq);
	
	void GetCurentTime(LPSTR lpszString);
	bool GetLastModified(HANDLE hFile, LPSTR lpszString);
	bool GetContenType(PREQUEST pReq, LPSTR type);

	void SendFile(PREQUEST pReq);
	bool SendBuffer(PREQUEST pReq, LPBYTE pBuf, DWORD dwBufSize);	
	void SendError(PREQUEST pReq, UINT uError);
	
	static void AnalyzeAndProcReqType(PREQUEST pReq);

	static char* ExecCGI(PREQUEST pReq);
	
	int	 Write(void* pData, int nSize); 
	int	 WriteString(const char* pszString);
	BOOL OpenDir(const std::string &strUrl, const std::string &strFilePath);


public:
	
	static HWND m_hwndDlg;
	SOCKET m_listenSocket;
	map<CString, char *> m_typeMap;		// ����content-type���ļ���׺�Ķ�Ӧ��ϵmap
	CWinThread* m_pListenThread;
	HANDLE m_hExit;
	
	static HANDLE	None;				// ��־�Ƿ���Client���ӵ�Server
	static UINT ClientNum;				// ���ӵ�Client����
	static CCriticalSection m_critSect; // �������
	static CCriticalSection m_csEnvVar; // ��Ϊ����������ȫ�ֹ���ģ����Դ˴�Ӧ�ô���������.
	
	
	CString m_strRootDir;				// web�ĸ�Ŀ¼
	UINT	m_nPort;					// http server�Ķ˿ں�
	CString m_strExePathName;           // post ��exe/cgi·����.
	CString m_strExeFileName;           // post ��exe/cgi�����ļ�����

	char	m_chMsgKernel[1024];        // post �÷��ͺ�����Ϣ.
	int		m_nBufferSize;
	int     m_nWritePos;
	char*   m_pData;

	//״̬����
	HTTPSTATS	stats;
};


#endif _HTTPPROTOCOL_H