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

//Iterate hardware IDs in given Inf Model section
//and try to update PnP driver
bool iterateUpdatePnPDriver(const wchar_t* infFullPath) {
	printf("PNP\r\n");		//debug

	UINT errorLine;
	HINF infHandle = SetupOpenInfFileW(infFullPath, NULL, INF_STYLE_WIN4, &errorLine);

	if (infHandle == INVALID_HANDLE_VALUE)
	{
		printf("Error: Failed to open inf file : %ld(%d)\r\n", errorLine, GetLastError());

		return false;
	}

	INFCONTEXT infContext;

	//Iterate all compatible HW ids in INF Model sections and try UpdateDriverForPlugAandPlayDevices call
	//TODO : Currently Inf model section's name should be Standard.NTamd64
	if (!SetupFindFirstLineW(infHandle, L"Standard.NTamd64", NULL, &infContext)) {
		printf("Warning: There is no 'Standard.NTamd64' section : %d\r\n", GetLastError());

		return false;
	}

	LONG countHID = SetupGetLineCountW(infHandle, L"Standard.NTamd64");
	printf("HID count : %d\r\n", countHID);		//debug

	wchar_t	HID[LINE_LEN];
	for (int i = 0; i < countHID; i++) {
		if (!SetupGetStringFieldW(&infContext, 2, HID, LINE_LEN, NULL)) {
			printf("Error: Failed to get HID : %d\r\n", GetLastError());

			return false;
		}

		//debug
		printf("HID : %S", HID);
		printf("\r\n");

		if (!UpdateDriverForPlugAndPlayDevicesW(NULL, HID, infFullPath, INSTALLFLAG_FORCE, false)) {
			DWORD error = GetLastError();
			printf("Failed to install inf. Move to next HID\r\n");

			SetupFindNextLine(&infContext, &infContext);
		}
		else {
			return true;	//escape
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
	
	//hardwareId = L"PCI\\VEN_10EE&DEV_D000&SUBSYS_000E10EE&REV_00"
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