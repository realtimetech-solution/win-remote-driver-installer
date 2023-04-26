#include <wrdi.h>
#include "ini_reader.h"

char fileBuffer[MAX_FILE_SIZE];

INIContext  iniContext;
wchar_t     iniValueBuffer[MAX_INI_STRING_LENGTH];

int wmain(int argc, wchar_t** argv)
{
	if (sizeof(wchar_t) != 2)
	{
		printf("Error: Not supported in system that wide character is not 2 bytes.\r\n");

		return EXIT_FAILURE;
	}

	if (argc != 2)
	{
		printf("Usage: wrdi-client.exe <Configuration INI File>\r\n");

		return EXIT_FAILURE;
	}

	if (!ReadINIFormFile(&iniContext, argv[1]))
	{
		printf("Error: Failed to read ini.\r\n");

		return EXIT_FAILURE;
	}

	IN_ADDR serverAddress;
	int serverPortNumber;

	wchar_t driverName[MAX_DRIVER_NAME_LENGTH];
	size_t driverNameLength;

	wchar_t uploadDirectoryPath[MAX_PATH];
	size_t uploadDirectoryPathLength;

	wchar_t installationFilePath[MAX_PATH];
	char installationMode = '\0';

	wchar_t executionFilePath[MAX_PATH];
	bool hasExecutionFilePath = false;

	if (!GetINIFieldValue(&iniContext, L"Server.Address", iniValueBuffer, MAX_INI_STRING_LENGTH))
	{
		printf("Error: Failed to get server address from ini.\r\n");

		return EXIT_FAILURE;
	}
	else
	{
		if (InetPtonW(AF_INET, iniValueBuffer, &serverAddress) <= 0)
		{
			printf("Error: Given server address is not valid.\r\n");

			return EXIT_FAILURE;
		}
	}


	if (!GetINIFieldValue(&iniContext, L"Server.Port", iniValueBuffer, MAX_INI_STRING_LENGTH))
	{
		printf("Error: Failed to get server port from ini.\r\n");

		return EXIT_FAILURE;
	}
	else
	{
		serverPortNumber = wcstol(iniValueBuffer, NULL, 10);;

		if (serverPortNumber == 0)
		{
			if (wcslen(iniValueBuffer) != 1 || iniValueBuffer[0] != '0')
			{
				printf("Error: Given server port number is not integer.\r\n");

				return EXIT_FAILURE;
			}
		}

		if (serverPortNumber < 0 || serverPortNumber > MAX_PORT_NUMBER)
		{
			printf("Error: Invalid server port number.\r\n");

			return EXIT_FAILURE;
		}
	}


	if (!GetINIFieldValue(&iniContext, L"Driver.Name", iniValueBuffer, MAX_INI_STRING_LENGTH))
	{
		printf("Error: Failed to get driver name from ini.\r\n");

		return EXIT_FAILURE;
	}
	else
	{
		driverNameLength = wcslen(iniValueBuffer);

		if (driverNameLength >= MAX_DRIVER_NAME_LENGTH)
		{
			printf("Error: Given driver name is too long.\r\n");

			return EXIT_FAILURE;
		}

		wcscpy_s(driverName, MAX_DRIVER_NAME_LENGTH, iniValueBuffer);
	}


	if (!GetINIFieldValue(&iniContext, L"Upload.DirectoryPath", iniValueBuffer, MAX_INI_STRING_LENGTH))
	{
		printf("Error: Failed to get upload directory path from ini.\r\n");

		return EXIT_FAILURE;
	}
	else
	{
		if (!GetResolvedFullPath(iniValueBuffer, uploadDirectoryPath, MAX_PATH))
		{
			printf("Error: Not valid upload directory path.\r\n");

			return EXIT_FAILURE;
		}

		RemoveLastPathSeparator(uploadDirectoryPath);

		DWORD attributes = GetFileAttributesW(uploadDirectoryPath);

		if (attributes == INVALID_FILE_ATTRIBUTES)
		{
			printf("Error: Not valid upload directory path.\r\n");

			return EXIT_FAILURE;
		}

		if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			printf("Error: Given upload directory path is not directory.\r\n");

			return EXIT_FAILURE;
		}

		uploadDirectoryPathLength = wcslen(uploadDirectoryPath);

		if (uploadDirectoryPathLength >= MAX_PATH)
		{
			printf("Error: Given upload directory path is too long.\r\n");

			return EXIT_FAILURE;
		}
	}


	if (!GetINIFieldValue(&iniContext, L"Installation.FilePath", iniValueBuffer, MAX_INI_STRING_LENGTH))
	{
		printf("Error: Failed to get installation file path from ini.\r\n");

		return EXIT_FAILURE;
	}
	else
	{
		if (!GetResolvedFullPath(iniValueBuffer, installationFilePath, MAX_PATH))
		{
			printf("Error: Not valid installation file path.\r\n");

			return EXIT_FAILURE;
		}

		RemoveLastPathSeparator(installationFilePath);

		DWORD attributes = GetFileAttributesW(installationFilePath);

		if (attributes == INVALID_FILE_ATTRIBUTES)
		{
			printf("Error: Not valid installation file path.\r\n");

			return EXIT_FAILURE;
		}

		if (attributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			printf("Error: Given installation file path is directory.\r\n");

			return EXIT_FAILURE;
		}

		if (!ContainsFileInPath(installationFilePath, uploadDirectoryPath))
		{
			printf("Error: Install file is not contained in upload target path.\r\n");

			return EXIT_FAILURE;
		}

		wchar_t* installationFileExtension = PathFindExtensionW(installationFilePath);

		if (installationFileExtension == NULL)
		{
			printf("Error: Install file doesn't have extension.\r\n");

			return EXIT_FAILURE;
		}

		if (_wcsicmp(installationFileExtension, L".inf") == 0)
		{
			installationMode = INSTALLATION_MODE_INF;
		}
		else if (_wcsicmp(installationFileExtension, L".sys") == 0)
		{
			installationMode = INSTALLATION_MODE_SYS;
		}
		else if (_wcsicmp(installationFileExtension, L".txt") == 0)
		{
			installationMode = INSTALLATION_MODE_DEBUG;
		}
		else
		{
			printf("Error: Unknown install file extension.\r\n");

			return EXIT_FAILURE;
		}
	}


	if (GetINIFieldValue(&iniContext, L"Execution.FilePath", iniValueBuffer, MAX_INI_STRING_LENGTH))
	{
		if (!GetResolvedFullPath(iniValueBuffer, executionFilePath, MAX_PATH))
		{
			printf("Error: Not valid execution file path.\r\n");

			return EXIT_FAILURE;
		}

		RemoveLastPathSeparator(executionFilePath);

		DWORD attributes = GetFileAttributesW(executionFilePath);

		if (attributes == INVALID_FILE_ATTRIBUTES)
		{
			printf("Error: Not valid execution file path.\r\n");

			return EXIT_FAILURE;
		}

		if (attributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			printf("Error: Given execution file path is directory.\r\n");

			return EXIT_FAILURE;
		}

		DWORD executableType;

		if (GetBinaryTypeW(executionFilePath, &executableType) == 0)
		{
			printf("Error: Given example executable is not executable.\r\n");

			return EXIT_FAILURE;
		}

		if (!ContainsFileInPath(executionFilePath, uploadDirectoryPath))
		{
			printf("Error: Execution file is not contained in upload target path.\r\n");

			return EXIT_FAILURE;
		}

		hasExecutionFilePath = true;
	}

	// Connect to server
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		printf("Error: Failure WSAStartup.\r\n");

		return EXIT_FAILURE;
	}

	SOCKET clientSocket = socket(PF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN socketAddress = { 0 };

	memcpy(&socketAddress.sin_addr, &serverAddress, sizeof(IN_ADDR));
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons(serverPortNumber);

	if (connect(clientSocket, (SOCKADDR*)&socketAddress, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		printf("Error: Failure socket connect.\r\n");

		return EXIT_FAILURE;
	}

	printf("Info: Connected!\r\n");

	printf("Info: Installation mode is %S\r\n", GetInstallationModeString(installationMode));

	printf("Info: Prepare upload targets..\r\n");
	wchar_t* directoryName = PathFindFileNameW(uploadDirectoryPath);

	wchar_t* uploadFiles[MAX_FILE_ENTRY_COUNT];
	size_t uploadFileCount = 0;

	if (!GetFilesInDirectory(uploadDirectoryPath, uploadFiles, &uploadFileCount))
	{
		printf("Error: Failure find to files in upload target directory.\r\n");

		return EXIT_FAILURE;
	}

	printf("Info: Send prepare packet.\r\n");

	PreparePacket preparePacket = {
		.driverNameLength = (uint32_t)driverNameLength,
		.fileCount = (uint32_t)uploadFileCount,
		.installationMode = installationMode,
		.hasExecutable = hasExecutionFilePath
	};

	if (SendBytesToSocket(clientSocket, (char*)&preparePacket, sizeof(PreparePacket)) == SOCKET_ERROR)
	{
		printf("Error: Failure send prepare packet.\r\n");

		return EXIT_FAILURE;
	}

	if (SendBytesToSocket(clientSocket, (char*)driverName, (int)(sizeof(wchar_t) * driverNameLength)) == SOCKET_ERROR)
	{
		printf("Error: Failure send driver name.\r\n");

		return EXIT_FAILURE;
	}

	ResponsePacket prepareResponsePacket;

	if (ReceiveBytesFromSocket(clientSocket, (char*)&prepareResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
	{
		printf("Error: Failure receive prepare response packet.\r\n");

		return EXIT_FAILURE;
	}

	if (prepareResponsePacket.responseState != RESPONSE_STATE_SUCCESS)
	{
		printf("Error: Failure prepare becuase %S.\r\n", GetResponseStateString(prepareResponsePacket.responseState));

		return EXIT_FAILURE;
	}

	printf("Info: Uploading %zu files..\r\n", uploadFileCount);

	for (int i = 0; i < uploadFileCount; i++)
	{
		wchar_t* filePath = uploadFiles[i];

		if (filePath != NULL)
		{
			wchar_t uploadFilePath[MAX_PATH];
			size_t uploadFilePathLength;

			wcscpy_s(uploadFilePath, MAX_PATH, directoryName);
			wcscat_s(uploadFilePath, MAX_PATH, L"\\\0");
			wcscat_s(uploadFilePath, MAX_PATH, filePath + uploadDirectoryPathLength + 1);

			uploadFilePathLength = wcsnlen_s(uploadFilePath, MAX_PATH);

			printf("Info: Uploading '%S' file..\r\n", uploadFilePath);

			HANDLE fileHandle = CreateFileW(filePath,
											GENERIC_READ,
											FILE_SHARE_READ,
											NULL,
											OPEN_EXISTING,
											FILE_ATTRIBUTE_NORMAL,
											NULL);

			if (fileHandle == INVALID_HANDLE_VALUE)
			{
				printf("Error: Failure open file read handle.\r\n");

				return EXIT_FAILURE;
			}

			LARGE_INTEGER fileSizeLarge;

			if (!GetFileSizeEx(fileHandle, &fileSizeLarge))
			{
				printf("Error: Failure get file size.\r\n");

				return EXIT_FAILURE;
			}

			if (fileSizeLarge.QuadPart > MAX_FILE_SIZE)
			{
				printf("Error: File size is too big.\r\n");

				return EXIT_FAILURE;
			}

			int fileSize = (int)fileSizeLarge.QuadPart;

			DWORD readBytes = 0;

			if (!ReadFile(fileHandle, fileBuffer, fileSize, &readBytes, NULL))
			{
				printf("Error: Failure read file.\r\n");

				return EXIT_FAILURE;
			}

			if (readBytes != fileSize)
			{
				printf("Error: Failure read all data of file.\r\n");

				return EXIT_FAILURE;
			}

			if (!CloseHandle(fileHandle))
			{
				printf("Error: Failure close file handle.\r\n");

				return EXIT_FAILURE;
			}

			UploadHeaderPacket uploadHeaderPacket = {
				.filePathLength = (uint32_t)uploadFilePathLength,
				.fileSize = fileSize
			};

			if (SendBytesToSocket(clientSocket, (char*)&uploadHeaderPacket, sizeof(UploadHeaderPacket)) == SOCKET_ERROR)
			{
				printf("Error: Failure send upload header packet.\r\n");

				return EXIT_FAILURE;
			}

			if (SendBytesToSocket(clientSocket, (char*)&uploadFilePath, sizeof(wchar_t) * (int)uploadFilePathLength) == SOCKET_ERROR)
			{
				printf("Error: Failure send upload file path.\r\n");

				return EXIT_FAILURE;
			}

			if (SendBytesToSocket(clientSocket, (char*)&fileBuffer, fileSize) == SOCKET_ERROR)
			{
				printf("Error: Failure send upload file data.\r\n");

				return EXIT_FAILURE;
			}

			ResponsePacket uploadResponsePacket;

			if (ReceiveBytesFromSocket(clientSocket, (char*)&uploadResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
			{
				printf("Error: Failure receive upload response packet.\r\n");

				return EXIT_FAILURE;
			}

			if (uploadResponsePacket.responseState != RESPONSE_STATE_SUCCESS)
			{
				printf("Error: Failure file upload becuase %S.\r\n", GetResponseStateString(uploadResponsePacket.responseState));

				return EXIT_FAILURE;
			}

			printf("Info: Uploaded!\r\n");
		}
	}

	printf("Info: Press any key to Install..\r\n");
	getch();

	wchar_t installFilePath[MAX_PATH];
	size_t installFilePathLength;

	wcscpy_s(installFilePath, MAX_PATH, directoryName);
	wcscat_s(installFilePath, MAX_PATH, L"\\\0");
	wcscat_s(installFilePath, MAX_PATH, installationFilePath + uploadDirectoryPathLength + 1);

	installFilePathLength = wcsnlen_s(installFilePath, MAX_PATH);

	printf("Info: Installing '%S' file.. (If it's stuck for a long time, check the server side.)\r\n", installFilePath);

	InstallPacket installPacket = {
		.installFilePathLength = (uint32_t)installFilePathLength
	};

	if (SendBytesToSocket(clientSocket, (char*)&installPacket, sizeof(InstallPacket)) == SOCKET_ERROR)
	{
		printf("Error: Failure send install packet.\r\n");

		return EXIT_FAILURE;
	}

	if (SendBytesToSocket(clientSocket, (char*)&installFilePath, sizeof(wchar_t) * (int)installFilePathLength) == SOCKET_ERROR)
	{
		printf("Error: Failure send install file path.\r\n");

		return EXIT_FAILURE;
	}

	ResponsePacket installResponsePacket;

	if (ReceiveBytesFromSocket(clientSocket, (char*)&installResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
	{
		printf("Error: Failure receive install response packet.\r\n");

		return EXIT_FAILURE;
	}

	if (installResponsePacket.responseState != RESPONSE_STATE_SUCCESS)
	{
		printf("Error: Failure install driver becuase %S.\r\n", GetResponseStateString(installResponsePacket.responseState));

		return EXIT_FAILURE;
	}

	printf("Info: Installed!\r\n");

	if (hasExecutionFilePath)
	{
		printf("Info: Press any key to Execute..\r\n");
		getch();

		wchar_t executeFilePath[MAX_PATH];
		size_t executeFilePathLength;

		wcscpy_s(executeFilePath, MAX_PATH, directoryName);
		wcscat_s(executeFilePath, MAX_PATH, L"\\\0");
		wcscat_s(executeFilePath, MAX_PATH, executionFilePath + uploadDirectoryPathLength + 1);

		executeFilePathLength = wcsnlen_s(executeFilePath, MAX_PATH);

		ExecutePacket executePacket = {
			.executeFilePathLength = (uint32_t)executeFilePathLength
		};

		if (SendBytesToSocket(clientSocket, (char*)&executePacket, sizeof(executePacket)) == SOCKET_ERROR)
		{
			printf("Error: Failure send execute packet.\r\n");

			return EXIT_FAILURE;
		}

		if (SendBytesToSocket(clientSocket, (char*)&executeFilePath, (int)(sizeof(wchar_t) * executeFilePathLength)) == SOCKET_ERROR)
		{
			printf("Error: Failure send execute file path.\r\n");

			return EXIT_FAILURE;
		}

		ResponsePacket executeResponsePacket;

		if (ReceiveBytesFromSocket(clientSocket, (char*)&executeResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
		{
			printf("Error: Failure receive execute response packet.\r\n");

			return EXIT_FAILURE;
		}

		if (executeResponsePacket.responseState != RESPONSE_STATE_SUCCESS)
		{
			printf("Error: Failure execute becuase %S.\r\n", GetResponseStateString(executeResponsePacket.responseState));

			return EXIT_FAILURE;
		}

		printf("Info: Executed!\r\n");
	}

	if (closesocket(clientSocket) == SOCKET_ERROR)
	{
		printf("Error: Failure socket close.\r\n");
	}

	if (WSACleanup() != 0)
	{
		printf("Error: Failure WSACleanup.\r\n");
	}

	return EXIT_SUCCESS;
}