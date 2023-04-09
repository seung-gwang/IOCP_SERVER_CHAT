#include "stdafx.h"
#include <winsock2.h>
#pragma comment(lib, "ws2_32")
#include <windows.h>
#include <list>
#include <iterator>

typedef struct _USERSESSION
{
	SOCKET	hSocket;
	char	buffer[8192];	//8KB
} USERSESSION;


#define MAX_THREAD_CNT		4

CRITICAL_SECTION  g_cs;				
std::list<SOCKET>	g_listClient;	
SOCKET	g_hSocket;					
HANDLE	g_hIocp;				


void SendMessageAll(char *pszMessage, int nSize)
{
	std::list<SOCKET>::iterator it;

	::EnterCriticalSection(&g_cs);
	for (it = g_listClient.begin(); it != g_listClient.end(); ++it)
		::send(*it, pszMessage, nSize, 0);
	::LeaveCriticalSection(&g_cs);
}


void CloseAll()
{
	std::list<SOCKET>::iterator it;

	::EnterCriticalSection(&g_cs);
	for (it = g_listClient.begin(); it != g_listClient.end(); ++it)
	{
		::shutdown(*it, SD_BOTH);
		::closesocket(*it);
	}
	::LeaveCriticalSection(&g_cs);
}

void CloseClient(SOCKET hSock)
{
	::shutdown(hSock, SD_BOTH);
	::closesocket(hSock);

	::EnterCriticalSection(&g_cs);
	g_listClient.remove(hSock);
	::LeaveCriticalSection(&g_cs);
}

void ReleaseServer(void)
{
	CloseAll();
	::Sleep(500);

	::shutdown(g_hSocket, SD_BOTH);
	::closesocket(g_hSocket);
	g_hSocket = NULL;

	::CloseHandle(g_hIocp);
	g_hIocp = NULL;

	::Sleep(500);
	::DeleteCriticalSection(&g_cs);
}


BOOL CtrlHandler(DWORD dwType)
{
	if (dwType == CTRL_C_EVENT)
	{
		ReleaseServer();

		puts("======서버를 종료합니다======");
		::WSACleanup();
		exit(0);
		return TRUE;
	}

	return FALSE;
}

DWORD WINAPI ThreadComplete(LPVOID pParam)
{
	DWORD			dwTransferredSize = 0;
	DWORD			dwFlag = 0;
	USERSESSION		*pSession = NULL;
	LPWSAOVERLAPPED	pWol = NULL;
	BOOL			bResult;

	puts("IOCP 작업 스레드 시작");
	while (1)
	{
		bResult = ::GetQueuedCompletionStatus(
			g_hIocp,				
			&dwTransferredSize,		
			(PULONG_PTR)&pSession,	
			&pWol,					
			INFINITE);				

		if (bResult == TRUE)
		{
			if (dwTransferredSize == 0)
			{
				
				CloseClient(pSession->hSocket);
				delete pWol;
				delete pSession;
				puts("\t클라이언트 연결 종료");
			}
			else
			{
				SendMessageAll(pSession->buffer, dwTransferredSize);
				memset(pSession->buffer, 0, sizeof(pSession->buffer));

				DWORD dwReceiveSize	= 0;
				DWORD dwFlag		= 0;
				WSABUF wsaBuf		= { 0 };
				wsaBuf.buf = pSession->buffer;
				wsaBuf.len = sizeof(pSession->buffer);

				::WSARecv(
					pSession->hSocket,	
					&wsaBuf,			
					1,					
					&dwReceiveSize,
					&dwFlag,
					pWol,
					NULL);
				if (::WSAGetLastError() != WSA_IO_PENDING)
					puts("\tGQCS: ERROR: WSARecv()");
			}
		}
		else
		{
			
			if (pWol == NULL)
			{
				puts("\tGQCS: IOCP 핸들이 닫혔습니다.");
				break;
			}
			else
			{
				if (pSession != NULL)
				{
					CloseClient(pSession->hSocket);
					delete pWol;
					delete pSession;
				}

				puts("\tGQCS: 서버 종료 혹은 비정상적 연결 종료");
			}
		}
	}

	puts("IOCP 작업자 스레드 종료");
	return 0;
}


