#ifndef _HTTPPROTOCOL_H
#define _HTTPPROTOCOL_H


#pragma once
#pragma warning(disable : 4786)    //消除STL中的警告.


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


//定格错误消息用
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

#define UNIT_BUFFER_SIZE 1024 * 10		 // HTTP的内存增长速度
#define MAX_BUFFER_SIZE	 1024 * 1024 * 5 // HTTP最大内存 5M


// 连接的Client的信息
typedef struct REQUEST
{
	HANDLE		hExit;
	SOCKET		Socket;                // 请求的socket
	int			nMethod;               // 请求的使用方法：GET或HEAD
	DWORD		dwRecv;                // 收到的字节数
	DWORD		dwSend;                // 发送的字节数
	HANDLE		hFile;                 // 请求连接的文件
	DWORD       dwConnectTime;         // 连接时间(new add 2013-1-1)
	char		szFileName[_MAX_PATH]; // 文件的相对路径: E:\FileSrcDir/testcgi.html
	char		postfix[10];           // 存储扩展名      .html/.cgi/.exe
	char        StatuCodeReason[100];  // 头部的status cod以及reason-phrase
	bool        permitted;             // 用户权限判断
	char *      authority;             // 用户提供的认证信息
	char        key[1024];             // 正确认证信息

	int			contentLength;		   // 存储Content-Length参数
	char		parameter[1024];	   // 具体最大值，需要看协议的说明,存储参数,eg: sex=male%C4%D0&name=xxxx&password=ssss
	char		m_chCurrentPath[100];  // post 用exe/cgi当前根路径(全)
	char        pOutMsg[1024*4];       // 用于Post输出信息.
	bool        bHasExe;			   // true,用扩展名; false,'/'.

	void*		pHttpProtocol;		   // 指向类CHttpProtocol的指针

}REQUEST, *PREQUEST;

typedef struct HTTPSTATS
{
	DWORD	dwRecv;               // 收到字节数
	DWORD	dwSend;               // 发送字节数
	DWORD	dwElapsedTime;        // 请求的持续时间[ms]
	DWORD   dwConNums;            // 连接个数
	DWORD   dwTransRate;          // 传输速率
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
	map<CString, char *> m_typeMap;		// 保存content-type和文件后缀的对应关系map
	CWinThread* m_pListenThread;
	HANDLE m_hExit;
	
	static HANDLE	None;				// 标志是否有Client连接到Server
	static UINT ClientNum;				// 连接的Client数量
	static CCriticalSection m_critSect; // 互斥变量
	static CCriticalSection m_csEnvVar; // 因为环境变量是全局共享的，所以此处应该处理并发连接.
	
	
	CString m_strRootDir;				// web的根目录
	UINT	m_nPort;					// http server的端口号
	CString m_strExePathName;           // post 用exe/cgi路径名.
	CString m_strExeFileName;           // post 用exe/cgi所在文件夹名

	char	m_chMsgKernel[1024];        // post 用发送核心信息.
	int		m_nBufferSize;
	int     m_nWritePos;
	char*   m_pData;

	//状态处理
	HTTPSTATS	stats;
};


#endif _HTTPPROTOCOL_H