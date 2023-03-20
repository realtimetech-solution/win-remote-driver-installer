#define _WINSOCKAPI_

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <string.h>
#include <memory.h>

#include <windows.h>
#include <Ws2tcpip.h>
#include <shlwapi.h>

#pragma comment (lib, "ws2_32.lib")
#pragma comment (lib, "shlwapi.lib")

bool findAllFiles(wchar_t* directory, wchar_t** files, int* count)
{
	wchar_t pattern[MAX_PATH];

	wcscpy_s(pattern, directory);
	wcscat_s(pattern, L"\\*");

	WIN32_FIND_DATA findData;

	HANDLE findHandle = FindFirstFile(pattern, &findData);

	if (findHandle == INVALID_HANDLE_VALUE)
	{
		printf("Error: Invalid file find handle.\r\n");

		return false;
	}

	*count = 0;

	do
	{
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0)
			{
				wchar_t subDirectory[MAX_PATH];

				wcscpy_s(subDirectory, directory);
				wcscat_s(subDirectory, L"\\");
				wcscat_s(subDirectory, findData.cFileName);

				int returnedCount = 0;
				
				findAllFiles(subDirectory, files + *count, &returnedCount);

				*count += returnedCount;
			}
		}
		else
		{
			wchar_t* copyBuffer = (wchar_t*)malloc(sizeof(wchar_t) * MAX_PATH);

			if (copyBuffer == nullptr)
			{
				FindClose(findHandle);

				return false;
			}

			wcscpy_s(copyBuffer, MAX_PATH, directory);
			wcscat_s(copyBuffer, MAX_PATH, findData.cFileName);

			files[*count] = copyBuffer;

			*count = *count + 1;
		}
	} while (FindNextFile(findHandle, &findData) != 0);

	FindClose(findHandle);

	return true;
}

int recvBytes(SOCKET socket, char* buffer, int length)
{
	int offset = 0;

	while (offset < length)
	{
		int ret = recv(socket, buffer + offset, length - offset, 0);

		if (ret <= 0)
		{
			return SOCKET_ERROR;
		}

		offset += ret;
	}

	return length;
}

int wmain(int argc, wchar_t** argv)
{
	if (argc != 5)
	{
		printf("Usage: client.exe <Server> <Port> <Upload Directory or File> <Install File>\r\n");
		return -1;
	}

	wchar_t* server = argv[1];
	IN_ADDR serverAddress;

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
	size_t uploadTargetFullPathLength = GetFullPathNameW(uploadTarget, MAX_PATH, uploadTargetFullPath, nullptr);

	if (uploadTargetFullPathLength < 0 || uploadTargetFullPathLength >= MAX_PATH)
	{
		printf("Error: Failure get full path of upload target path.\r\n");

		return -1;
	}

	wchar_t installFileFullPath[MAX_PATH];
	size_t installFileFullPathLength = GetFullPathNameW(installFile, MAX_PATH, installFileFullPath, nullptr);

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
		printf("Error: WSAStartup failed.\r\n");

		return 1;
	}

	SOCKET clientSocket = socket(PF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN socketAddress;

	memcpy(&socketAddress.sin_addr, &serverAddress, sizeof(IN_ADDR));
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons(port);

	if (connect(clientSocket, (SOCKADDR*)&socketAddress, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		printf("Error: Socket connect failed.\r\n");

		return 1;
	}

	printf("Info: Connected!\r\n");
	printf("Info: Upload target..\r\n");

	if (uploadTargetAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		wchar_t** chars = (wchar_t**) malloc(sizeof(wchar_t*) * 1000);
		int count = 0;

		findAllFiles(uploadTargetFullPath, chars, &count);

		for (int i = 0; i < count; i++)
		{
			printf("%d. %S\r\n", i, chars[i]);
		}
	}
	else
	{
	}

	if (closesocket(clientSocket) == SOCKET_ERROR)
	{
		printf("Error: Socket close failed.\r\n");
	}

	if (WSACleanup() != 0)
	{
		printf("Error: WSACleanup failed.\r\n");
	}

	return 0;
}