DWORD WINAPI ThreadAcceptLoop(LPVOID pParam)
{
	LPWSAOVERLAPPED	pWol = NULL;
	DWORD			dwReceiveSize, dwFlag;
	USERSESSION		*pNewUser;
	int				nAddrSize = sizeof(SOCKADDR);
	WSABUF			wsaBuf;
	SOCKADDR		ClientAddr;
	SOCKET			hClient;
	int				nRecvResult = 0;

	while ((hClient = ::accept(g_hSocket,
						&ClientAddr, &nAddrSize)) != INVALID_SOCKET)
	{
		puts("클라이언트가 연결됨!");
		::EnterCriticalSection(&g_cs);
		g_listClient.push_back(hClient);
		::LeaveCriticalSection(&g_cs);

		pNewUser = new USERSESSION;
		::ZeroMemory(pNewUser, sizeof(USERSESSION));
		pNewUser->hSocket = hClient;

		pWol = new WSAOVERLAPPED;
		::ZeroMemory(pWol, sizeof(WSAOVERLAPPED));

		::CreateIoCompletionPort( (HANDLE)hClient, g_hIocp,
			(ULONG_PTR)pNewUser,		
			0);

		dwReceiveSize = 0;
		dwFlag = 0;
		wsaBuf.buf = pNewUser->buffer;
		wsaBuf.len = sizeof(pNewUser->buffer);

		nRecvResult = ::WSARecv(hClient, &wsaBuf, 1, &dwReceiveSize,
							&dwFlag, pWol, NULL);
		if (::WSAGetLastError() != WSA_IO_PENDING)
			puts("ERROR: WSARecv() != WSA_IO_PENDING");
	}

	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	WSADATA wsa = { 0 };
	if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		puts("ERROR: 윈속을 초기화 할 수 없습니다.");
		return 0;
	}

	::InitializeCriticalSection(&g_cs);

	if (::SetConsoleCtrlHandler(
			(PHANDLER_ROUTINE)CtrlHandler, TRUE) == FALSE)
		puts("ERROR: Ctrl+C 처리기를 등록할 수 없습니다.");

	g_hIocp = ::CreateIoCompletionPort(
		INVALID_HANDLE_VALUE,	
		NULL,			
		0,				
		0);				
	if (g_hIocp == NULL)
	{
		puts("ERORR: IOCP를 생성할 수 없습니다.");
		return 0;
	}

	HANDLE hThread;
	DWORD dwThreadID;
	for (int i = 0; i < MAX_THREAD_CNT; ++i)
	{
		dwThreadID = 0;
		hThread = ::CreateThread(NULL,	
			0,				
			ThreadComplete,	
			(LPVOID)NULL,	
			0,				
			&dwThreadID);	

		::CloseHandle(hThread);
	}

	g_hSocket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
						NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN addrsvr;
	addrsvr.sin_family = AF_INET;
	addrsvr.sin_addr.S_un.S_addr = ::htonl(INADDR_ANY);
	addrsvr.sin_port = ::htons(25000);

	if (::bind(g_hSocket,
			(SOCKADDR*)&addrsvr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		puts("ERROR: 포트가 이미 사용중입니다.");
		ReleaseServer();
		return 0;
	}

	if (::listen(g_hSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		puts("ERROR:  리슨 상태로 전환할 수 없습니다.");
		ReleaseServer();
		return 0;
	}

	hThread = ::CreateThread(NULL, 0, ThreadAcceptLoop,
					(LPVOID)NULL, 0, &dwThreadID);
	::CloseHandle(hThread);

	puts("*** 채팅서버를 시작합니다! ***");
	while (1)
		getchar();

	return 0;
}


