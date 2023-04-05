#include "wrdi.h"
#include <WinBase.h>

boolean clientINIRead(wchar_t* iniFullPath, ClientConfig* clientConfig)
{
    //server ip
    if (GetPrivateProfileString(L"CONNECTION", L"server_ip", NULL, clientConfig->serverAddress, MAX_BUFFER_SIZE, iniFullPath) <= 0)
    {
        printf("Error: Failed to read server address in INI.\r\n");

        return false;
    }

    //server port
    if (GetPrivateProfileString(L"CONNECTION", L"port", NULL, clientConfig->port, MAX_BUFFER_SIZE, iniFullPath) <= 0)
    {
        printf("Error: Failed to read server port in INI.\r\n");

        return false;
    }

    //driver name
    if (GetPrivateProfileString(L"FILE", L"driver_name", NULL, clientConfig->driverName, MAX_BUFFER_SIZE, iniFullPath) <= 0)
    {
        printf("Error: Failed to read driver name in INI.\r\n");

        return false;
    }

    //upload target (file or directory)
    if (GetPrivateProfileString(L"FILE", L"upload_target", NULL, clientConfig->uploadTarget, MAX_BUFFER_SIZE, iniFullPath) <= 0)
    {
        printf("Error: Failed to read upload target's name in INI.\r\n");

        return false;
    }

    //install file
    if (GetPrivateProfileString(L"FILE", L"install_file", NULL, clientConfig->installFile, MAX_BUFFER_SIZE, iniFullPath) <= 0)
    {
        printf("Error: Failed to read install file's name in INI.\r\n");

        return false;
    }

    //example binary file; could be empty
    int hasExecutable = GetPrivateProfileStringW(L"FILE", L"example_executable", NULL, clientConfig->exampleExecutable, MAX_BUFFER_SIZE, iniFullPath);
    if (hasExecutable <= 0)
    {
        if (hasExecutable == 0)
        {
            printf("Info: No example executable file provided.\r\n");
        } 
        else
        {
            printf("Error: Failed to read example executable file's name in INI.\r\n");
            return false;
        }
    }

    return true;
}

boolean serverINIRead(wchar_t* iniFullPath, ServerConfig* serverConfig)
{
    //server ip
    if (GetPrivateProfileString(L"CONNECTION", L"server_ip", NULL, serverConfig->serverAddress, MAX_BUFFER_SIZE, iniFullPath) <= 0)
    {
        printf("Error: Failed to read server address in INI.\r\n");

        return false;
    }

    //server port
    if (GetPrivateProfileString(L"CONNECTION", L"port", NULL, serverConfig->port, MAX_BUFFER_SIZE, iniFullPath) <= 0)
    {
        printf("Error: Failed to read server port in INI.\r\n");

        return false;
    }

    //working directory name
    if (GetPrivateProfileString(L"FILE", L"working_directory", NULL, serverConfig->workingDirectory, MAX_BUFFER_SIZE, iniFullPath) <= 0)
    {
        printf("Error: Failed to read working directory's name in INI.\r\n");

        return false;
    }

    return true;
}