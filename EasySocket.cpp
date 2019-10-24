#include "EasySocket.h"
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 65507

class EasySocketHelper
{
public:
	EasySocketHelper()
	{
		recvBuffer = new char[DEFAULT_BUFLEN];
	}
	~EasySocketHelper()
	{
		delete[] recvBuffer;
	}
	WSADATA wsaData;
	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo* result = NULL, * ptr = NULL, hints{};
	char* recvBuffer = nullptr;
};

EasySocket::~EasySocket()
{
	if (helper->ConnectSocket != INVALID_SOCKET)
	{
		int iResult = shutdown(helper->ConnectSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			printf("shutdown failed with error: %s\n", GetErrorStr().c_str());
			closesocket(helper->ConnectSocket);
			WSACleanup();
		}
	}
	delete helper;
}

EasySocket::EasySocket(const char* port, const char* address, bool isUrl)
{
	helper = new EasySocketHelper();
	int iResult = WSAStartup(MAKEWORD(2, 2), &helper->wsaData);

	if (iResult != 0) {
		printf("WSAStartup failed with error: %s\n", GetErrorStr().c_str());
		return;
	}

	helper->hints.ai_family = isUrl ? AF_INET : AF_UNSPEC;
	helper->hints.ai_socktype = SOCK_STREAM;
	helper->hints.ai_protocol =  IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(address, port, &helper->hints, &helper->result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %s\n", GetErrorStr().c_str());
		WSACleanup();
		return;
	}

	// Attempt to connect to an address until one succeeds
	for (helper->ptr = helper->result; helper->ptr != NULL; helper->ptr = helper->ptr->ai_next) {

		// Create a SOCKET for connecting to server
		helper->ConnectSocket = socket(helper->ptr->ai_family, helper->ptr->ai_socktype,
			helper->ptr->ai_protocol);
		if (helper->ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %s\n", GetErrorStr().c_str());
			WSACleanup();
			return;
		}

		// Connect to server.
		iResult = connect(helper->ConnectSocket, helper->ptr->ai_addr, (int)helper->ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(helper->ConnectSocket);
			helper->ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(helper->result);

	if (helper->ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return;
	}
	okay = true;

	
}

EasySocket::EasySocket(const char* port)
{
	helper = new EasySocketHelper();

	int iResult = WSAStartup(MAKEWORD(2, 2), &helper->wsaData);

	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return;
	}

	ZeroMemory(&helper->hints, sizeof(helper->hints));
	helper->hints.ai_family = AF_INET;
	helper->hints.ai_socktype = SOCK_STREAM;
	helper->hints.ai_protocol = IPPROTO_TCP;
	helper->hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, port, &helper->hints, &helper->result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %s\n", GetErrorStr().c_str());
		WSACleanup();
		return;
	}

	// Create a SOCKET for connecting to server
	helper->ListenSocket = socket(helper->result->ai_family, helper->result->ai_socktype, helper->result->ai_protocol);
	if (helper->ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %s\n", GetErrorStr().c_str());
		freeaddrinfo(helper->result);
		WSACleanup();
		return;
	}

	// Setup the TCP listening socket
	iResult = bind(helper->ListenSocket, helper->result->ai_addr, (int)helper->result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %s\n", GetErrorStr().c_str());
		freeaddrinfo(helper->result);
		closesocket(helper->ListenSocket);
		WSACleanup();
		return;
	}

	freeaddrinfo(helper->result);

	iResult = listen(helper->ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error:  %s\n", GetErrorStr().c_str());
		closesocket(helper->ListenSocket);
		WSACleanup();
		return;
	}
	okay = true;
}

bool EasySocket::ListenAndAccept()
{
	struct sockaddr_in sa = { 0 };
	socklen_t socklen = sizeof(sockaddr);

	// Accept a client socket
	helper->ConnectSocket = accept(helper->ListenSocket, (struct sockaddr*) & sa, &socklen);
	if (helper->ConnectSocket == INVALID_SOCKET) {
		printf("accept failed with error:  %s\n", GetErrorStr().c_str());
		closesocket(helper->ListenSocket);
		WSACleanup();
		return false;
	}

	char address[256];
	inet_ntop(sa.sin_family, &sa.sin_addr, address,sizeof(address));

	connectionIp = address;

	// No longer need server socket
	closesocket(helper->ListenSocket);
	return true;
}

std::vector<char> EasySocket::ReceiveBytes()
{
	std::vector<char> data;
	int iResult = 0;

	timeval timeout = { 10, 0 };
	fd_set in_set;

	FD_ZERO(&in_set);
	FD_SET(helper->ConnectSocket, &in_set);

	while (!select(helper->ConnectSocket, &in_set, NULL, NULL, &timeout)) {};

	char headerBuffer[11];
	if (FD_ISSET(helper->ConnectSocket, &in_set))
	{
		int numBytes = recv(helper->ConnectSocket, (char*)&headerBuffer, sizeof(headerBuffer) - 1, 0);
		if (numBytes == -1)
		{
			printf("recv failed with error: %s\n", GetErrorStr().c_str());
			closesocket(helper->ConnectSocket);
			WSACleanup();
			return data;
		}
	}
	else
	{
		printf("recv failed with timeout");
		return data;
	}

	int packetSize = atoi(headerBuffer);

	data.reserve(packetSize);

	do {

		select(helper->ConnectSocket, &in_set, NULL, NULL, &timeout);

		if (FD_ISSET(helper->ConnectSocket, &in_set))
		{
			iResult = recv(helper->ConnectSocket, helper->recvBuffer, DEFAULT_BUFLEN, 0);

			if (iResult > 0) {
				data.insert(data.end(), &helper->recvBuffer[0], &helper->recvBuffer[iResult]);
				packetSize -= iResult;
			}
			else if (iResult == 0)
				break;
			else {
				printf("recv failed with error: %s\n", GetErrorStr().c_str());
				closesocket(helper->ConnectSocket);
				WSACleanup();
				return std::vector<char>();
			}
		}
		else
			break;

	} while (packetSize > 0);

	return data;
}


