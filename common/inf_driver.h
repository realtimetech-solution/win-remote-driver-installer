#include <Windows.h>
#include <Setupapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <Newdev.h>
#include <cfgmgr32.h>
#include <stdbool.h>

#pragma comment (lib, "newdev.lib")
#pragma comment (lib, "Setupapi.lib")

bool isPnPDriver(const wchar_t* infPath) {
	wchar_t infFullPath[MAX_PATH];
	size_t infFullPathLength = GetFullPathNameW(infPath, MAX_PATH, infFullPath, NULL);

	if (infFullPathLength < 0 || infFullPathLength >= MAX_PATH)
	{
		printf("Error: Inf file path is not appropriate\r\n");

		return false;
	}

	UINT errorLine;
	HINF infHandle = SetupOpenInfFileW(infFullPath, NULL, INF_STYLE_WIN4, &errorLine);

	if (infHandle == INVALID_HANDLE_VALUE)
	{
		printf("Error: Failed to open inf file : %ld(%d)\r\n", errorLine, GetLastError());

		return false;
	}

	INFCONTEXT infContext;

	if (!SetupFindFirstLineW(infHandle, L"Version", L"DriverPackageType", &infContext)) {
		printf("Warning: There is no 'DriverPackageType' field in 'Version' section : %d\r\n", GetLastError());

		return false;
	}

	wchar_t value[LINE_LEN];

	if (!SetupGetStringFieldW(&infContext, 1, value, LINE_LEN, NULL)) {
		printf("Error: Failed to get 'DriverPackageType' field value : %d\r\n", GetLastError());

		return false;
	}

	if (_wcsicmp(value, L"PlugAndPlay") != 0) {
		return false;
	}

	return true;
}

//Iterate all hardware Ids (in all Models sections)
//and try UpdateDriverForPlugAandPlayDevices call
bool iterateUpdatePnPDriver(const wchar_t* infFullPath) {
	UINT errorLine;
	HINF infHandle = SetupOpenInfFileW(infFullPath, NULL, INF_STYLE_WIN4, &errorLine);

	if (infHandle == INVALID_HANDLE_VALUE)
	{
		printf("Error: Failed to open inf file : %ld(%d)\r\n", errorLine, GetLastError());

		return false;
	}

	INFCONTEXT infContext;
	DWORD modelSectionNameSize;
	wchar_t modelSectionName[LINE_LEN];
	wchar_t deviceIdName[LINE_LEN];

	if (!SetupFindFirstLineW(infHandle, L"Manufacturer", NULL, &infContext)) {
		printf("Error: There is no 'Manufacturer' section : %d\r\n", GetLastError());

		return false;
	}

	while (true) {
		//get next models section's name
		//note that below will automatically concatenate models section name and user computer's architecture name
		if (!SetupDiGetActualModelsSectionW(&infContext, NULL, modelSectionName, LINE_LEN, NULL, NULL)) {
			printf("Error: SetupDiGetActualModelsSectionW(&infContext, NULL, NULL, 0, 0, NULL) : %d\r\n", GetLastError());

			return false;
		}

		INFCONTEXT infContextForFindDeviceId;

		// Move to next models section
		if (!SetupFindFirstLineW(infHandle, modelSectionName, NULL, &infContextForFindDeviceId)) {
			printf("Warning: There is no '%S' section : %d\r\n", modelSectionName, GetLastError());

			return false;
		}

		while (true) {
			if (!SetupGetStringFieldW(&infContextForFindDeviceId, 2, deviceIdName, LINE_LEN, NULL)) {
				break;
			}

			//printf("%S\r\n", deviceIdName);

			if (!UpdateDriverForPlugAndPlayDevicesW(NULL, deviceIdName, infFullPath, INSTALLFLAG_FORCE, false)) {
				printf("Info : Failed to install inf. Move to next hardware id\r\n");
			}
			else {
				return true;
			}

			//move to next line descripting hardware Id
			if (!SetupFindNextLine(&infContextForFindDeviceId, &infContextForFindDeviceId)) {
				break;
			}
		}

		if (!SetupFindNextLine(&infContext, &infContext)) {
			break;
		}
	}

	return false;	//No valid HID matched
}

bool installInfDriver(const wchar_t* infPath) {
	bool infFlag = 0;

	if (isPnPDriver(infPath)) infFlag = 1;

	wchar_t infFullPath[MAX_PATH];
	size_t infFullPathLength = GetFullPathNameW(infPath, MAX_PATH, infFullPath, NULL);

	if (infFullPathLength < 0 || infFullPathLength >= MAX_PATH)
	{
		printf("Error: Inf file path is not appropriate\r\n");

		return false;
	}
	
	if (infFlag) {
		if (!iterateUpdatePnPDriver(infFullPath)) {
			DWORD error = GetLastError();
			printf("The driver is not installed \r\n");
			printf("The error is %x ", error);
		}
		else
		{
			printf("The driver is installed correctly !");

			return false;
		}
	}
	else {
		//TODO : check whether DiInstallDriver call functions for non-PnP Inf files
		if (!DiInstallDriverW(NULL, infFullPath, DIIRFLAG_FORCE_INF, false)) {
			DWORD error = GetLastError();
			printf("The driver is not installed \r\n");
			printf("The error is %x ", error);
		}
		else
		{
			printf("The driver is installed correctly !");

			return false;
		}
	}

	return true;
}

bool cleanDriverInf() {
	printf("Cleaning Inf driver currently not implemented.\r\n");

	return true;
}