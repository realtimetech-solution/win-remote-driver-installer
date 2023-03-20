#include <wrdi.h>

int runService(IN_ADDR* hostAddress, int port, wchar_t* workingDirectory)
{
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		printf("Error: Failure WSAStartup.\r\n");

		return 1;
	}

	SOCKET serverSocket = socket(PF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN socketAddress = { 0 };

	memcpy(&socketAddress.sin_addr, hostAddress, sizeof(IN_ADDR));
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons(port);

	if (bind(serverSocket, (SOCKADDR*)&socketAddress, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		printf("Error: Failure socket bind.\r\n");

		return 1;
	}

	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		printf("Error: Failure socket listen.\r\n");

		return 1;
	}

	printf("Info: Socket Started..\r\n");

	while (1)
	{
		int clientAddressLength = sizeof(SOCKADDR_IN);
		char clientAddressString[INET_ADDRSTRLEN];
		SOCKADDR_IN clientAddress = { 0 };

		printf("Info: Waiting for client..\r\n");

		SOCKET clientSocket = accept(serverSocket, (SOCKADDR*)&clientAddress, &clientAddressLength);

		if (clientSocket == SOCKET_ERROR)
		{
			printf("Error: Failure socket accept.\r\n");

			return 1;
		}

		inet_ntop(AF_INET, &(clientAddress.sin_addr), clientAddressString, INET_ADDRSTRLEN);
		printf("Info: Connected Client %s:%d..\r\n", clientAddressString, clientAddress.sin_port);

		char preparePacketBuffer[5];

		if (recvBytes(clientSocket, preparePacketBuffer, 5) == SOCKET_ERROR)
		{
			printf("Error: Failure receive prepare packet.\r\n");

			continue;
		}

		int fileCount = (int)preparePacketBuffer;
		char installationMode = preparePacketBuffer[4];

		printf("Info: Start receving %d files..\r\n", fileCount);

		for (int fileIndex = 0; fileIndex < fileCount; fileIndex++)
		{
			char fileNameSizeBuffer[4];
			if (recvBytes(clientSocket, fileNameSizeBuffer, 4) == SOCKET_ERROR)
			{
				printf("Error: Failure receive file name size.\r\n");

				break;
			}

			int fileNameSize = (int)fileNameSizeBuffer;

			if (fileNameSize < 1)
			{
				printf("Error: Invalid file name size.\r\n");

				break;
			}

			wchar_t fileName[MAX_PATH] = { 0 };
			char* fileNameBuffer = (char*)fileName;

			if (recvBytes(clientSocket, fileNameBuffer, fileNameSize) == SOCKET_ERROR)
			{
				printf("Error: Failure receive file name.\r\n");

				break;
			}

			fileNameBuffer[fileNameSize] = '\0';

			char fileSizeBuffer[4];

			if (recvBytes(clientSocket, fileSizeBuffer, 4) == SOCKET_ERROR)
			{
				printf("Error: Failure receive file size.\r\n");

				break;
			}

			int fileSize = (int)fileSizeBuffer;

			if (fileSize < 1)
			{
				printf("Error: Invalid file size.\r\n");

				break;
			}

			char* fileData = malloc(sizeof(char) * fileSize);

			if (recvBytes(clientSocket, fileData, fileSize) == SOCKET_ERROR)
			{
				printf("Error: Failure receive file data.\r\n");
				free(fileData);

				break;
			}

			wchar_t* fileNameUnicode = (wchar_t*)fileName;
			wchar_t fullPath[MAX_PATH];

			if (PathCombineW(fullPath, workingDirectory, fileNameUnicode) == NULL)
			{
				printf("Error: Failure path combine.\r\n");
				free(fileData);

				break;
			}

			wchar_t directoryPath[MAX_PATH];
			wcscpy_s(directoryPath, MAX_PATH, fullPath);

			if (!PathRemoveFileSpecW(directoryPath))
			{
				printf("Error: Failure remove file name in full path.\r\n");
				free(fileData);

				break;
			}

			if (!createDirectories(directoryPath))
			{
				printf("Error: Failure create sub directories.\r\n");
				free(fileData);

				break;
			}

			HANDLE fileHandle = CreateFileW(fullPath, 
											GENERIC_WRITE, 
											0, 
											NULL, 
											CREATE_NEW,
											FILE_ATTRIBUTE_NORMAL,
											NULL);

			if (fileHandle == INVALID_HANDLE_VALUE)
			{
				printf("Error: Failure open file write handle.\r\n");
				free(fileData);

				break;
			}

			DWORD writtenBytes = 0;

			if (!WriteFile(fileHandle, fileData, fileSize, &writtenBytes, NULL))
			{
				printf("Error: Failure write file.\r\n");
				free(fileData);

				break;
			}

			if (writtenBytes != fileSize)
			{
				printf("Error: Failure write all data to file.\r\n");
				free(fileData);

				break;
			}

			if (writtenBytes != fileSize)
			{
				printf("Error: Failure write all data to file.\r\n");
				free(fileData);

				break;
			}

			if (!CloseHandle(fileHandle))
			{
				printf("Error: Failure close file handle.\r\n");
				free(fileData);

				break;
			}

			free(fileData);
		}
	}

	if (closesocket(serverSocket) == SOCKET_ERROR)
	{
		printf("Error: Failure socket close.\r\n");
	}

	if (WSACleanup() != 0)
	{
		printf("Error: Failure WSACleanup.\r\n");
	}

	return 0;
}


int wmain(int argc, wchar_t** argv)
{
	if (argc != 4)
	{
		printf("Usage: wrdi-server.exe <Host> <Port> <Working Directory>\r\n");
		return -1;
	}

	wchar_t* host = argv[1];
	IN_ADDR hostAddress = { 0 };

	if (InetPtonW(AF_INET, host, &hostAddress) <= 0)
	{
		printf("Error: Given host is not valid.\r\n");

		return -1;
	}

	wchar_t* portArgument = argv[2];
	int port = wcstol(portArgument, NULL, 10);

	if (port == 0)
	{
		if (wcslen(portArgument) != 1 || portArgument[0] != '0')
		{
			printf("Error: Given port number is not integer.\r\n");

			return -1;
		}
	}

	wchar_t* workingDirectory = argv[3];
	DWORD attributes = GetFileAttributesW(workingDirectory);

	if (attributes == INVALID_FILE_ATTRIBUTES)
	{
		printf("Error: Not valid working directory.\r\n");

		return -1;
	}

	if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		printf("Error: Given working directory is not directory.\r\n");

		return -1;
	}

	return runService(&hostAddress, port, workingDirectory);
}