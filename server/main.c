#include <wrdi.h>

#include <sys_driver.h>
#include <inf_driver.h>
#include <ini_reader.h>

char        fileBuffer[MAX_FILE_SIZE];
wchar_t		pathBuffer[MAX_PATH];

INIContext  iniContext;
wchar_t     iniValueBuffer[MAX_INI_STRING_LENGTH];

int runService(IN_ADDR* hostAddress, int hostPortNumber, wchar_t* workingDirectory)
{
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("Error: Failure at WSAStartup.\r\n");

        return EXIT_FAILURE;
    }

    SOCKET serverSocket = socket(PF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN socketAddress = { 0 };

    memcpy(&socketAddress.sin_addr, hostAddress, sizeof(IN_ADDR));
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_port = htons(hostPortNumber);

    if (bind(serverSocket, (SOCKADDR*)&socketAddress, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
    {
        printf("Error: Failure in socket binding.\r\n");

        return EXIT_FAILURE;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        printf("Error: Failure in socket listening.\r\n");

        return EXIT_FAILURE;
    }

    printf("Info: Socket Started..\r\n");

    while (true)
    {
        int clientAddressLength = sizeof(SOCKADDR_IN);
        char clientAddressString[INET_ADDRSTRLEN];
        SOCKADDR_IN clientAddress;

        printf("Info: Waiting for client..\r\n");

        SOCKET clientSocket = accept(serverSocket, (SOCKADDR*)&clientAddress, &clientAddressLength);

        if (clientSocket == SOCKET_ERROR)
        {
            printf("Error: Failure in socket accepting.\r\n");

            return EXIT_FAILURE;
        }

        inet_ntop(AF_INET, &(clientAddress.sin_addr), clientAddressString, INET_ADDRSTRLEN);
        printf("Info: Connected Client %s:%d..\r\n", clientAddressString, clientAddress.sin_port);

        ResponsePacket prepareResponsePacket = {
            .responseState = RESPONSE_STATE_ERROR
        };

        PreparePacket preparePacket;

        if (ReceiveBytesFromSocket(clientSocket, (char*)&preparePacket, sizeof(PreparePacket)) == SOCKET_ERROR)
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

        if (ReceiveBytesFromSocket(clientSocket, (char*)&driverName, sizeof(wchar_t) * preparePacket.driverNameLength) == SOCKET_ERROR)
        {
            printf("Error: Failure in receiving driver name.\r\n");
            prepareResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

            goto RESPONSE_PREPARE;
        }

        driverName[preparePacket.driverNameLength] = '\0';

        printf("Info: Driver Name is %S\r\n", driverName);
        printf("Info: Installation Mode is %S\r\n", GetInstallationModeString(preparePacket.installationMode));

        switch (preparePacket.installationMode)
        {
        case INSTALLATION_MODE_DEBUG:
            printf("Info: Debug Mode..\r\n");
            prepareResponsePacket.responseState = RESPONSE_STATE_SUCCESS;

            break;
        case INSTALLATION_MODE_SYS:
            printf("Info: Cleaning existing sys driver..\r\n");

            if (!CleanSysDriver(driverName))
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

            if (!CleanInfDriver())
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
        if (SendBytesToSocket(clientSocket, (char*)&prepareResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
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

            if (ReceiveBytesFromSocket(clientSocket, (char*)&uploadHeaderPacket, sizeof(UploadHeaderPacket)) == SOCKET_ERROR)
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

            if (ReceiveBytesFromSocket(clientSocket, (char*)&pathBuffer, sizeof(wchar_t) * uploadHeaderPacket.filePathLength) == SOCKET_ERROR)
            {
                printf("Error: Failure in receiving file path.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

                goto RESPONSE_UPLOAD;
            }

            pathBuffer[uploadHeaderPacket.filePathLength] = '\0';

            if (!GetCombinedAndResolvedFullPath(workingDirectory, pathBuffer, filePath, MAX_PATH))
            {
                printf("Error: Failure in path combining.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_UPLOAD;
            }

            printf("Info: Receiving '%S' file..\r\n", filePath);

            if (uploadHeaderPacket.fileSize == 0 || uploadHeaderPacket.fileSize > MAX_FILE_SIZE)
            {
                printf("Error: Invalid file size.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_UPLOAD;
            }

            if (ReceiveBytesFromSocket(clientSocket, fileBuffer, uploadHeaderPacket.fileSize) == SOCKET_ERROR)
            {
                printf("Error: Failure in receiving file data.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

                goto RESPONSE_UPLOAD;
            }

            wchar_t directoryPath[MAX_PATH];
            wcscpy_s(directoryPath, MAX_PATH, filePath);

            if (!PathRemoveFileSpecW(directoryPath))
            {
                printf("Error: Failure in removing file name in full path.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_UPLOAD;
            }

            if (!CreateDirectories(directoryPath))
            {
                printf("Error: Failure in creating sub directories.\r\n");
                uploadResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_UPLOAD;
            }

            DWORD fileAttributes = GetFileAttributesW(filePath);

            if (fileAttributes != INVALID_FILE_ATTRIBUTES)
            {
                bool succeedDelete = true;

                if (fileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (RemoveDirectoryForce(filePath) != 0)
                    {
                        succeedDelete = false;
                    }
                }
                else
                {
                    if (!DeleteFileW(filePath))
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

            HANDLE fileHandle = CreateFileW(filePath,
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
            }

            uploadResponsePacket.responseState = RESPONSE_STATE_SUCCESS;

        RESPONSE_UPLOAD:
            if (SendBytesToSocket(clientSocket, (char*)&uploadResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
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

        if (ReceiveBytesFromSocket(clientSocket, (char*)&installPacket, sizeof(InstallPacket)) == SOCKET_ERROR)
        {
            printf("Error: Failure receive install packet.\r\n");
            installResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

            goto RESPONSE_INSTALL;
        }

        wchar_t installFilePath[MAX_PATH];

        if (ReceiveBytesFromSocket(clientSocket, (char*)&pathBuffer, sizeof(wchar_t) * installPacket.installFilePathLength) == SOCKET_ERROR)
        {
            printf("Error: Failure receive install file path.\r\n");
            installResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

            goto RESPONSE_INSTALL;
        }

        pathBuffer[installPacket.installFilePathLength] = '\0';

        if (!GetCombinedAndResolvedFullPath(workingDirectory, pathBuffer, installFilePath, MAX_PATH))
        {
            printf("Error: Failure path combine and get full path of install file.\r\n");
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
            if (!StartSysDriver(driverName, installFilePath))
            {
                printf("Error: Failure start sys driver.\r\n");
                installResponsePacket.responseState = RESPONSE_STATE_ERROR_DRIVER_START;

                break;
            }

            installResponsePacket.responseState = RESPONSE_STATE_SUCCESS;

            break;
        case INSTALLATION_MODE_INF:
            if (!InstallInfDriver(installFilePath))
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
        if (SendBytesToSocket(clientSocket, (char*)&installResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
        {
            printf("Error: Failed to resend install response packet.\r\n");
        }

        if (installResponsePacket.responseState == RESPONSE_STATE_SUCCESS)
        {
            printf("Info: Installed!\r\n");
        }

        if (preparePacket.hasExecutable)
        {
            printf("Info: Waiting for execute signal..\r\n");

            ResponsePacket executeResponsePacket = {
                .responseState = RESPONSE_STATE_ERROR
            };

            ExecutePacket executePacket;

            if (ReceiveBytesFromSocket(clientSocket, (char*)&executePacket, sizeof(ExecutePacket)) == SOCKET_ERROR)
            {
                printf("Error: Failed to receive execute packet.\r\n");
                executeResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

                goto RESPONSE_EXECUTE;
            }

            wchar_t executeFilePath[MAX_PATH];

            if (ReceiveBytesFromSocket(clientSocket, (char*)&pathBuffer, sizeof(wchar_t) * executePacket.executeFilePathLength) == SOCKET_ERROR)
            {
                printf("Error: Failed to receive execute file path.\r\n");
                executeResponsePacket.responseState = RESPONSE_STATE_ERROR_NETWORK;

                goto RESPONSE_EXECUTE;
            }

            pathBuffer[executePacket.executeFilePathLength] = '\0';

            if (!GetCombinedAndResolvedFullPath(workingDirectory, pathBuffer, executeFilePath, MAX_PATH))
            {
                printf("Error: Failure path combine and get full path of execute file.\r\n");
                installResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_EXECUTE;
            }

            STARTUPINFO startupInfo = { 0 };
            PROCESS_INFORMATION processInfo = { 0 };

            startupInfo.cb = sizeof(startupInfo);

            printf("Info: Executing %S..\r\n", executeFilePath);

            if (!CreateProcessW(NULL, executeFilePath, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo))
            {
                printf("Error: Failure execute executable.\r\n");
                executeResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_EXECUTE;
            }

            WaitForSingleObject(processInfo.hProcess, INFINITE);

            if (!CloseHandle(processInfo.hProcess))
            {
                printf("Failed to close process information handle.\r\n");
                executeResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_EXECUTE;
            }

            if (!CloseHandle(processInfo.hThread))
            {
                printf("Failed to close thread information handle.\r\n");
                executeResponsePacket.responseState = RESPONSE_STATE_ERROR_INTERNAL;

                goto RESPONSE_EXECUTE;
            }

            executeResponsePacket.responseState = RESPONSE_STATE_SUCCESS;

        RESPONSE_EXECUTE:
            if (SendBytesToSocket(clientSocket, (char*)&executeResponsePacket, sizeof(ResponsePacket)) == SOCKET_ERROR)
            {
                printf("Error: Failed to resend execute response packet.\r\n");
            }

            if (executeResponsePacket.responseState != RESPONSE_STATE_SUCCESS)
            {
                printf("Error: Failed to execute given example executable!\r\n");
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

    printf("Info: Finished!\r\n");

    return EXIT_SUCCESS;
}

int wmain(int argc, wchar_t** argv)
{
    if (sizeof(wchar_t) != 2)
    {
        printf("Error: Not supported in system that wide character is not 2 bytes.\r\n");

        return EXIT_FAILURE;
    }

    if (argc != 2)
    {
        printf("Usage: wrdi-server.exe <Configuration INI File>\r\n");

        return EXIT_FAILURE;
    }

    if (!ReadINIFormFile(&iniContext, argv[1]))
    {
        printf("Error: Failed to read ini.\r\n");

        return EXIT_FAILURE;
    }

    IN_ADDR hostAddress;
    int hostPortNumber;
    wchar_t workingDirectory[MAX_PATH];

    if (!GetINIFieldValue(&iniContext, L"Host.Address", iniValueBuffer, MAX_INI_STRING_LENGTH))
    {
        printf("Error: Failed to get host address from ini.\r\n");

        return EXIT_FAILURE;
    }
    else
    {
        if (InetPtonW(AF_INET, iniValueBuffer, &hostAddress) <= 0)
        {
            printf("Error: Given host address is not valid.\r\n");

            return EXIT_FAILURE;
        }
    }


    if (!GetINIFieldValue(&iniContext, L"Host.Port", iniValueBuffer, MAX_INI_STRING_LENGTH))
    {
        printf("Error: Failed to get host port from ini.\r\n");

        return EXIT_FAILURE;
    }
    else
    {
        hostPortNumber = wcstol(iniValueBuffer, NULL, 10);;

        if (hostPortNumber == 0)
        {
            if (wcslen(iniValueBuffer) != 1 || iniValueBuffer[0] != '0')
            {
                printf("Error: Given host port number is not integer.\r\n");

                return EXIT_FAILURE;
            }
        }

        if (hostPortNumber < 0 || hostPortNumber > MAX_PORT_NUMBER)
        {
            printf("Error: Invalid host port number.\r\n");

            return EXIT_FAILURE;
        }
    }


    if (!GetINIFieldValue(&iniContext, L"Environment.WorkingDirectory", iniValueBuffer, MAX_INI_STRING_LENGTH))
    {
        printf("Error: Failed to get server port from ini.\r\n");

        return EXIT_FAILURE;
    }
    else
    {
        wcscpy_s(workingDirectory, MAX_PATH, iniValueBuffer);
        RemoveLastPathSeparator(workingDirectory);

        DWORD attributes = GetFileAttributesW(workingDirectory);

        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            printf("Error: Not valid working directory.\r\n");

            return EXIT_FAILURE;
        }

        if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            printf("Error: Given working directory is not directory.\r\n");

            return EXIT_FAILURE;
        }
    }

    return runService(&hostAddress, hostPortNumber, workingDirectory);
}