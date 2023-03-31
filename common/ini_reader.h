#include "wrdi.h"
#include <WinBase.h>

boolean clientINIRead(wchar_t* iniFullPath, ClientConfig* clientConfig) {
    //server ip
    if (GetPrivateProfileString(L"CONNECTION", L"server_ip", NULL, clientConfig->serverAddress, MAX_BUFFER_SIZE, iniFullPath) <= 0) {
        printf("Error: Failed to read server address in INI.\r\n");

        return false;
    }

    //server port
    if (GetPrivateProfileString(L"CONNECTION", L"port", NULL, clientConfig->port, MAX_BUFFER_SIZE, iniFullPath) <= 0) {
        printf("Error: Failed to read server port in INI.\r\n");

        return false;
    }

    //driver name
    if (GetPrivateProfileString(L"FILES", L"driver_name", NULL, clientConfig->driverName, MAX_BUFFER_SIZE, iniFullPath) <= 0) {
        printf("Error: Failed to read driver name in INI.\r\n");

        return false;
    }
    
    //upload target (file or directory)
    if (GetPrivateProfileString(L"FILES", L"upload_target", NULL, clientConfig->uploadTarget, MAX_BUFFER_SIZE, iniFullPath) <= 0) {
        printf("Error: Failed to read upload target's name in INI.\r\n");

        return false;
    }

    //install file
    wchar_t installFile[MAX_BUFFER_SIZE];
    if (GetPrivateProfileString(L"FILES", L"install_file", NULL, installFile, MAX_BUFFER_SIZE, iniFullPath) <= 0) {
        printf("Error: Failed to read install file's name in INI.\r\n");

        return false;
    }

    //example binary file
    wchar_t exampleBinary[MAX_BUFFER_SIZE];
    if (GetPrivateProfileString(L"FILES", L"example_binary", NULL, exampleBinary, MAX_BUFFER_SIZE, iniFullPath) <= 0) {
        printf("Error: Failed to read example binary file's name in INI.\r\n");

        return false;
    }
}

boolean serverINIRead(wchar_t* iniFullPath, ServerConfig* serverConfig) {
    printf("Not implemented yet.\r\n");

    return false;
}