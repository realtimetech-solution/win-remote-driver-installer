#include <wrdi.h>

char fileBuffer[MAX_FILE_SIZE];

int wmain(int argc, wchar_t** argv)
{
    if (sizeof(wchar_t) != 2)
    {
        printf("Error: Not supported in system that wide character is not 2 bytes.\r\n");

        return 1;
    }

    if (argc != 6)
    {
        printf("Usage: wrdi-client.exe <Server> <Port> <Driver Name> <Upload Directory or File> <Install File>\r\n");

        return 2;
    }

    wchar_t* server = argv[1];
    IN_ADDR serverAddress;

    if (InetPtonW(AF_INET, server, &serverAddress) <= 0)
    {
        printf("Error: Given host is not valid.\r\n");

        return 2;
    }

    wchar_t* portArgument = argv[2];
    int port = wcstol(portArgument, NULL, 10);

    if (port == 0)
    {
        if (wcslen(portArgument) != 1 || portArgument[0] != '0')
        {
            printf("Error: Given port number is not integer.\r\n");

            return 2;
        }
    }

    wchar_t* driverName = argv[3];
    int driverNameLength = (int)wcslen(driverName);

    if (driverNameLength > MAX_DRIVER_NAME_LENGTH)
    {
        printf("Error: Given driver name is too long.\r\n");

        return 2;
    }

    wchar_t* uploadTarget = removeLastPathSeparator(argv[4]);
    DWORD uploadTargetAttributes = GetFileAttributesW(uploadTarget);

    if (uploadTargetAttributes == INVALID_FILE_ATTRIBUTES)
    {
        printf("Error: Not valid upload target directory or file.\r\n");

        return 2;
    }

    wchar_t* installFile = removeLastPathSeparator(argv[5]);
    DWORD installFileAttributes = GetFileAttributesW(installFile);

    if (installFileAttributes == INVALID_FILE_ATTRIBUTES)
    {
        printf("Error: Not valid install file.\r\n");

        return 2;
    }

    if (installFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        printf("Error: Given install file is directory.\r\n");

        return 2;
    }

    wchar_t uploadTargetFullPath[MAX_PATH];
    size_t uploadTargetFullPathLength = GetFullPathNameW(uploadTarget, MAX_PATH, uploadTargetFullPath, NULL);

    if (uploadTargetFullPathLength < 0 || uploadTargetFullPathLength >= MAX_PATH)
    {
        printf("Error: Failure get full path of upload target path.\r\n");

        return 2;
    }

    wchar_t installFileFullPath[MAX_PATH];
    size_t installFileFullPathLength = GetFullPathNameW(installFile, MAX_PATH, installFileFullPath, NULL);

    if (installFileFullPathLength < 0 || installFileFullPathLength >= MAX_PATH)
    {
        printf("Error: Failure get full path of install file.\r\n");

        return 2;
    }

    // Check install file exists in upload target
    if (uploadTargetAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        if (uploadTargetFullPathLength >= installFileFullPathLength ||
            wcsncmp(installFileFullPath, uploadTargetFullPath, uploadTargetFullPathLength) != 0 ||
            (installFileFullPath[uploadTargetFullPathLength] != '\\' && installFileFullPath[uploadTargetFullPathLength] != '/'))
        {
            printf("Error: Install file is not contains in upload target directory.\r\n");

            return 2;
        }
    }
    else
    {
        if (uploadTargetFullPathLength != installFileFullPathLength ||
            wcsncmp(installFileFullPath, uploadTargetFullPath, installFileFullPathLength) != 0)
        {
            printf("Error: Install file is not upload target file.\r\n");

            return 2;
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

    char installationMode = '\0';

    if (_wcsicmp(installFileExtension, L".inf") == 0)
    {
        installationMode = INSTALLATION_MODE_INF;
    }
    else if (_wcsicmp(installFileExtension, L".sys") == 0)
    {
        installationMode = INSTALLATION_MODE_SYS;
    }
    else if (_wcsicmp(installFileExtension, L".txt") == 0)
    {
        installationMode = INSTALLATION_MODE_DEBUG;
    }
    else
    {
        printf("Error: Unknown install file extension.\r\n");

        return 1;
    }

    printf("Info: Installation mode is %S\r\n", getInstallationModeString(installationMode));

    printf("Info: Prepare upload targets..\r\n");

    bool directoryMode = uploadTargetAttributes & FILE_ATTRIBUTE_DIRECTORY;
    wchar_t* directoryName = PathFindFileNameW(uploadTargetFullPath);

    wchar_t* uploadFiles[MAX_FILE_ENTRY_COUNT];
    size_t uploadFileCount = 0;

    if (directoryMode)
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

    printf("Info: Send prepare packet.\r\n");

    PreparePacket preparePacket = {
        .driverNameLength = driverNameLength,
        .fileCount = (uint32_t)uploadFileCount,
        .installationMode = installationMode
    };

    if (sendBytes(clientSocket, (char*)&preparePacket, sizeof(PreparePacket)) == SOCKET_ERROR)
    {
        printf("Error: Failure send prepare packet.\r\n");

        return 1;
    }

    if (sendBytes(clientSocket, (char*)driverName, sizeof(wchar_t) * driverNameLength) == SOCKET_ERROR)
    {
        printf("Error: Failure send driver name.\r\n");

        return 1;
    }

    ResponsePacket prepareResponsePacket;

    if (recvBytes(clientSocket, (char*)&prepareResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
    {
        printf("Error: Failure receive prepare response packet.\r\n");

        return 1;
    }

    if (prepareResponsePacket.responseState != RESPONSE_STATE_SUCCESS)
    {
        printf("Error: Failure prepare becuase %S.\r\n", getResponseStateString(prepareResponsePacket.responseState));

        return 1;
    }

    printf("Info: Uploading %zu files..\r\n", uploadFileCount);

    for (int i = 0; i < uploadFileCount; i++)
    {
        wchar_t* filePath = uploadFiles[i];

        if (filePath != NULL)
        {
            wchar_t uploadFilePath[MAX_PATH];
            int uploadFilePathLength;

            if (directoryMode)
            {
                wcscpy_s(uploadFilePath, MAX_PATH, directoryName);
                wcscat_s(uploadFilePath, MAX_PATH, L"\\\0");
                wcscat_s(uploadFilePath, MAX_PATH, filePath + uploadTargetFullPathLength + 1);
            }
            else
            {
                wcscpy_s(uploadFilePath, MAX_PATH, filePath);
                PathStripPathW(uploadFilePath);
            }

            uploadFilePathLength = (int)wcsnlen_s(uploadFilePath, MAX_PATH);

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

                return 1;
            }

            LARGE_INTEGER fileSizeLarge;

            if (!GetFileSizeEx(fileHandle, &fileSizeLarge))
            {
                printf("Error: Failure get file size.\r\n");

                return 1;
            }

            if (fileSizeLarge.QuadPart > MAX_FILE_SIZE)
            {
                printf("Error: File size is too big.\r\n");

                return 1;
            }

            int fileSize = (int)fileSizeLarge.QuadPart;

            DWORD readBytes = 0;

            if (!ReadFile(fileHandle, fileBuffer, fileSize, &readBytes, NULL))
            {
                printf("Error: Failure read file.\r\n");

                return 1;
            }

            if (readBytes != fileSize)
            {
                printf("Error: Failure read all data of file.\r\n");

                return 1;
            }

            if (!CloseHandle(fileHandle))
            {
                printf("Error: Failure close file handle.\r\n");

                return 1;
            }

            UploadHeaderPacket uploadHeaderPacket = {
                .filePathLength = uploadFilePathLength,
                .fileSize = fileSize
            };

            if (sendBytes(clientSocket, (char*)&uploadHeaderPacket, sizeof(UploadHeaderPacket)) == SOCKET_ERROR)
            {
                printf("Error: Failure send upload header packet.\r\n");

                return 1;
            }

            if (sendBytes(clientSocket, (char*)&uploadFilePath, sizeof(wchar_t) * (int)uploadFilePathLength) == SOCKET_ERROR)
            {
                printf("Error: Failure send upload file path.\r\n");

                return 1;
            }

            if (sendBytes(clientSocket, (char*)&fileBuffer, fileSize) == SOCKET_ERROR)
            {
                printf("Error: Failure send upload file data.\r\n");

                return 1;
            }

            ResponsePacket uploadResponsePacket;

            if (recvBytes(clientSocket, (char*)&uploadResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
            {
                printf("Error: Failure receive upload response packet.\r\n");

                return 1;
            }

            if (uploadResponsePacket.responseState != RESPONSE_STATE_SUCCESS)
            {
                printf("Error: Failure file upload becuase %S.\r\n", getResponseStateString(uploadResponsePacket.responseState));

                return 1;
            }

            printf("Info: Uploaded!\r\n");
        }
    }

    printf("Info: Press any key to Install..\r\n");
    getch();

    wchar_t installFilePath[MAX_PATH];
    int installFilePathLength;

    if (directoryMode)
    {
        wcscpy_s(installFilePath, MAX_PATH, directoryName);
        wcscat_s(installFilePath, MAX_PATH, L"\\\0");
        wcscat_s(installFilePath, MAX_PATH, installFileFullPath + uploadTargetFullPathLength + 1);
    }
    else
    {
        wcscpy_s(installFilePath, MAX_PATH, installFileFullPath);
        PathStripPathW(installFilePath);
    }

    installFilePathLength = (int)wcsnlen_s(installFilePath, MAX_PATH);

    printf("Info: Installing '%S' file..\r\n", installFilePath);

    InstallPacket installPacket = {
        .installFilePathLength = installFilePathLength
    };

    if (sendBytes(clientSocket, (char*)&installPacket, sizeof(InstallPacket)) == SOCKET_ERROR)
    {
        printf("Error: Failure send install packet.\r\n");

        return 1;
    }

    if (sendBytes(clientSocket, (char*)&installFilePath, sizeof(wchar_t) * (int)installFilePathLength) == SOCKET_ERROR)
    {
        printf("Error: Failure send install file path.\r\n");

        return 1;
    }

    ResponsePacket installResponsePacket;

    if (recvBytes(clientSocket, (char*)&installResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
    {
        printf("Error: Failure receive install response packet.\r\n");

        return 1;
    }

    if (installResponsePacket.responseState != RESPONSE_STATE_SUCCESS)
    {
        printf("Error: Failure install driver becuase %S.\r\n", getResponseStateString(installResponsePacket.responseState));

        return 1;
    }

    printf("Info: Installed!\r\n");

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