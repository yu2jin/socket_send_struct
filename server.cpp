/*--------------------------------------------------------------------
IOCP Proactor 방식의 고성능 비동기 I/O Notification Model

IO 작업에서 동시 수행되는 스레드 개수의 상한을 설정하여 CPU 자원을 효율적으로 사용
1. IOCP 생성 CreateIoCompletionPort()
2. IO 장치와 IOPC 연결 CreateoCompletionPort
3. IO 작업 완료 후 완료된 IO에 대한 처리를 수행할 스레드 풀 구성(프로세서 개수의 2배)
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

typedef struct { //소켓 정보
	SOCKET hClntSock;
	SOCKADDR_IN clntAddr;
	int client_imei;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct { //소켓 버퍼 정보
	OVERLAPPED overlapped; //현재 완료된 입출력 정보
	char buffer[BUFSIZE];
	bool Incoming_data;
	WSABUF wsaBuf; //WSASend WSARecv 함수의 인자로 전달되는 버퍼에 사용되는 구조체
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

bool client[MAX_Client] = { false }; // 클라이언트 처리
unsigned int __stdcall CompletionThread(LPVOID pComPort); //완료된 스레드 처리
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

	//Completion Port 생성
	hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	//Completion Port 에서 입출력 완료를 대기하는 스레드를 CPU 개수만큼 생성
	GetSystemInfo(&SystemInfo);
	for (i = 0; i < SystemInfo.dwNumberOfProcessors; i++)
		_beginthreadex(NULL, 0, CompletionThread, (LPVOID)hCompletionPort, 0, NULL);

	//소켓 생성
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

		// 연결된 클라이언트의 소켓 핸들 정보와 주소 정보를 설정
		PerHandleData = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
		PerHandleData->hClntSock = hClntSock;
		memcpy(&(PerHandleData->clntAddr), &clntAddr, addrLen);

		// Overlapped 소켓과 CompletionPort 연결
		CreateIoCompletionPort((HANDLE)hClntSock, hCompletionPort, (DWORD)PerHandleData, 0);

		// 연결된 클라이언트를 위한 버퍼를 설정하고 OVERLAPPED 구조체 변수 초기화
		PerIoData = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
		for (int i = 0; i < MAX_Client; ++i) {
			if (client[i] == false) {
				client[i] = true;
				PerHandleData->client_imei = i;
				printf("Client%d connected\n", PerHandleData->client_imei);
				break;
			}
		}

		//PerIoData 소켓 버퍼정보를 지니는 구조체
		memset(&(PerIoData->overlapped), 0, sizeof(OVERLAPPED));
		PerIoData->wsaBuf.len = BUFSIZE;
		PerIoData->wsaBuf.buf = PerIoData->buffer;
		PerIoData->Incoming_data = Recv_Mode;

		// 데이터 수신
		WSARecv(PerHandleData->hClntSock, // 데이터 입력 소켓
			&(PerIoData->wsaBuf),  // 데이터 입력 버퍼포인터
			1,       // 데이터 입력 버퍼의 수
			(LPDWORD)&RecvBytes,
			(LPDWORD)&Flags,
			&(PerIoData->overlapped), // OVERLAPPED 구조체 포인터
			NULL
		);
	}
	return 0;
}

//입출력 완료에 따른 스레드의 행동 정의
unsigned int __stdcall CompletionThread(LPVOID pComPort) {
	HANDLE hCompletionPort = (HANDLE)pComPort;
	DWORD BytesTransferred;
	LPPER_HANDLE_DATA PerHandleData;
	LPPER_IO_DATA PerIoData;
	DWORD flags=0;
	int clntnum;

	while (1) {
		// 입출력이 완료된 소켓의 정보
		GetQueuedCompletionStatus(hCompletionPort,    // Completion Port
			&BytesTransferred,   // 전송된 바이트수
			(LPDWORD)&PerHandleData,
			(LPOVERLAPPED*)&PerIoData, // OVERLAPPED 구조체 포인터
			INFINITE
		);

		//클라이언트 종료
		if (BytesTransferred == 0) {
			clntnum = PerHandleData->client_imei;
			printf("client%d disconnected\n", clntnum);
			client[clntnum] = false;
			closesocket(PerHandleData->hClntSock);
			free(PerHandleData);
			free(PerIoData);

			continue;
		}

		//수신 데이터 구조체화
		PerIoData->wsaBuf.buf[BytesTransferred] = '\0';
		clntnum = PerHandleData->client_imei;
		server_data.data[clntnum] = (Data_Socket&)PerIoData->buffer;
		printf("Recv%d, %s\n", server_data.data[clntnum].num, &server_data.data[clntnum].string );

		//데이터 전송
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