#pragma once
#include <string>
#include <vector>

class EasySocketHelper;

class EasySocket
{
public:

	~EasySocket();

	// Client
	EasySocket(const char* port, const char* address, bool isUrl = false);

	//  Server
	EasySocket(const char* port);

	bool ListenAndAccept();

	std::vector<char> ReceiveBytes();

	std::string ReceiveStr();

	// Sends a buffer of data
	bool Send(char* data, int size);

	// Sends a string
	bool Send(std::string str);

	std::string connectionIp; // The ip of the connection

	std::string GetErrorStr();

	bool IsOk();

	EasySocketHelper * helper = nullptr; // Helper class used to avoid Winsock Include hell

	bool okay = false;
};