std::string EasySocket::ReceiveStr()
{
	std::string data;

	int iResult = 0;

	timeval timeout = { 10, 0 };
	fd_set in_set;

	FD_ZERO(&in_set);
	FD_SET(helper->ConnectSocket, &in_set);

	while (!select(helper->ConnectSocket, &in_set, NULL, NULL, &timeout)) {};

	char headerBuffer[11];
	if (FD_ISSET(helper->ConnectSocket, &in_set))
	{
		int numBytes = recv(helper->ConnectSocket, (char*)&headerBuffer, sizeof(headerBuffer) - 1, 0);
		if (numBytes == -1)
		{
			printf("recv failed with error: %s\n", GetErrorStr().c_str());
			closesocket(helper->ConnectSocket);
			WSACleanup();
			return data;
		}
	}
	else
	{
		printf("recv failed with timeout");
		return data;
	}

	int packetSize = atoi(headerBuffer);

	data.reserve(packetSize);

	do {

		select(helper->ConnectSocket, &in_set, NULL, NULL, &timeout);

		if (FD_ISSET(helper->ConnectSocket, &in_set))
		{
			iResult = recv(helper->ConnectSocket, helper->recvBuffer, DEFAULT_BUFLEN, 0);

			if (iResult > 0) {
				data.insert(data.end(), &helper->recvBuffer[0], &helper->recvBuffer[iResult]);
				packetSize -= iResult;
			}
			else if (iResult == 0)
				break;
			else {
				printf("recv failed with error: %s\n", GetErrorStr().c_str());
				closesocket(helper->ConnectSocket);
				WSACleanup();
				return std::string();
			}
		}
		else
			break;

	} while (packetSize > 0);

	return data;
}

bool EasySocket::Send(char* data, int size)
{
	int sizeRemaning = size;
	const char* curData = (const char*)data;

	timeval timeout = { 10, 0 };
	fd_set in_set;

	FD_ZERO(&in_set);
	FD_SET(helper->ConnectSocket, &in_set);

	while (!select(helper->ConnectSocket, NULL, &in_set, NULL, &timeout)) {};

	if (!FD_ISSET(helper->ConnectSocket, &in_set))
	{
		printf("send failed because timeout\n");
		return false;
	}

	char header[11];
	sprintf_s(header, "%s", std::to_string(size).c_str());
	if (send(helper->ConnectSocket, header, sizeof(header) - 1, 0) == SOCKET_ERROR) {
		printf("send failed with error: %s\n", GetErrorStr().c_str());
		closesocket(helper->ConnectSocket);
		WSACleanup();
		return false;
	}

	do {

		int iSendResult = send(helper->ConnectSocket, curData, sizeRemaning, 0);

		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %s\n", GetErrorStr().c_str());
			closesocket(helper->ConnectSocket);
			WSACleanup();
			return false;
		}

		sizeRemaning -= iSendResult;
		curData += iSendResult;

	} while (sizeRemaning > 0);

	return true;
}

bool EasySocket::Send(std::string str)
{
	int sizeRemaning = str.size();
	const char* curData = (const char*)str.data();

	timeval timeout = { 10, 0 };
	fd_set in_set;

	FD_ZERO(&in_set);
	FD_SET(helper->ConnectSocket, &in_set);

	while (!select(helper->ConnectSocket, NULL, &in_set, NULL, &timeout)) {};

	if (!FD_ISSET(helper->ConnectSocket, &in_set))
	{
		printf("send failed because timeout\n");
		return false;
	}

	char header[11];
	sprintf_s(header, "%s", std::to_string(sizeRemaning).c_str());
	if (send(helper->ConnectSocket, header, sizeof(header) - 1, 0) == SOCKET_ERROR) {
		printf("send failed with error: %s\n", GetErrorStr().c_str());
		closesocket(helper->ConnectSocket);
		WSACleanup();
		return false;
	}

	do {

		int iSendResult = send(helper->ConnectSocket, curData, sizeRemaning, 0);

		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %s\n", GetErrorStr().c_str());
			closesocket(helper->ConnectSocket);
			WSACleanup();
			return false;
		}

		sizeRemaning -= iSendResult;
		curData += iSendResult;

	} while (sizeRemaning > 0);

	return true;
}

std::string EasySocket::GetErrorStr()
{
	char buffer[256];

	int size = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buffer,
		sizeof(buffer),
		NULL);

	return std::string(buffer, size);
}

bool EasySocket::IsOk()
{
	return okay;
}