#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib") 

#define BUFSIZE    512
#define Recv_Mode 0
#define Send_Mode 1
#define MAX_Client 4

void ErrorHandling(char *message);

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

int main() {
	WSADATA wsaData;
	SOCKET hSocket;
	SOCKADDR_IN recvAddr;

	SOCKETINFO dataBuf;
	Server_Data *data;
	char message[1024] = { 0, };
	int sendBytes = 0;
	int recvBytes = 0;
	int flags = 0;

	WSAEVENT event;
	WSAOVERLAPPED overlapped;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error!");

	hSocket = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (hSocket == INVALID_SOCKET)
		ErrorHandling("socket() error");

	memset(&recvAddr, 0, sizeof(recvAddr));
	recvAddr.sin_family = AF_INET;
	recvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	recvAddr.sin_port = htons(atoi("9000"));

	if (connect(hSocket, (SOCKADDR*)&recvAddr, sizeof(recvAddr)) == SOCKET_ERROR)
		ErrorHandling("connect() error!");

	//구조체에 이벤트 핸들 삽입해서 전달
	event = WSACreateEvent();
	memset(&overlapped, 0, sizeof(overlapped));

	overlapped.hEvent = event;
	dataBuf.Incoming_data = Send_Mode;

	// 전송할 데이터
	while (1) {
		printf("Input data <integer> <string> :\n");
		scanf("%d %s", &struct_data.num, &struct_data.string);

		//데이터 전송
		if (dataBuf.Incoming_data == Send_Mode) {
			printf("Send to server : %d, %s\n", struct_data.num, &struct_data.string);
			dataBuf.wsabuf.len = sizeof(Data_Socket);
			dataBuf.wsabuf.buf = (char*)&struct_data;
			dataBuf.Incoming_data = Recv_Mode;

			if (WSASend(hSocket, &dataBuf.wsabuf, 1, (LPDWORD)&sendBytes, 0, &overlapped, NULL) == SOCKET_ERROR) {
				if (WSAGetLastError() != WSA_IO_PENDING)
					ErrorHandling("WSASend() error");
			}

			//전송 완료 확인
			WSAWaitForMultipleEvents(1, &event, TRUE, WSA_INFINITE, FALSE); //데이터 전송이 끝났는지 확인
			WSAGetOverlappedResult(hSocket, &overlapped, (LPDWORD)&sendBytes, FALSE, NULL); //실제로 전송된 바이트 수
		}  

		Sleep(50);

		//데이터 수신
		if (dataBuf.Incoming_data == Recv_Mode) {
			memset(&(overlapped), 0, sizeof(OVERLAPPED));
			dataBuf.wsabuf.len = BUFSIZE;
			dataBuf.wsabuf.buf = dataBuf.buf;
			
			if (WSARecv(hSocket, &dataBuf.wsabuf, 1, (LPDWORD)&recvBytes, (LPDWORD)&flags, &overlapped, NULL) == SOCKET_ERROR) {
				if (WSAGetLastError() != WSA_IO_PENDING)
					ErrorHandling("WSASend() error");
			}

			dataBuf.wsabuf.buf[recvBytes] = '\0';
			data = (Server_Data*)dataBuf.wsabuf.buf;
			server_data = *data;
			printf("Recv from server : %d, %s\n", server_data.data[0].num, &server_data.data[0].string);

			dataBuf.Incoming_data = Send_Mode;

		}
	}

	closesocket(hSocket);
	WSACleanup();

	return 0;
}

void ErrorHandling(char *message) {
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}