#include <wrdi.h>

#include <sys_driver.h>
#include <inf_driver.h>
#include <ini_reader.h>

char fileBuffer[MAX_FILE_SIZE];

int runService(IN_ADDR* hostAddress, int port, wchar_t* workingDirectory)
{
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("Error: Failure at WSAStartup.\r\n");

        return 1;
    }

    SOCKET serverSocket = socket(PF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN socketAddress = { 0 };

    memcpy(&socketAddress.sin_addr, hostAddress, sizeof(IN_ADDR));
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_port = htons(port);

    if (bind(serverSocket, (SOCKADDR*)&socketAddress, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
    {
        printf("Error: Failure in socket binding.\r\n");

        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        printf("Error: Failure in socket listening.\r\n");

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
            printf("Error: Failure in socket accepting.\r\n");

            return 1;
        }

        inet_ntop(AF_INET, &(clientAddress.sin_addr), clientAddressString, INET_ADDRSTRLEN);
        printf("Info: Connected Client %s:%d..\r\n", clientAddressString, clientAddress.sin_port);

        ResponsePacket prepareResponsePacket = {
            .responseState = RESPONSE_STATE_ERROR
        };

        PreparePacket preparePacket;

        if (recvBytes(clientSocket, (char*)&preparePacket, sizeof(PreparePacket)) == SOCKET_ERROR)
        {
            printf("Error: Failure in receiving prepare packet.\r\n");
            prepareResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

            goto RESPONSE_PREPARE;
        }

        if (preparePacket.driverNameLength >= MAX_DRIVER_NAME_LENGTH)
        {
            printf("Error: Received driver name length is too long.\r\n");
            prepareResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

            goto RESPONSE_PREPARE;
        }

        wchar_t driverName[MAX_DRIVER_NAME_LENGTH];

        if (recvBytes(clientSocket, (char*)&driverName, sizeof(wchar_t) * preparePacket.driverNameLength) == SOCKET_ERROR)
        {
            printf("Error: Failure in receiving driver name.\r\n");
            prepareResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

            goto RESPONSE_PREPARE;
        }

        driverName[preparePacket.driverNameLength] = '\0';

        printf("Info: Driver Name is %S\r\n", driverName);
        printf("Info: Installation Mode is %S\r\n", getInstallationModeString(preparePacket.installationMode));

        switch (preparePacket.installationMode)
        {
        case INSTALLATION_MODE_DEBUG:
            printf("Info: Debug Mode..\r\n");
            prepareResponsePacket.responseState = RESPONSE_STATE_SUCCESS;

            break;
        case INSTALLATION_MODE_SYS:
            printf("Info: Cleaning existing sys driver..\r\n");

            if (!cleanDriverSys(driverName))
            {
                printf("Error: Failed to clean sys driver.\r\n");
                prepareResponsePacket.responseState = RESPONSE_STATE_ERROR_DRIVER_CLEAN;

                break;
            }

            printf("Info: Cleaned!\r\n");

            prepareResponsePacket.responseState = RESPONSE_STATE_SUCCESS;

            break;
        case INSTALLATION_MODE_INF:
            printf("Info: Cleaning existing inf driver..\r\n");

            if (!cleanDriverInf())
            {
                printf("Error: Failed to clean inf driver.\r\n");
                prepareResponsePacket.responseState = RESPONSE_STATE_ERROR_DRIVER_CLEAN;

                break;
            }

            printf("Info: Cleaned!\r\n");

            prepareResponsePacket.responseState = RESPONSE_STATE_SUCCESS;

            break;
        default:
            printf("Error: Received unknown installation mode.\r\n");
            prepareResponsePacket.responseState = RESPONSE_STATE_ERROR_NOT_IMPLEMENTED;

            break;
        }

    RESPONSE_PREPARE:
        if (sendBytes(clientSocket, (char*)&prepareResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
        {
            printf("Error: Failure in preparing response packet.\r\n");
        }

        if (prepareResponsePacket.responseState != RESPONSE_STATE_SUCCESS)
        {
            continue;
        }

        printf("Info: Start receving %d files..\r\n", preparePacket.fileCount);

        for (uint32_t fileIndex = 0; fileIndex < preparePacket.fileCount; fileIndex++)
        {
            ResponsePacket uploadResponsePacket = {
                .responseState = RESPONSE_STATE_ERROR
            };

            UploadHeaderPacket uploadHeaderPacket;

            if (recvBytes(clientSocket, (char*)&uploadHeaderPacket, sizeof(UploadHeaderPacket)) == SOCKET_ERROR)
            {
                printf("Error: Failure in receiving upload header packet.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

                goto RESPONSE_UPLOAD;
            }

            if (uploadHeaderPacket.filePathLength >= MAX_PATH)
            {
                printf("Error: Received file path length is too big.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_UPLOAD;
            }

            wchar_t filePath[MAX_PATH];

            if (recvBytes(clientSocket, (char*)&filePath, sizeof(wchar_t) * uploadHeaderPacket.filePathLength) == SOCKET_ERROR)
            {
                printf("Error: Failure in receiving file path.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

                goto RESPONSE_UPLOAD;
            }

            filePath[uploadHeaderPacket.filePathLength] = '\0';

            printf("Info: Receiving '%S' file..\r\n", filePath);

            if (uploadHeaderPacket.fileSize == 0 || uploadHeaderPacket.fileSize > MAX_FILE_SIZE)
            {
                printf("Error: Invalid file size.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_UPLOAD;
            }

            if (recvBytes(clientSocket, fileBuffer, uploadHeaderPacket.fileSize) == SOCKET_ERROR)
            {
                printf("Error: Failure in receiving file data.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

                goto RESPONSE_UPLOAD;
            }

            wchar_t fileWithWorkingDirectoryPath[MAX_PATH];

            if (PathCombineW(fileWithWorkingDirectoryPath, workingDirectory, filePath) == NULL)
            {
                printf("Error: Failure in path combining.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_UPLOAD;
            }

            wchar_t directoryPath[MAX_PATH];
            wcscpy_s(directoryPath, MAX_PATH, fileWithWorkingDirectoryPath);

            if (!PathRemoveFileSpecW(directoryPath))
            {
                printf("Error: Failure in removing file name in full path.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_UPLOAD;
            }

            if (!createDirectories(directoryPath))
            {
                printf("Error: Failure in creating sub directories.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_UPLOAD;
            }

            DWORD uploadFileAttributes = GetFileAttributesW(fileWithWorkingDirectoryPath);

            if (uploadFileAttributes != INVALID_FILE_ATTRIBUTES)
            {
                bool succeedDelete = true;

                if (uploadFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (removeDirectory(fileWithWorkingDirectoryPath) != 0)
                    {
                        succeedDelete = false;
                    }
                }
                else
                {
                    if (!DeleteFileW(fileWithWorkingDirectoryPath))
                    {
                        succeedDelete = false;
                    }
                }

                if (!succeedDelete)
                {
                    uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_DELETE;

                    goto RESPONSE_UPLOAD;
                }
            }

            HANDLE fileHandle = CreateFileW(fileWithWorkingDirectoryPath,
                                            GENERIC_WRITE,
                                            0,
                                            NULL,
                                            CREATE_NEW,
                                            FILE_ATTRIBUTE_NORMAL,
                                            NULL);

            if (fileHandle == INVALID_HANDLE_VALUE)
            {
                printf("Error: Failure in opening file write handle.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_UPLOAD;
            }

            DWORD writtenBytes = 0;

            if (!WriteFile(fileHandle, fileBuffer, uploadHeaderPacket.fileSize, &writtenBytes, NULL))
            {
                printf("Error: Failure in writing file.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_UPLOAD;
            }

            if (writtenBytes != uploadHeaderPacket.fileSize)
            {
                printf("Error: Failure in writing all data to file.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_UPLOAD;
            }

            if (!CloseHandle(fileHandle))
            {
                printf("Error: Failure close file handle.\r\n");
                // Ignore
            }

            uploadResponsePacket.responseState = RESPONSE_STATE_SUCCESS;

        RESPONSE_UPLOAD:
            if (sendBytes(clientSocket, (char*)&uploadResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
            {
                printf("Error: Failure upload response packet.\r\n");
            }

            if (uploadResponsePacket.responseState != RESPONSE_STATE_SUCCESS)
            {
                break;
            }

            printf("Info: Received!\r\n");
        }

        printf("Info: Waiting for install signal..\r\n");

        ResponsePacket installResponsePacket = {
            .responseState = RESPONSE_STATE_ERROR
        };

        InstallPacket installPacket;

        if (recvBytes(clientSocket, (char*)&installPacket, sizeof(InstallPacket)) == SOCKET_ERROR)
        {
            printf("Error: Failure receive install packet.\r\n");
            installResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

            goto RESPONSE_INSTALL;
        }

        wchar_t installFilePath[MAX_PATH];

        if (recvBytes(clientSocket, (char*)&installFilePath, sizeof(wchar_t) * installPacket.installFilePathLength) == SOCKET_ERROR)
        {
            printf("Error: Failure receive install file path.\r\n");
            installResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

            goto RESPONSE_INSTALL;
        }

        installFilePath[installPacket.installFilePathLength] = '\0';

        wchar_t installFileWithWorkingDirectoryPath[MAX_PATH];

        if (PathCombineW(installFileWithWorkingDirectoryPath, workingDirectory, installFilePath) == NULL)
        {
            printf("Error: Failure path combine.\r\n");
            installResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

            goto RESPONSE_INSTALL;
        }

        wchar_t installFileFullPath[MAX_PATH];
        size_t installFileFullPathLength = GetFullPathNameW(installFileWithWorkingDirectoryPath, MAX_PATH, installFileFullPath, NULL);

        if (installFileFullPathLength < 0 || installFileFullPathLength >= MAX_PATH)
        {
            printf("Error: Failure get full path of install file path.\r\n");
            installResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

            goto RESPONSE_INSTALL;
        }

        switch (preparePacket.installationMode)
        {
        case INSTALLATION_MODE_DEBUG:
            printf("Info: Debug Mode..\r\n");
            installResponsePacket.responseState = RESPONSE_STATE_SUCCESS;

            break;
        case INSTALLATION_MODE_SYS:
            if (!startDriverSys(driverName, installFileFullPath))
            {
                printf("Error: Failure start sys driver.\r\n");
                installResponsePacket.responseState = RESPONSE_STATE_ERROR_DRIVER_START;

                break;
            }

            installResponsePacket.responseState = RESPONSE_STATE_SUCCESS;

            break;
        case INSTALLATION_MODE_INF:
            if (!installInfDriver(installFileFullPath))
            {
                printf("Error: Failure start inf driver.\r\n");
                installResponsePacket.responseState = RESPONSE_STATE_ERROR_DRIVER_START;

                break;
            }

            installResponsePacket.responseState = RESPONSE_STATE_SUCCESS;

            break;
        default:
            printf("Error: Received unknown installation mode.\r\n");
            installResponsePacket.responseState = RESPONSE_STATE_ERROR_NOT_IMPLEMENTED;

            break;
        }

    RESPONSE_INSTALL:
        if (sendBytes(clientSocket, (char*)&installResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
        {
            printf("Error: Failed to resend install response packet.\r\n");
        }

        if (installResponsePacket.responseState == RESPONSE_STATE_SUCCESS)
        {
            printf("Info: Installed!\r\n");
        }

        //download and execute example binary
        if (preparePacket.hasBinary)
        {
            printf("Info: Waiting for execute signal.\r\n");

            ResponsePacket executeResponsePacket = {
                .responseState = RESPONSE_STATE_ERROR
            };

            ExecutePacket executePacket;

            if (recvBytes(clientSocket, (char*)&executePacket, sizeof(ExecutePacket)) == SOCKET_ERROR)
            {
                printf("Error: Failed to receive execute packet.\r\n");
                executeResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

                goto RESPONSE_EXECUTE;
            }

            //get example binary file name
            wchar_t executeBinaryPath[MAX_PATH];

            if (recvBytes(clientSocket, (char*)&executeBinaryPath, sizeof(wchar_t) * executePacket.executeFilePathLength) == SOCKET_ERROR)
            {
                printf("Error: Failed to receive execute file path.\r\n");
                executeResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

                goto RESPONSE_EXECUTE;
            }

            executeBinaryPath[executePacket.executeFilePathLength] = '\0';

            wchar_t executeFileWithWorkingDirectoryPath[MAX_PATH];

            if (PathCombineW(executeFileWithWorkingDirectoryPath, workingDirectory, executeBinaryPath) == NULL)
            {
                printf("Error: Failed to combine path.\r\n");
                installResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_EXECUTE;
            }

            wchar_t executeBinaryFullPath[MAX_PATH];
            size_t executeBinaryFullPathLength = GetFullPathNameW(executeFileWithWorkingDirectoryPath, MAX_PATH, executeBinaryFullPath, NULL);

            if (executeBinaryFullPathLength < 0 || executeBinaryFullPathLength >= MAX_PATH)
            {
                printf("Error: Failure get full path of execute file path.\r\n");
                installResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_EXECUTE;
            }

            //get content of execute file, if needed
            if (executePacket.needInstall)
            {
                printf("Info: Wait to receive execute file.\r\n");

                UploadHeaderPacket uploadHeaderPacket;

                if (recvBytes(clientSocket, (char*)&uploadHeaderPacket, sizeof(UploadHeaderPacket)) == SOCKET_ERROR)
                {
                    printf("Error: Failure in receiving upload header packet.\r\n");
                    executeResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

                    goto RESPONSE_EXECUTE;
                }

                if (uploadHeaderPacket.filePathLength >= MAX_PATH)
                {
                    printf("Error: Received file path length is too big.\r\n");
                    executeResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                    goto RESPONSE_EXECUTE;
                }

                wchar_t filePath[MAX_PATH];

                if (recvBytes(clientSocket, (char*)&filePath, sizeof(wchar_t) * uploadHeaderPacket.filePathLength) == SOCKET_ERROR)
                {
                    printf("Error: Failure in receiving file path.\r\n");
                    executeResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

                    goto RESPONSE_EXECUTE;
                }

                filePath[uploadHeaderPacket.filePathLength] = '\0';

                if (wcsncmp(filePath, executeBinaryPath, executePacket.executeFilePathLength) != 0 ||
                    executePacket.executeFilePathLength != uploadHeaderPacket.filePathLength)
                {
                    printf("Error: Inappropriate file received as example binary file.\r\n");
                    executeResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                    goto RESPONSE_EXECUTE;
                }

                printf("Info: Receiving '%S' file..\r\n", filePath);

                if (uploadHeaderPacket.fileSize == 0 || uploadHeaderPacket.fileSize > MAX_FILE_SIZE)
                {
                    printf("Error: Invalid file size.\r\n");
                    executeResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                    goto RESPONSE_EXECUTE;
                }

                if (recvBytes(clientSocket, fileBuffer, uploadHeaderPacket.fileSize) == SOCKET_ERROR)
                {
                    printf("Error: Failure in receiving file data.\r\n");
                    executeResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

                    goto RESPONSE_EXECUTE;
                }

                HANDLE fileHandle = CreateFileW(executeFileWithWorkingDirectoryPath,
                                                GENERIC_WRITE,
                                                0,
                                                NULL,
                                                CREATE_NEW,
                                                FILE_ATTRIBUTE_NORMAL,
                                                NULL);

                if (fileHandle == INVALID_HANDLE_VALUE)
                {
                    printf("Error: Failure in opening file write handle.\r\n");
                    executeResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                    goto RESPONSE_EXECUTE;
                }

                DWORD writtenBytes = 0;

                if (!WriteFile(fileHandle, fileBuffer, uploadHeaderPacket.fileSize, &writtenBytes, NULL))
                {
                    printf("Error: Failure in writing file.\r\n");
                    executeResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                    goto RESPONSE_EXECUTE;
                }

                if (writtenBytes != uploadHeaderPacket.fileSize)
                {
                    printf("Error: Failure in writing all data to file.\r\n");
                    executeResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                    goto RESPONSE_EXECUTE;
                }

                if (!CloseHandle(fileHandle))
                {
                    printf("Error: Failure close file handle.\r\n");
                }
            }

            //execute example file
            STARTUPINFO si = { 0 };
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi = { 0 };

            wchar_t executeFileWithWorkingDirectoryFullPath[MAX_PATH];
            size_t executeFileWithWorkingDirectoryFullPathLength = GetFullPathNameW(executeFileWithWorkingDirectoryPath, MAX_PATH, executeFileWithWorkingDirectoryFullPath, NULL);

            if (executeFileWithWorkingDirectoryFullPathLength < 0 || executeFileWithWorkingDirectoryFullPathLength >= MAX_PATH)
            {
                printf("Error: Failed to get full path of example binary file.\r\n");
                executeResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_EXECUTE;
            }

            printf("Execute file : %S\r\n", executeFileWithWorkingDirectoryFullPath);

            if (!CreateProcessW(NULL, executeFileWithWorkingDirectoryPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
            {
                printf("Error: Fail to execute example binary.\r\n");
                executeResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_EXECUTE;
            }

            WaitForSingleObject(pi.hProcess, INFINITE);
            printf("Info: Executed example executable.\r\n");

            //example binary will run background
            if (!CloseHandle(pi.hProcess))
            {
                printf("Failed to close process information handle.\r\n");
                executeResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_EXECUTE;
            }

            if (!CloseHandle(pi.hThread))
            {
                printf("Failed to close thread information handle.\r\n");
                executeResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_EXECUTE;
            }

            executeResponsePacket.responseState = RESPONSE_STATE_SUCCESS;

        RESPONSE_EXECUTE:
            if (sendBytes(clientSocket, (char*)&executeResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
            {
                printf("Error: Failed to resend execute response packet.\r\n");
            }

            if (executeResponsePacket.responseState == RESPONSE_STATE_SUCCESS)
            {
                printf("Info: Executed given example binary!\r\n");
            }
            else
            {
                printf("Error: Failed to execute given example binary!\r\n");
            }
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

        return 1;
    }

    if (argc != 2)
    {
        printf("Usage: wrdi-server.exe <Server configuration INI file>\r\n");

        return 2;
    }

    wchar_t serverAddressRaw[MAX_BUFFER_SIZE];
    wchar_t portRaw[MAX_BUFFER_SIZE];
    wchar_t workingDirectoryRaw[MAX_BUFFER_SIZE];

    ServerConfig serverConfig = { serverAddressRaw, portRaw, workingDirectoryRaw };

    wchar_t* iniFile = argv[1];
    wchar_t iniFileFullPath[MAX_PATH];
    size_t iniFileFullPathLength = GetFullPathNameW(iniFile, MAX_PATH, iniFileFullPath, NULL);

    if (iniFileFullPathLength < 0 || iniFileFullPathLength >= MAX_PATH)
    {
        printf("Error: Failed to get full path of ini file.\r\n");

        return 1;
    }

    if (!serverINIRead(iniFileFullPath, &serverConfig))
    {
        printf("Error: Failed to setup server's configuration.\r\n");

        return 1;
    }

    IN_ADDR hostAddress;
    if (InetPtonW(AF_INET, serverAddressRaw, &hostAddress) <= 0)
    {
        printf("Error: Given host is not valid.\r\n");

        return 2;
    }

    int portNumber = wcstol(portRaw, NULL, 10);

    if (portNumber == 0)
    {
        if (wcslen(portRaw) != 1 || portRaw[0] != '0')
        {
            printf("Error: Given port number is not integer.\r\n");

            return 2;
        }
    }

    if (portNumber < 0 || portNumber > MAX_PORT_NUMBER)
    {
        printf("Error: Invalid port number.\r\n");

        return false;

    }

    wchar_t* workingDirectory = removeLastPathSeparator(workingDirectoryRaw);
    DWORD attributes = GetFileAttributesW(workingDirectory);

    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        printf("Error: Not valid working directory.\r\n");

        return 2;
    }

    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        printf("Error: Given working directory is not directory.\r\n");

        return 2;
    }

    return runService(&hostAddress, portNumber, workingDirectory);
}