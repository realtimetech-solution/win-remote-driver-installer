#ifndef SYS_DRIVER_H
#define SYS_DRIVER_H

#include "wrdi.h"

bool startDriverSys(const wchar_t* driverName, const wchar_t* driverFilePath)
{
    bool returnValue = true;

    SC_HANDLE scManagerHandle = NULL;
    SC_HANDLE serviceHandle = NULL;

    scManagerHandle = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (scManagerHandle == NULL)
    {
        DWORD lastError = GetLastError();

        printf("Error: Failure open sc manager. (LastError %d)\r\n", lastError);
        returnValue = false;

        goto RETURN;
    }

    // More options?
    serviceHandle = CreateServiceW(scManagerHandle,
                                   driverName,
                                   driverName,
                                   SERVICE_ALL_ACCESS,
                                   SERVICE_KERNEL_DRIVER,
                                   SERVICE_DEMAND_START,
                                   SERVICE_ERROR_NORMAL,
                                   driverFilePath,
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL
    );


    if (serviceHandle == NULL)
    {
        DWORD lastError = GetLastError();

        printf("Error: Failure create service. (LastError %d)\r\n", lastError);
        returnValue = false;

        goto RETURN;
    }

    if (StartServiceW(serviceHandle, 0, NULL) != TRUE)
    {
        DWORD lastError = GetLastError();

        printf("Error: Failure start service. (LastError %d)\r\n", lastError);
        returnValue = false;

        goto RETURN;
    }

RETURN:
    if (scManagerHandle != NULL)
    {
        CloseServiceHandle(scManagerHandle);
    }

    if (serviceHandle != NULL)
    {
        CloseServiceHandle(serviceHandle);
    }

    return returnValue;
}


bool cleanDriverSys(const wchar_t* driverName)
{
    bool returnValue = true;

    SC_HANDLE scManagerHandle = NULL;
    SC_HANDLE serviceHandle = NULL;

    scManagerHandle = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (scManagerHandle == NULL)
    {
        DWORD lastError = GetLastError();

        printf("Error: Failure open sc manager. (LastError %d)\r\n", lastError);
        returnValue = false;

        goto RETURN;
    }

    serviceHandle = OpenServiceW(scManagerHandle, driverName, SERVICE_ALL_ACCESS);

    if (serviceHandle == NULL)
    {
        goto RETURN;
    }

    SERVICE_STATUS serviceStatus;

    if (!QueryServiceStatus(serviceHandle, &serviceStatus))
    {
        DWORD lastError = GetLastError();

        printf("Error: Failure query service. (LastError %d)\r\n", lastError);
        returnValue = false;

        goto RETURN;
    }

    if (serviceStatus.dwCurrentState == SERVICE_RUNNING)
    {
        if (ControlService(serviceHandle, SERVICE_CONTROL_STOP, &serviceStatus) != TRUE)
        {
            DWORD lastError = GetLastError();

            printf("Error: Failure stop service. (LastError %d)\r\n", lastError);
            returnValue = false;

            goto RETURN;
        }
    }

    if (serviceStatus.dwCurrentState != SERVICE_STOPPED)
    {
        printf("Error: Failure stop service.\r\n");
        returnValue = false;

        goto RETURN;
    }

    if (DeleteService(serviceHandle) != TRUE)
    {
        DWORD lastError = GetLastError();

        printf("Error: Failure delete service. (LastError %d)\r\n", lastError);
        returnValue = false;

        goto RETURN;
    }


RETURN:
    if (scManagerHandle != NULL)
    {
        CloseServiceHandle(scManagerHandle);
    }

    if (serviceHandle != NULL)
    {
        CloseServiceHandle(serviceHandle);
    }

    return returnValue;
}


#endif