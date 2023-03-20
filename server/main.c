#include <wrdi.h>

char fileBuffer[MAX_FILE_SIZE];

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
		SOCKADDR_IN clientAddress;

		printf("Info: Waiting for client..\r\n");

		SOCKET clientSocket = accept(serverSocket, (SOCKADDR*)&clientAddress, &clientAddressLength);

		if (clientSocket == SOCKET_ERROR)
		{
			printf("Error: Failure socket accept.\r\n");

			return 1;
		}

		inet_ntop(AF_INET, &(clientAddress.sin_addr), clientAddressString, INET_ADDRSTRLEN);
		printf("Info: Connected Client %s:%d..\r\n", clientAddressString, clientAddress.sin_port);

		PreparePacket preparePacket;

		if (recvBytes(clientSocket, (char*)&preparePacket, sizeof(PreparePacket)) == SOCKET_ERROR)
		{
			printf("Error: Failure receive prepare packet.\r\n");

			break;
		}

		printf("Info: Start receving %d files..\r\n", preparePacket.fileCount);

		for (uint32_t fileIndex = 0; fileIndex < preparePacket.fileCount; fileIndex++)
		{
			UploadHeaderPacket uploadHeaderPacket;

			if (recvBytes(clientSocket, (char*)&uploadHeaderPacket, sizeof(UploadHeaderPacket)) == SOCKET_ERROR)
			{
				printf("Error: Failure receive upload header packet.\r\n");

				break;
			}

			if (uploadHeaderPacket.filePathLength >= MAX_PATH)
			{
				printf("Error: Received file path length is too big.\r\n");

				break;
			}

			wchar_t filePath[MAX_PATH];

			if (recvBytes(clientSocket, (char*)&filePath, sizeof(wchar_t) * uploadHeaderPacket.filePathLength) == SOCKET_ERROR)
			{
				printf("Error: Failure receive file path.\r\n");

				break;
			}

			filePath[uploadHeaderPacket.filePathLength] = '\0';

			printf("Info: Receiving '%S' file..\r\n", filePath);

			if (uploadHeaderPacket.fileSize == 0 || uploadHeaderPacket.fileSize > MAX_FILE_SIZE)
			{
				printf("Error: Invalid file size.\r\n");

				break;
			}

			if (recvBytes(clientSocket, fileBuffer, uploadHeaderPacket.fileSize) == SOCKET_ERROR)
			{
				printf("Error: Failure receive file data.\r\n");

				break;
			}

			wchar_t fullPath[MAX_PATH];

			if (PathCombineW(fullPath, workingDirectory, filePath) == NULL)
			{
				printf("Error: Failure path combine.\r\n");

				break;
			}

			wchar_t directoryPath[MAX_PATH];
			wcscpy_s(directoryPath, MAX_PATH, fullPath);

			if (!PathRemoveFileSpecW(directoryPath))
			{
				printf("Error: Failure remove file name in full path.\r\n");

				break;
			}

			if (!createDirectories(directoryPath))
			{
				printf("Error: Failure create sub directories.\r\n");

				break;
			}


			DWORD uploadFileAttributes = GetFileAttributesW(fullPath);

			if (uploadFileAttributes != INVALID_FILE_ATTRIBUTES)
			{
				bool succeedDelete = true;

				if (uploadFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					if (removeDirectory(fullPath) != 0)
					{
						succeedDelete = false;
					}
				}
				else
				{
					if (!DeleteFileW(fullPath))
					{
						succeedDelete = false;
					}
				}

				if (!succeedDelete)
				{
					ResponsePacket responsePacket = {
						.responseState = RESPONSE_STATE_ERROR_DELETE
					};

					if (sendBytes(clientSocket, (char*)&responsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
					{
						printf("Error: Failure response packet.\r\n");
					}

					return 1;
				}
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

				ResponsePacket responsePacket = {
					.responseState = RESPONSE_STATE_ERROR_WRITE
				};

				if (sendBytes(clientSocket, (char*)&responsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
				{
					printf("Error: Failure response packet.\r\n");
				}

				break;
			}

			DWORD writtenBytes = 0;

			if (!WriteFile(fileHandle, fileBuffer, uploadHeaderPacket.fileSize, &writtenBytes, NULL))
			{
				printf("Error: Failure write file.\r\n");

				ResponsePacket responsePacket = {
					.responseState = RESPONSE_STATE_ERROR_WRITE
				};

				if (sendBytes(clientSocket, (char*)&responsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
				{
					printf("Error: Failure response packet.\r\n");
				}

				break;
			}

			if (writtenBytes != uploadHeaderPacket.fileSize)
			{
				printf("Error: Failure write all data to file.\r\n");

				ResponsePacket responsePacket = {
					.responseState = RESPONSE_STATE_ERROR_WRITE
				};

				if (sendBytes(clientSocket, (char*)&responsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
				{
					printf("Error: Failure response packet.\r\n");
				}

				break;
			}

			if (!CloseHandle(fileHandle))
			{
				printf("Error: Failure close file handle.\r\n");
			}

			ResponsePacket uploadResponsePacket = {
				.responseState = RESPONSE_STATE_SUCCESS
			};

			if (sendBytes(clientSocket, (char*)&uploadResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
			{
				printf("Error: Failure upload response packet.\r\n");
			}

			printf("Info: Received!\r\n");
		}

		printf("Info: Waiting for install signal..\r\n");

		InstallPacket installPacket;

		if (recvBytes(clientSocket, (char*)&installPacket, sizeof(InstallPacket)) == SOCKET_ERROR)
		{
			printf("Error: Failure receive install packet.\r\n");

			break;
		}

		wchar_t installFilePath[MAX_PATH];

		if (recvBytes(clientSocket, (char*)&installFilePath, sizeof(wchar_t) * installPacket.installFilePathLength) == SOCKET_ERROR)
		{
			printf("Error: Failure receive install file path.\r\n");

			break;
		}

		installFilePath[installPacket.installFilePathLength] = '\0';

		wchar_t installFileFullPath[MAX_PATH];

		if (PathCombineW(installFileFullPath, workingDirectory, installFilePath) == NULL)
		{
			printf("Error: Failure path combine.\r\n");

			break;
		}
		
		// Install with installFileFullPath and preparePacket.installationMode
		// and

		ResponsePacket installResponsePacket = {
			.responseState = RESPONSE_STATE_SUCCESS
		};

		if (sendBytes(clientSocket, (char*)&installResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
		{
			printf("Error: Failure install response packet.\r\n");
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
	if (sizeof(wchar_t) != 2)
	{
		printf("Error: Not supported in system that wide character is not 2 bytes.\r\n");

		return -1;
	}

	if (argc != 4)
	{
		printf("Usage: wrdi-server.exe <Host> <Port> <Working Directory>\r\n");

		return -1;
	}

	wchar_t* host = argv[1];
	IN_ADDR hostAddress;

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
	removeLastPathSeparator(workingDirectory, MAX_PATH);
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