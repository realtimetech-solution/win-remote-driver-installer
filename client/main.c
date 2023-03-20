#include <wrdi.h>

int wmain(int argc, wchar_t** argv)
{
	if (argc != 5)
	{
		printf("Usage: wrdi-client.exe <Server> <Port> <Upload Directory or File> <Install File>\r\n");
		return -1;
	}

	wchar_t* server = argv[1];
	IN_ADDR serverAddress = { 0 };

	if (InetPtonW(AF_INET, server, &serverAddress) <= 0)
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

	wchar_t* uploadTarget = argv[3];
	DWORD uploadTargetAttributes = GetFileAttributesW(uploadTarget);

	if (uploadTargetAttributes == INVALID_FILE_ATTRIBUTES)
	{
		printf("Error: Not valid upload target directory or file.\r\n");

		return -1;
	}

	wchar_t* installFile = argv[4];
	DWORD installFileAttributes = GetFileAttributesW(installFile);

	if (installFileAttributes == INVALID_FILE_ATTRIBUTES)
	{
		printf("Error: Not valid install file.\r\n");

		return -1;
	}

	if (installFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		printf("Error: Given install file is directory.\r\n");

		return -1;
	}

	wchar_t uploadTargetFullPath[MAX_PATH];
	size_t uploadTargetFullPathLength = GetFullPathNameW(uploadTarget, MAX_PATH, uploadTargetFullPath, NULL);

	if (uploadTargetFullPathLength < 0 || uploadTargetFullPathLength >= MAX_PATH)
	{
		printf("Error: Failure get full path of upload target path.\r\n");

		return -1;
	}

	wchar_t installFileFullPath[MAX_PATH];
	size_t installFileFullPathLength = GetFullPathNameW(installFile, MAX_PATH, installFileFullPath, NULL);

	if (installFileFullPathLength < 0 || installFileFullPathLength >= MAX_PATH)
	{
		printf("Error: Failure get full path of install file.\r\n");

		return -1;
	}

	// Check install file exists in upload target
	if (uploadTargetAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		if (uploadTargetFullPathLength >= installFileFullPathLength ||
			wcsncmp(installFileFullPath, uploadTargetFullPath, uploadTargetFullPathLength) != 0 ||
			(installFileFullPath[uploadTargetFullPathLength] != '\\' && installFileFullPath[uploadTargetFullPathLength] != '/'))
		{
			printf("Error: Install file is not contains in upload target directory.\r\n");

			return -1;
		}
	}
	else
	{
		if (uploadTargetFullPathLength != installFileFullPathLength ||
			wcsncmp(installFileFullPath, uploadTargetFullPath, installFileFullPathLength) != 0)
		{
			printf("Error: Install file is not upload target file.\r\n");
		}
	}

	// Connect to server
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		printf("Error: Failure WSAStartup.\r\n");

		return 1;
	}

	SOCKET clientSocket = socket(PF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN socketAddress = { 0 };

	memcpy(&socketAddress.sin_addr, &serverAddress, sizeof(IN_ADDR));
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons(port);

	if (connect(clientSocket, (SOCKADDR*)&socketAddress, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		printf("Error: Failure socket connect.\r\n");

		return 1;
	}

	printf("Info: Connected!\r\n");

	wchar_t* installFileExtension = PathFindExtensionW(installFileFullPath);

	if (installFileExtension == NULL)
	{
		printf("Error: Install file doesn't have extension.\r\n");

		return 1;
	}

	if (_wcsicmp(installFileExtension, L".inf") == 0)
	{

	}
	else if (_wcsicmp(installFileExtension, L".sys") == 0)
	{

	}
	else if (_wcsicmp(installFileExtension, L".sys") == 0)
	{

	}

	printf("Info: Prepare upload targets..\r\n");

	char fileBuffer[MAX_FILE_SIZE] = { 0 };
	wchar_t* directoryName = PathFindFileNameW(uploadTargetFullPath);

	wchar_t* uploadFiles[MAX_FILE_ENTRY_COUNT];
	int uploadFileCount = 0;

	if (uploadTargetAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		if (!findAllFiles(uploadTargetFullPath, uploadFiles, &uploadFileCount))
		{
			printf("Error: Failure find all files in upload target directory.\r\n");

			return 1;
		}
	}
	else
	{
		wchar_t copyBuffer[MAX_PATH];

		wcscpy_s(copyBuffer, MAX_PATH, uploadTargetFullPath);
		uploadFiles[0] = copyBuffer;

		uploadFileCount = 1;
	}

	printf("Info: Uploading %d files..\r\n", uploadFileCount);
	bool succeedUpload = true;

	for (int i = 0; i < uploadFileCount; i++)
	{
		wchar_t* filePath = uploadFiles[i];

		if (filePath != NULL)
		{
			wchar_t localPath[MAX_PATH];

			wcscpy_s(localPath, MAX_PATH, directoryName);
			wcscat_s(localPath, MAX_PATH, L"\\\0");
			wcscat_s(localPath, MAX_PATH, filePath + uploadTargetFullPathLength + 1);

			printf("Info: Uploading '%S' file..\r\n", localPath);

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
				succeedUpload = false;

				break;
			}

			LARGE_INTEGER fileSize;

			if (!GetFileSizeEx(fileHandle, &fileSize))
			{
				printf("Error: Failure get file size.\r\n");
				succeedUpload = false;

				break;
			}

			if (fileSize.QuadPart > MAX_FILE_SIZE)
			{
				printf("Error: File size is too big. (Maximum %lld byte)\r\n", MAX_FILE_SIZE);
				succeedUpload = false;

				break;
			}

			DWORD readBytes = 0;

			if (!ReadFile(fileHandle, fileBuffer, fileSize.QuadPart, &readBytes, NULL))
			{
				printf("Error: Failure read file.\r\n");
				succeedUpload = false;

				break;
			}

			if (readBytes != fileSize.QuadPart)
			{
				printf("Error: Failure read all data of file.\r\n");
				succeedUpload = false;

				break;
			}

			if (!CloseHandle(fileHandle))
			{
				printf("Error: Failure close file handle.\r\n");
				succeedUpload = false;

				break;
			}
		}
	}

	if (closesocket(clientSocket) == SOCKET_ERROR)
	{
		printf("Error: Failure socket close.\r\n");
	}

	if (WSACleanup() != 0)
	{
		printf("Error: Failure WSACleanup.\r\n");
	}

	return 0;
}