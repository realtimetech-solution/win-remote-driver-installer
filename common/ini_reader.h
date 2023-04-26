#include "wrdi.h"
#include <Windows.h>

#define MAX_INI_STRING_LENGTH   256
#define MAX_INI_FIELD_COUNT     32

typedef struct INIContext_t
{
    int         items;
    wchar_t     fieldKeys[MAX_INI_FIELD_COUNT][MAX_INI_STRING_LENGTH];
    wchar_t     fieldValues[MAX_INI_FIELD_COUNT][MAX_INI_STRING_LENGTH];
} INIContext;

bool ReadINIFormFile(INIContext* iniContext, const wchar_t* filePath)
{
    iniContext->items = 0;

    wchar_t fullPath[MAX_PATH];

    if (!GetResolvedFullPath(filePath, fullPath, MAX_PATH))
    {
        printf("Error: Ini file is not exists.\r\n");

        return false;
    }

    wchar_t sectionNamesBuffer[MAX_INI_STRING_LENGTH];
    DWORD sectionNamesLength = GetPrivateProfileSectionNamesW(sectionNamesBuffer, MAX_INI_STRING_LENGTH, fullPath);
    int startSectionNameIndex = 0;

    if (sectionNamesLength < 0)
    {
        printf("Error: Failed to read ini sections.\r\n");

        return false;
    }

    if (sectionNamesLength - 2 >= MAX_INI_STRING_LENGTH)
    {
        printf("Error: Ini section is too long.\r\n");

        return false;
    }

    for (DWORD sectionNameIndex = 0; sectionNameIndex <= sectionNamesLength; sectionNameIndex++)
    {
        if (sectionNamesBuffer[sectionNameIndex] == '\0')
        {
            if (sectionNameIndex >= sectionNamesLength)
            {
                break;
            }

            wchar_t* sectionName = sectionNamesBuffer + startSectionNameIndex;
            int sectionNameLength = sectionNameIndex - startSectionNameIndex;

            wchar_t keyNamesBuffer[MAX_INI_STRING_LENGTH];
            DWORD keyNamesLength = GetPrivateProfileString(sectionName, NULL, L"", keyNamesBuffer, MAX_INI_STRING_LENGTH, fullPath);
            int startKeyNameIndex = 0;

            if (keyNamesLength < 0)
            {
                printf("Error: Failed to read ini key name.\r\n");

                return false;
            }

            if (keyNamesLength - 2 >= MAX_INI_STRING_LENGTH)
            {
                printf("Error: Ini key name is too long.\r\n");

                return false;
            }


            for (DWORD keyNameIndex = 0; keyNameIndex <= keyNamesLength; keyNameIndex++)
            {
                if (keyNamesBuffer[keyNameIndex] == '\0')
                {
                    if (keyNameIndex >= keyNamesLength)
                    {
                        break;
                    }

                    wchar_t* keyName = keyNamesBuffer + startKeyNameIndex;
                    int keyNameLength = keyNameIndex - startKeyNameIndex;

                    DWORD fieldValueLength = GetPrivateProfileString(sectionName, keyName, L"", iniContext->fieldValues[iniContext->items], MAX_INI_STRING_LENGTH, fullPath);

                    wcscpy_s(iniContext->fieldKeys[iniContext->items], MAX_INI_STRING_LENGTH, sectionName);
                    wcscat_s(iniContext->fieldKeys[iniContext->items], MAX_INI_STRING_LENGTH, L".");
                    wcscat_s(iniContext->fieldKeys[iniContext->items], MAX_INI_STRING_LENGTH, keyName);

                    if (fieldValueLength == 0)
                    {
                        startKeyNameIndex = keyNameIndex + 1;
                    }
                    else
                    {
                        if (fieldValueLength - 1 >= MAX_INI_STRING_LENGTH)
                        {
                            printf("Error: Ini value is too long.\r\n");

                            return false;
                        }

                        iniContext->items++;

                        startKeyNameIndex = keyNameIndex + 1;
                    }
                }
            }

            startSectionNameIndex = sectionNameIndex + 1;
        }
    }


    return true;
}

bool ContainsINIField(const INIContext* iniContext, const wchar_t* fieldKey)
{
    for (int index = 0; index < iniContext->items; index++)
    {
        if (wcscmp(iniContext->fieldKeys[index], fieldKey) == 0)
        {
            return true;
        }
    }

    return false;
}

bool GetINIFieldValue(const INIContext* iniContext, const wchar_t* fieldKey, wchar_t* value, const size_t valueLength)
{
    for (int index = 0; index < iniContext->items; index++)
    {
        if (wcscmp(iniContext->fieldKeys[index], fieldKey) == 0)
        {
            size_t fieldValueLength = wcsnlen_s(iniContext->fieldValues[index], MAX_INI_STRING_LENGTH);

            if (valueLength <= fieldValueLength)
            {
                return false;
            }

            wcscpy_s(value, valueLength, iniContext->fieldValues[index]);

            return true;                  
        }
    }

    return false;
}