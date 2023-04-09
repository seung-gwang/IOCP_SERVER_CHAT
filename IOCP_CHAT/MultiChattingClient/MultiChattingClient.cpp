#include <stdio.h>
#include "stdafx.h"
#include <winsock2.h>

//������ ���� ������ �Է�
#define SERVER_IP_STR ("110.12.201.228") //"127.0.0.1"

using namespace std;

#pragma comment(lib, "ws2_32")

DWORD WINAPI ReceiveThread(LPVOID pParam)
{
	SOCKET hSocket = (SOCKET)pParam;
	char szBuffer[128] = { 0 };
	while (::recv(hSocket, szBuffer, sizeof(szBuffer), 0) > 0)
	{
		printf("-> %s\n", szBuffer);
		memset(szBuffer, 0, sizeof(szBuffer));
	}

	puts("ä���� ����Ǿ���.");
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	WSADATA wsa = { 0 };
	if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		puts("ERROR: ���� �ʱ�ȭ ����!");
		return 0;
	}

	SOCKET hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (hSocket == INVALID_SOCKET)
	{
		puts("ERROR: ���� ���� ����!");
		return 0;
	}

	SOCKADDR_IN	svraddr = { 0 };
	svraddr.sin_family = AF_INET;
	svraddr.sin_port = htons(25000);
	svraddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP_STR); //Loopback local Host
	if (::connect(hSocket,
		(SOCKADDR*)&svraddr, sizeof(svraddr)) == SOCKET_ERROR)
	{
		puts("ERROR: ���� ���� ����!");
		return 0;
	}

	DWORD dwThreadID = 0;
	HANDLE hThread = ::CreateThread(NULL,
		0,					
		ReceiveThread,		
		(LPVOID)hSocket,	
		0,					
		&dwThreadID);		
	::CloseHandle(hThread);

	char szBuffer[256];
	char idBuffer[128];
	
	printf("ä�ý� ����� �г����� �Է����ּ��� : ");
	
	gets_s(idBuffer);
	system("cls");

	puts("ä���� �����մϴ�. �޽����� �Է��ϼ���. 'EXIT' �Է½� ä���� �����մϴ�.");
	while (1)
	{
		char msgBuffer[128];
		
		
		memset(msgBuffer, 0, sizeof(msgBuffer));
		gets_s(msgBuffer);
	
		if (strcmp(msgBuffer, "EXIT") == 0) break;
				
		int idLen = strlen(idBuffer);
		int msgLen = strlen(msgBuffer);

		memcpy(szBuffer, idBuffer, idLen);
		szBuffer[idLen] = ':';
		szBuffer[idLen + 1] = ' ';
		memcpy(szBuffer + idLen + 2, msgBuffer, strlen(msgBuffer) + 1);
		::send(hSocket, szBuffer, strlen(szBuffer) + 1, 0);

	}

	::closesocket(hSocket);
	::Sleep(100);
	::WSACleanup();
	return 0;
}
