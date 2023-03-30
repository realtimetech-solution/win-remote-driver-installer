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

//Iterate all hardware Ids (for all Models sections referred in Manufacturer section)
//and try UpdateDriverForPlugAndPlayDevices call
//returns true if UpdateDriverForPlugAndPlayDevices call succeeded at least once
bool iterateUpdatePnPDriver(const wchar_t* infFullPath) {
	UINT errorLine;
	HINF infHandle = SetupOpenInfFileW(infFullPath, NULL, INF_STYLE_WIN4, &errorLine);

	UINT installCount = 0;

	if (infHandle == INVALID_HANDLE_VALUE)
	{
		printf("Error: Failed to open inf file : %ld(%d)\r\n", errorLine, GetLastError());

		return false;
	}

	INFCONTEXT manufacturerContext;
	DWORD modelSectionNameSize;
	wchar_t modelSectionName[LINE_LEN];
	wchar_t deviceIdName[LINE_LEN];

	if (!SetupFindFirstLineW(infHandle, L"Manufacturer", NULL, &manufacturerContext)) {		//go to first line of specified section
		printf("Error: There is no 'Manufacturer' section : %d\r\n", GetLastError());

		return false;
	}

	while (true) {
		//get next models section's name
		//note that below will automatically concatenate models section name and user computer's architecture name
		wchar_t manufacturer[LINE_LEN];
		if (!SetupGetStringFieldW(&manufacturerContext, 0, manufacturer, LINE_LEN, NULL)) {
			//no more manufacturer exists
			goto RETURN;
		}

		if (!SetupDiGetActualModelsSectionW(&manufacturerContext, NULL, modelSectionName, LINE_LEN, NULL, NULL)) {
			printf("Error: Getting models section for manufacturer %S failed.\r\n", manufacturer);

			return 1;
		}

		INFCONTEXT deviceIdContext;

		// Move to next models section
		if (!SetupFindFirstLineW(infHandle, modelSectionName, NULL, &deviceIdContext)) {
			//if models section mentioned in Inf file is not compatible with caller's architecture, move to next models section referred in manufacturer section
			printf("Warning: There is no '%S' section : %d\r\n", modelSectionName, GetLastError());

			if (!SetupFindNextLine(&manufacturerContext, &manufacturerContext)) {
				//no more lines in manufacturer section
				goto RETURN;
			}

			continue;
		}

		//iterate all hardware Id in current models section
		while (true) {
			if (!SetupGetStringFieldW(&deviceIdContext, 2, deviceIdName, LINE_LEN, NULL)) {
				//current line is not descripting hardward Id
				//move on to the next models section
				if (!SetupFindNextLine(&manufacturerContext, &manufacturerContext)) {
					goto RETURN;
				}

				break;
			}

			if (!UpdateDriverForPlugAndPlayDevicesW(NULL, deviceIdName, infFullPath, INSTALLFLAG_FORCE, false)) {
				printf("Info : Failed to install inf. Move to next hardware id\r\n");

				//move to next line descripting hardware Id
				if (!SetupFindNextLine(&deviceIdContext, &deviceIdContext)) {
					//no more compatible hardware id, move on to next models section
					if (!SetupFindNextLine(&manufacturerContext, &manufacturerContext)) {
						goto RETURN;
					}
					break;
				}
			}
			else {
				//successfuly found target hardware id in current section. Move on to the next section referred in manufacturer section
				installCount += 1;

				if (!SetupFindNextLine(&manufacturerContext, &manufacturerContext)) {
					goto RETURN;
				}

				break;
			}
		}
	}

RETURN:
	return installCount != 0 ? true : false;
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
			printf("The pnp driver is not installed \r\n");
			printf("The error is %x ", error);
		}
		else
		{
			printf("The pnp driver is installed correctly !");

			return true;
		}
	}
	else {
		//TODO : check whether DiInstallDriver call functions for non-PnP Inf files
		if (!DiInstallDriverW(NULL, infFullPath, DIIRFLAG_FORCE_INF, false)) {
			DWORD error = GetLastError();
			printf("The non-pnp driver is not installed \r\n");
			printf("The error is %x ", error);
		}
		else
		{
			printf("The non-pnp driver is installed correctly !");

			return true;
		}
	}

	return false;
}

bool cleanDriverInf() {
	printf("Cleaning Inf driver currently not implemented.\r\n");

	return true;
}