/*--------------------------------------------------------------------
IOCP Proactor ����� ���� �񵿱� I/O Notification Model

IO �۾����� ���� ����Ǵ� ������ ������ ������ �����Ͽ� CPU �ڿ��� ȿ�������� ���
1. IOCP ���� CreateIoCompletionPort()
2. IO ��ġ�� IOPC ���� CreateoCompletionPort
3. IO �۾� �Ϸ� �� �Ϸ�� IO�� ���� ó���� ������ ������ Ǯ ����(���μ��� ������ 2��)
https://www.slideshare.net/namhyeonuk90/iocp
http://windowshyun.tistory.com/6
https://blog.naver.com/0421cjy/220676239722
--------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib") 

#define _CRT_SECURE_NO_WARNINGS

#define BUFSIZE 1024
#define MAX_Client 5
#define Recv_Mode 0
#define Send_Mode 1

typedef struct { //���� ����
	SOCKET hClntSock;
	SOCKADDR_IN clntAddr;
	int client_imei;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct { //���� ���� ����
	OVERLAPPED overlapped; //���� �Ϸ�� ����� ����
	char buffer[BUFSIZE];
	bool Incoming_data;
	WSABUF wsaBuf; //WSASend WSARecv �Լ��� ���ڷ� ���޵Ǵ� ���ۿ� ���Ǵ� ����ü
} PER_IO_DATA, *LPPER_IO_DATA;

struct SOCKETINFO {
	OVERLAPPED overlapped;
	SOCKET sock;
	char buf[BUFSIZE + 1];
	bool Incoming_data;
	WSABUF wsabuf;
};

typedef struct Data_Socket {
	int num;
	char *string;
} Data_Socket;
Data_Socket struct_data;

typedef struct Server_Data {
	Data_Socket data[MAX_Client];
} Server_Data;
Server_Data server_data;

bool client[MAX_Client] = { false }; // Ŭ���̾�Ʈ ó��
unsigned int __stdcall CompletionThread(LPVOID pComPort); //�Ϸ�� ������ ó��
void ErrorHandling(char *message);

int main(int argc, char** argv) {
	WSADATA wsaData;
	HANDLE hCompletionPort;
	SYSTEM_INFO SystemInfo;
	SOCKADDR_IN servAddr;
	LPPER_IO_DATA PerIoData;
	LPPER_HANDLE_DATA PerHandleData;

	SOCKET hServSock;
	int RecvBytes;
	int i, Flags=0;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error!");

	//Completion Port ����
	hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	//Completion Port ���� ����� �ϷḦ ����ϴ� �����带 CPU ������ŭ ����
	GetSystemInfo(&SystemInfo);
	for (i = 0; i < SystemInfo.dwNumberOfProcessors; i++)
		_beginthreadex(NULL, 0, CompletionThread, (LPVOID)hCompletionPort, 0, NULL);

	//���� ����
	hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(atoi("9000"));

	bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr));
	listen(hServSock, 5);

	while (1) {
		SOCKET hClntSock;
		SOCKADDR_IN clntAddr;
		int addrLen = sizeof(clntAddr);

		hClntSock = accept(hServSock, (SOCKADDR*)&clntAddr, &addrLen);

		// ����� Ŭ���̾�Ʈ�� ���� �ڵ� ������ �ּ� ������ ����
		PerHandleData = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
		PerHandleData->hClntSock = hClntSock;
		memcpy(&(PerHandleData->clntAddr), &clntAddr, addrLen);

		// Overlapped ���ϰ� CompletionPort ����
		CreateIoCompletionPort((HANDLE)hClntSock, hCompletionPort, (DWORD)PerHandleData, 0);

		// ����� Ŭ���̾�Ʈ�� ���� ���۸� �����ϰ� OVERLAPPED ����ü ���� �ʱ�ȭ
		PerIoData = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
		for (int i = 0; i < MAX_Client; ++i) {
			if (client[i] == false) {
				client[i] = true;
				PerHandleData->client_imei = i;
				printf("Client%d connected\n", PerHandleData->client_imei);
				break;
			}
		}

		//PerIoData ���� ���������� ���ϴ� ����ü
		memset(&(PerIoData->overlapped), 0, sizeof(OVERLAPPED));
		PerIoData->wsaBuf.len = BUFSIZE;
		PerIoData->wsaBuf.buf = PerIoData->buffer;
		PerIoData->Incoming_data = Recv_Mode;

		// ������ ����
		WSARecv(PerHandleData->hClntSock, // ������ �Է� ����
			&(PerIoData->wsaBuf),  // ������ �Է� ����������
			1,       // ������ �Է� ������ ��
			(LPDWORD)&RecvBytes,
			(LPDWORD)&Flags,
			&(PerIoData->overlapped), // OVERLAPPED ����ü ������
			NULL
		);
	}
	return 0;
}

//����� �Ϸῡ ���� �������� �ൿ ����
unsigned int __stdcall CompletionThread(LPVOID pComPort) {
	HANDLE hCompletionPort = (HANDLE)pComPort;
	DWORD BytesTransferred;
	LPPER_HANDLE_DATA PerHandleData;
	LPPER_IO_DATA PerIoData;
	DWORD flags=0;
	int clntnum;

	while (1) {
		// ������� �Ϸ�� ������ ����
		GetQueuedCompletionStatus(hCompletionPort,    // Completion Port
			&BytesTransferred,   // ���۵� ����Ʈ��
			(LPDWORD)&PerHandleData,
			(LPOVERLAPPED*)&PerIoData, // OVERLAPPED ����ü ������
			INFINITE
		);

		//Ŭ���̾�Ʈ ����
		if (BytesTransferred == 0) {
			clntnum = PerHandleData->client_imei;
			printf("client%d disconnected\n", clntnum);
			client[clntnum] = false;
			closesocket(PerHandleData->hClntSock);
			free(PerHandleData);
			free(PerIoData);

			continue;
		}

		//���� ������ ����üȭ
		PerIoData->wsaBuf.buf[BytesTransferred] = '\0';
		clntnum = PerHandleData->client_imei;
		server_data.data[clntnum] = (Data_Socket&)PerIoData->buffer;
		printf("Recv%d, %s\n", server_data.data[clntnum].num, &server_data.data[clntnum].string );

		//������ ����
		PerIoData->wsaBuf.len = BytesTransferred;
		WSASend(PerHandleData->hClntSock, &(PerIoData->wsaBuf), 1, NULL, 0, NULL, NULL);

		// RECEIVE AGAIN
		memset(&(PerIoData->overlapped), 0, sizeof(OVERLAPPED));
		PerIoData->wsaBuf.len = BUFSIZE;
		PerIoData->wsaBuf.buf = PerIoData->buffer;

		WSARecv(PerHandleData->hClntSock, &(PerIoData->wsaBuf), 1, NULL, &flags, &(PerIoData->overlapped), NULL);
	}
	return 0;
}

void ErrorHandling(char *message) {
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}