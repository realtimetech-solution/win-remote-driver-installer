#include <wrdi.h>
#include "ini_reader.h"

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

    //pack client's configuration
    wchar_t serverAddressRaw[MAX_BUFFER_SIZE];
    wchar_t portRaw[MAX_BUFFER_SIZE];
    wchar_t driverNameRaw[MAX_BUFFER_SIZE];
    wchar_t uploadTargetRaw[MAX_BUFFER_SIZE];
    wchar_t installFileRaw[MAX_BUFFER_SIZE];
    wchar_t exampleBinaryRaw[MAX_BUFFER_SIZE];

    ClientConfig clientConfig = { serverAddressRaw, portRaw, driverNameRaw, uploadTargetRaw, installFileRaw, exampleBinaryRaw };

    wchar_t* iniFile = argv[1];
    wchar_t iniFileFullPath[MAX_PATH];
    size_t iniFileFullPathLength = GetFullPathNameW(iniFile, MAX_PATH, iniFileFullPath, NULL);

    if (iniFileFullPathLength < 0 || iniFileFullPathLength >= MAX_PATH) {
        printf("Error: Failed to get full path of ini file.\r\n");

        return 1;
    }

    if (!clientINIRead(iniFileFullPath , &clientConfig)) {
        printf("Error: Failed to setup client's configuration.\r\n");

        return 1;
    }

    //server ip check
    IN_ADDR serverAddressConverted;
    if (InetPtonW(AF_INET, serverAddressRaw, &serverAddressConverted) <= 0) {
        printf("Error: Given server address is not valid.\r\n");

        return false;
    }

    //port check
    int portNumber = wcstol(portRaw, NULL, 10);

    if (portNumber == 0) {
        if (wcslen(portRaw) != 1 || portRaw[0] != '0') {
            printf("Error: Given port number is not integer.\r\n");

            return false;
        }
    }

    if (portNumber < 0 || portNumber > MAX_PORT_NUMBER) {
        printf("Error: Invalid port number.\r\n");

        return false;
    }

    //driver name check
    int driverNameLength = (int)wcslen(driverNameRaw);

    if (driverNameLength > MAX_DRIVER_NAME_LENGTH)
    {
        printf("Error: Given driver name is too long.\r\n");

        return 2;
    }

    //upload target check
    wchar_t* uploadTarget = removeLastPathSeparator(uploadTargetRaw);
    DWORD uploadTargetAttributes = GetFileAttributesW(uploadTarget);

    if (uploadTargetAttributes == INVALID_FILE_ATTRIBUTES)
    {
        printf("Error: Not valid upload target directory or file.\r\n");

        return false;
    }

    wchar_t uploadTargetFullPath[MAX_PATH];
    size_t uploadTargetFullPathLength = GetFullPathNameW(uploadTarget, MAX_PATH, uploadTargetFullPath, NULL);

    if (uploadTargetFullPathLength < 0 || uploadTargetFullPathLength >= MAX_PATH)
    {
        printf("Error: Failure get full path of upload target path.\r\n");

        return false;
    }

    //install file check
    wchar_t* installFile = removeLastPathSeparator(installFileRaw);
    DWORD installFileAttributes = GetFileAttributesW(installFile);

    if (installFileAttributes == INVALID_FILE_ATTRIBUTES)
    {
        printf("Error: Not valid install file.\r\n");

        return false;
    }

    if (installFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        printf("Error: Given install file is directory.\r\n");

        return false;
    }

    wchar_t installFileFullPath[MAX_PATH];
    size_t installFileFullPathLength = GetFullPathNameW(installFile, MAX_PATH, installFileFullPath, NULL);

    if (installFileFullPathLength < 0 || installFileFullPathLength >= MAX_PATH)
    {
        printf("Error: Failure get full path of install file.\r\n");

        return false;
    }

    if (uploadTargetAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        if (uploadTargetFullPathLength >= installFileFullPathLength ||
            wcsncmp(installFileFullPath, uploadTargetFullPath, uploadTargetFullPathLength) != 0 ||
            (installFileFullPath[uploadTargetFullPathLength] != '\\' && installFileFullPath[uploadTargetFullPathLength] != '/'))
        {
            printf("Error: Install file is not contained in upload target directory.\r\n");

            return false;
        }
    }
    else
    {
        if (uploadTargetFullPathLength != installFileFullPathLength ||
            wcsncmp(installFileFullPath, uploadTargetFullPath, installFileFullPathLength) != 0)
        {
            printf("Error: Install file and upload target file are not the same.\r\n");

            return false;
        }
    }

    //example binary file check
    wchar_t* exampleBinary = removeLastPathSeparator(exampleBinaryRaw);
    bool hasBinary = (int)wcsnlen_s(exampleBinary, MAX_PATH) == 0 ? false : true;

    wchar_t exampleBinaryFullPath[MAX_PATH];

    if (hasBinary) {
        DWORD exampleBinaryAttributes = GetFileAttributesW(exampleBinary);

        if (installFileAttributes == INVALID_FILE_ATTRIBUTES)
        {
            printf("Error: Not valid install file.\r\n");

            return false;
        }

        if (installFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            printf("Error: Given install file is directory.\r\n");

            return false;
        }

        if (GetBinaryTypeW(exampleBinary, NULL)) {
            printf("Given example binary is not executable.");

            return false;
        }

        size_t exampleBinaryFullPathLength = GetFullPathNameW(exampleBinary, MAX_PATH, exampleBinaryFullPath, NULL);

        if (exampleBinaryFullPathLength < 0 || exampleBinaryFullPathLength >= MAX_PATH)
        {
            printf("Error: Failure get full path of example binary.\r\n");

            return false;
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

    memcpy(&socketAddress.sin_addr, &serverAddressConverted, sizeof(IN_ADDR));
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_port = htons(portNumber);

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
        .installationMode = installationMode,
        .hasBinary = hasBinary
    };

    if (sendBytes(clientSocket, (char*)&preparePacket, sizeof(PreparePacket)) == SOCKET_ERROR)
    {
        printf("Error: Failure send prepare packet.\r\n");

        return 1;
    }

    if (sendBytes(clientSocket, (char*)driverNameRaw, sizeof(wchar_t) * driverNameLength) == SOCKET_ERROR)
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

    printf("Info: Installing '%S' file.. (If it's stuck for a long time, check the server side.)\r\n", installFilePath);

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

    //execute provided example binary
    //note: example binary is optional
    // 
    //check whether example binary is included in upload target directory : if so, send only example binary file name. Else, send file content too.

    if (hasBinary) {
        bool sendBinary = true;
        if (directoryMode)
        {
            if (uploadTargetFullPathLength >= exampleBinaryFullPath ||
                wcsncmp(exampleBinaryFullPath, uploadTargetFullPath, uploadTargetFullPathLength) != 0 ||
                (exampleBinaryFullPath[uploadTargetFullPathLength] != '\\' && exampleBinaryFullPath[uploadTargetFullPathLength] != '/'))
            {
                printf("Info: Example Binary file is not contained in upload target directory.\r\n");

                sendBinary = false;
            }
        }

        //send example binary's name
        wchar_t exampleBinaryPath[MAX_PATH];
        int exampleBinaryPathLength;

        wcscpy_s(exampleBinaryPath, MAX_PATH, exampleBinaryFullPath);
        PathStripPathW(exampleBinaryPath);

        exampleBinaryPathLength = (int)wcsnlen_s(exampleBinaryPath, MAX_PATH);

        ExecutePacket executePacket = {
            .executeFilePathLength = exampleBinaryPathLength,       //TODO : Refactor namings
            .needInstall = sendBinary
        };

        if (sendBytes(clientSocket, (char*)&executePacket, sizeof(executePacket)) == SOCKET_ERROR)
        {
            printf("Error: Failure send execute packet.\r\n");

            return 1;
        }

        if (sendBytes(clientSocket, (char*)&exampleBinaryPath, sizeof(wchar_t) * (int)exampleBinaryPathLength) == SOCKET_ERROR)
        {
            printf("Error: Failure send example binary file path.\r\n");

            return 1;
        }

        //send content of example binary file, if needed
        if (sendBinary) {
            printf("Info: Uploading '%S' file..\r\n", exampleBinaryPath);

            //TODO :  refactor sending file routine as common function

            HANDLE fileHandle = CreateFileW(exampleBinaryFullPath,
                                            GENERIC_READ,
                                            FILE_SHARE_READ,
                                            NULL,
                                            OPEN_EXISTING,
                                            FILE_ATTRIBUTE_NORMAL,
                                            NULL
            );

            if (fileHandle == INVALID_HANDLE_VALUE)
            {
                printf("Error: Failed to open file read handle.\r\n");

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
                .filePathLength = exampleBinaryPathLength,
                .fileSize = fileSize
            };

            if (sendBytes(clientSocket, (char*)&uploadHeaderPacket, sizeof(UploadHeaderPacket)) == SOCKET_ERROR)
            {
                printf("Error: Failure send upload header packet.\r\n");

                return 1;
            }

            if (sendBytes(clientSocket, (char*)&exampleBinaryPath, sizeof(wchar_t) * (int)exampleBinaryPathLength) == SOCKET_ERROR)
            {
                printf("Error: Failure send upload file path.\r\n");

                return 1;
            }

            if (sendBytes(clientSocket, (char*)&fileBuffer, fileSize) == SOCKET_ERROR)
            {
                printf("Error: Failure send upload file data.\r\n");

                return 1;
            }
        }

        ResponsePacket executeResponsePacket;

        if (recvBytes(clientSocket, (char*)&executeResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
        {
            printf("Error: Failure receive execute response packet.\r\n");

            return 1;
        }

        if (executeResponsePacket.responseState != RESPONSE_STATE_SUCCESS)
        {
            printf("Error: Failure execute example binary becuase %S.\r\n", getResponseStateString(executeResponsePacket.responseState));

            return 1;
        }
        else {
            printf("Info: Example binary will be executed!\r\n");
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