#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <string.h>
#include <memory.h>

#include <sys/stat.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

void concatArrays(void* array1, int size1, void* array2, int size2, void* destination) {
	memcpy(destination, array1, size1);
	memcpy(((byte*)destination) + size1, array2, size2);
}


bool createDirectories(char* path)
{
	char* buffer = new char[1];
	int previousLength = 0;
	buffer[0] = '\0';

	char* remainPath = NULL;
	char* splitedPath = strtok_s(path, "\\/", &remainPath);

	while (splitedPath != NULL)
	{
		int directoryNameLength = strlen(splitedPath);

		if (directoryNameLength < 1) {
			delete[] buffer;

			return false;
		}

		int newNameLength = previousLength + directoryNameLength + 2;
		char* tempBuffer = new char[newNameLength];

		concatArrays(buffer, previousLength, splitedPath, directoryNameLength, tempBuffer);
		tempBuffer[newNameLength - 2] = '\\';
		tempBuffer[newNameLength - 1] = '\0';

		delete[] buffer;

		buffer = tempBuffer;
		previousLength = newNameLength - 1;
		
		printf("%s\r\n", buffer);
		splitedPath = strtok_s(NULL, "\\/", &remainPath);
	}

	delete[] buffer;

	return true;
}

int recvBytes(SOCKET socket, char* buffer, int length)
{
	int offset = 0;
	
	while (offset < length) {
		int ret = recv(socket, buffer + offset, length - offset, 0);
		
		if (ret < 0) {
			return ret;
		}

		offset += ret;
	}

	return length;
}

int runService(char* workingDirectory, IN_ADDR* hostAddress, int port) {
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		printf("Error: WSAStartup failed.\r\n");

		return 1;
	}

	SOCKET serverSock = socket(PF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN socketAddress;

	memcpy(&socketAddress.sin_addr, hostAddress, sizeof(IN_ADDR));
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons(port);

	if (bind(serverSock, (SOCKADDR*)&socketAddress, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		printf("Error: Socket bind failed.\r\n");

		return 1;
	}

	if (listen(serverSock, SOMAXCONN) == SOCKET_ERROR)
	{
		printf("Error: Socket listen failed.\r\n");

		return 1;
	}

	printf("Info: Socket Started..\r\n");

	while (1)
	{
		int clientAddressLength = sizeof(SOCKADDR_IN);
		char clientAddressString[INET_ADDRSTRLEN];
		SOCKADDR_IN clientAddress;

		SOCKET clientSocket = accept(serverSock, (SOCKADDR*)&clientAddress, &clientAddressLength);

		if (clientSocket == SOCKET_ERROR) {
			printf("Error: Socket accept failed.\r\n");

			return 1;
		}
	
		inet_ntop(AF_INET, &(clientAddress.sin_addr), clientAddressString, INET_ADDRSTRLEN);
		printf("Info: Connected Client %s:%d..\r\n", clientAddressString, clientAddress.sin_port);

		char preparePacketBuffer[5];

		if (recvBytes(clientSocket, preparePacketBuffer, 5) == SOCKET_ERROR) {
			printf("Error: Failure receive prepare packet.\r\n");

			return 1;
		}
		
		int fileCount = (int)preparePacketBuffer;
		char installationMode = preparePacketBuffer[4];

		printf("Info: Start receving %d files..\r\n", fileCount);

		for (int fileIndex = 0; fileIndex < fileCount; fileIndex++) {
			char fileNameLengthBuffer[4];
			if (recvBytes(clientSocket, fileNameLengthBuffer, 4) == SOCKET_ERROR) {
				printf("Error: Failure receive file name length.\r\n");
				return 1;
			}

			int fileNameLength = (int) fileNameLengthBuffer;

			if (fileNameLength < 1) {
				printf("Error: Invalid file name length.\r\n");
			}

			char* fileName = new char[fileNameLength + 1];

			if (recvBytes(clientSocket, fileName, fileNameLength) == SOCKET_ERROR) {
				printf("Error: Failure receive file name.\r\n");
				delete[] fileName;

				return 1;
			}

			fileName[fileNameLength] = '\0';

			char fileSizeBuffer[4];

			if (recvBytes(clientSocket, fileSizeBuffer, 4) == SOCKET_ERROR) {
				printf("Error: Failure receive file size.\r\n");
				delete[] fileName;

				return 1;
			}

			int fileSize = (int)fileSizeBuffer;

			if (fileSize < 1) {
				printf("Error: Invalid file size.\r\n");
				delete[] fileName;

				return 1;
			}

			char* fileData = new char[fileSize];

			if (recvBytes(clientSocket, fileName, fileSize) == SOCKET_ERROR) {
				printf("Error: Failure receive file data.\r\n");
				delete[] fileName;
				delete[] fileData;

				return 1;
			}

			delete[] fileName;
			delete[] fileData;
		}
	}

	if (closesocket(serverSock) == SOCKET_ERROR)
	{
		printf("Error: Socket close failed.\r\n");
	}

	if (WSACleanup() != 0)
	{
		printf("Error: WSACleanup failed.\r\n");
	}

	return 0;
}


int main(int argc, char** argv) {
	if (argc != 4) {
		printf("Usage: server.exe <Working Directory> <Host> <Port>\r\n");
		return -1;
	}

	char* workingDirectory = argv[1];

	struct stat workingDirectoryStat;

	if (stat(workingDirectory, &workingDirectoryStat) != 0)
	{
		printf("Error: Not valid working directory.\r\n");

		return -1;
	}

	if (workingDirectoryStat.st_mode & S_IFREG)
	{
		printf("Error: Given working directory path is file.\r\n");

		return -1;
	}

	if (!(workingDirectoryStat.st_mode & S_IFDIR))
	{
		printf("Error: Given working directory path is werid.\r\n");

		return -1;
	}

	char* host = argv[2];
	IN_ADDR hostAddress;

	if (inet_pton(AF_INET, host, &hostAddress) <= 0) {
		printf("Error: Given host is not valid.\r\n");

		return -1;
	}

	char* portArgument = argv[3];
	int port = atoi(portArgument);

	if (port == 0) {
		if (strlen(portArgument) != 1 || portArgument[0] != '0') {
			printf("Error: Given port number is not integer.\r\n");

			return -1;
		}
	}

	char aa[] = "a/b/c/d";
	createDirectories(aa);
	return runService(workingDirectory, &hostAddress, port);
}