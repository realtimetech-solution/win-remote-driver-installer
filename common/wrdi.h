#ifndef WRDI_H
#define WRDI_H

#define _WINSOCKAPI_

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>

#include <string.h>
#include <memory.h>

#include <windows.h>
#include <Ws2tcpip.h>
#include <shlwapi.h>

#pragma comment (lib, "ws2_32.lib")
#pragma comment (lib, "shlwapi.lib")

#define MAX_FILE_ENTRY_COUNT	1024
#define MAX_FILE_SIZE			(long long) (2 ^ 26)

bool findAllFiles(wchar_t* directory, wchar_t** files, int* count)
{
	wchar_t pattern[MAX_PATH];

	wcscpy_s(pattern, MAX_PATH, directory);
	wcscat_s(pattern, MAX_PATH, L"\\*");

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

				wcscpy_s(subDirectory, MAX_PATH, directory);
				wcscat_s(subDirectory, MAX_PATH, L"\\");
				wcscat_s(subDirectory, MAX_PATH, findData.cFileName);

				int returnedCount = 0;

				findAllFiles(subDirectory, files + *count, &returnedCount);

				*count += returnedCount;
			}
		}
		else
		{
			wchar_t* copyBuffer = (wchar_t*)malloc(sizeof(wchar_t) * MAX_PATH);

			if (copyBuffer == NULL)
			{
				printf("Error: Failure allocate file path buffer.\r\n");
				FindClose(findHandle);

				return false;
			}

			wcscpy_s(copyBuffer, MAX_PATH, directory);

			// Appending path separator
			wchar_t lastCharacter = copyBuffer[wcsnlen_s(copyBuffer, MAX_PATH) - 1];

			if (lastCharacter != L'/' && lastCharacter != L'\\')
			{
				wcscat_s(copyBuffer, MAX_PATH, L"\\\0");
			}

			wcscat_s(copyBuffer, MAX_PATH, findData.cFileName);

			files[*count] = copyBuffer;

			*count = *count + 1;
		}
	} while (FindNextFile(findHandle, &findData) != 0);

	FindClose(findHandle);

	return true;
}

bool createDirectories(wchar_t* path)
{
	wchar_t* buffer = malloc(sizeof(wchar_t) * MAX_PATH);

	if (buffer == NULL)
	{
		return false;
	}

	buffer[0] = '\0';

	wchar_t* remainPath = NULL;
	wchar_t* splitedPath = wcstok_s(path, L"\\/", &remainPath);

	while (splitedPath != NULL)
	{
		size_t directoryNameLength = wcsnlen_s(splitedPath, MAX_PATH);

		if (directoryNameLength < 1)
		{
			free(buffer);

			return false;
		}

		wcscat_s(buffer, MAX_PATH, splitedPath);
		wcscat_s(buffer, MAX_PATH, L"\\");

		if (!CreateDirectoryW(buffer, NULL))
		{
			if (GetLastError() == ERROR_PATH_NOT_FOUND)
			{
				free(buffer);

				return false;
			}
		}

		splitedPath = wcstok_s(NULL, L"\\/", &remainPath);
	}

	free(buffer);

	return true;
}

int recvBytes(SOCKET socket, char* buffer, int length)
{
	int offset = 0;

	while (offset < length)
	{
		int returnValue = recv(socket, buffer + offset, length - offset, 0);

		if (returnValue <= 0)
		{
			return SOCKET_ERROR;
		}

		offset += returnValue;
	}

	return length;
}

#endif