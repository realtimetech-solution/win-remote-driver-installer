#ifndef WRDI_H
#define WRDI_H

#define _WINSOCKAPI_

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <string.h>
#include <memory.h>

#include <windows.h>
#include <Ws2tcpip.h>
#include <shlwapi.h>

#include "sys_driver.h"

#pragma comment (lib, "ws2_32.lib")
#pragma comment (lib, "shlwapi.lib")

#pragma pack(push, 1)
typedef struct PreparePacket_t
{
    uint32_t	driverNameLength;
    uint32_t	fileCount;
    uint8_t		installationMode;
} PreparePacket;

typedef struct UploadHeaderPacket_t
{
    uint32_t    filePathLength;
    uint32_t	fileSize;
} UploadHeaderPacket;

typedef struct ResponsePacket_t
{
    uint8_t	    responseState;
} ResponsePacket;

typedef struct InstallPacket_t
{
    uint32_t    installFilePathLength;
    // More options?
} InstallPacket;
#pragma pack(pop)

#define MAX_DRIVER_NAME_LENGTH  (512)
#define MAX_FILE_ENTRY_COUNT    (1024)
#define MAX_FILE_SIZE           (67108864)

#define RESPONSE_STATE_SUCCESS                      (0x00)
#define RESPONSE_STATE_ERROR                        (0x10)
#define RESPONSE_STATE_ERROR_NETWORK                (0x11)
#define RESPONSE_STATE_ERROR_DELETE                 (0x12)
#define RESPONSE_STATE_ERROR_INTERNAL               (0x13)
#define RESPONSE_STATE_ERROR_DRIVER_CLEAN           (0x14)
#define RESPONSE_STATE_ERROR_DRIVER_START           (0x15)
#define RESPONSE_STATE_ERROR_NOT_IMPLEMENTED        (0x16)

wchar_t* removeLastPathSeparator(wchar_t* string)
{
    size_t actualLength = wcslen(string);
    wchar_t lastCharacter = string[actualLength - 1];

    if (lastCharacter == L'/' || lastCharacter == L'\\')
    {
        string[actualLength - 1] = L'\0';

        return string;
    }

    return string;
}

const wchar_t* getResponseStateString(const uint8_t value)
{
    switch (value)
    {
    case RESPONSE_STATE_SUCCESS:
        return L"RESPONSE_STATE_SUCCESS";
    case RESPONSE_STATE_ERROR:
        return L"RESPONSE_STATE_ERROR";
    case RESPONSE_STATE_ERROR_DELETE:
        return L"RESPONSE_STATE_ERROR_NETWORK";
    case RESPONSE_STATE_ERROR_NETWORK:
        return L"RESPONSE_STATE_ERROR_DELETE";
    case RESPONSE_STATE_ERROR_INTERNAL:
        return L"RESPONSE_STATE_ERROR_INTERNAL";
    case RESPONSE_STATE_ERROR_DRIVER_CLEAN:
        return L"RESPONSE_STATE_ERROR_DRIVER_CLEAN";
    case RESPONSE_STATE_ERROR_DRIVER_START:
        return L"RESPONSE_STATE_ERROR_DRIVER_START";
    case RESPONSE_STATE_ERROR_NOT_IMPLEMENTED:
        return L"RESPONSE_STATE_ERROR_NOT_IMPLEMENTED";
    }

    if (value & RESPONSE_STATE_ERROR)
    {
        return L"RESPONSE_STATE_ERROR";
    }

    return L"RESPONSE_STATE_UNKNOWN";
}

#define INSTALLATION_MODE_DEBUG	(0x00)
#define INSTALLATION_MODE_SYS	(0x01)
#define INSTALLATION_MODE_INF	(0x02)
const wchar_t* getInstallationModeString(const uint8_t value)
{
    switch (value)
    {
    case INSTALLATION_MODE_DEBUG:
        return L"INSTALLATION_MODE_DEBUG";
    case INSTALLATION_MODE_SYS:
        return L"INSTALLATION_MODE_SYS";
    case INSTALLATION_MODE_INF:
        return L"INSTALLATION_MODE_INF";
    }

    return L"INSTALLATION_UNKNOWN";
}

bool findAllFiles(const wchar_t* directory, const wchar_t** files, size_t* count)
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

                size_t returnedCount = 0;

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
    wchar_t buffer[MAX_PATH];
    buffer[0] = '\0';

    wchar_t* remainPath = NULL;
    wchar_t* splitedPath = wcstok_s(path, L"\\/", &remainPath);

    while (splitedPath != NULL)
    {
        size_t directoryNameLength = wcsnlen_s(splitedPath, MAX_PATH);

        if (directoryNameLength < 1)
        {
            return false;
        }

        wcscat_s(buffer, MAX_PATH, splitedPath);
        wcscat_s(buffer, MAX_PATH, L"\\");

        if (!CreateDirectoryW(buffer, NULL))
        {
            if (GetLastError() == ERROR_PATH_NOT_FOUND)
            {
                return false;
            }
        }

        splitedPath = wcstok_s(NULL, L"\\/", &remainPath);
    }

    return true;
}

int removeDirectory(const wchar_t* path)
{
    SHFILEOPSTRUCT operation = {
        NULL,
        FO_DELETE,
        path,
        L"",
        FOF_NOCONFIRMATION |
        FOF_NOERRORUI |
        FOF_SILENT,
        false,
        0,
        L""
    };

    return SHFileOperation(&operation);
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

int sendBytes(SOCKET socket, const char* data, int length)
{
    int offset = 0;

    while (offset < length)
    {
        int returnValue = send(socket, data + offset, length - offset, 0);

        if (returnValue <= 0)
        {
            return SOCKET_ERROR;
        }

        offset += returnValue;
    }

    return length;
}

#endif