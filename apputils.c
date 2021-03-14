#include <windows.h>
#include <Shlwapi.h>
#include <strsafe.h>
#include <wchar.h>

#include "apputils.h"

BYTE* readConfigFile() {

    char appModuleFilename[MAX_PATH];
    char appConfigFilename[MAX_PATH];
    HRESULT result;
    HANDLE configFileHandle;
    int configFileSize, bytesRead;
    BYTE* configFileContentBuffer;
    UINT codePage;
    char* fontPropertyStart, * fontPropertyEnd;

    // here we deliberatelly use byte-oriented functions

    // NULL means "module of current process"
    GetModuleFileNameA(NULL, appModuleFilename, MAX_PATH);
    result = StringCchCopyA(appConfigFilename, MAX_PATH, appModuleFilename); // TODO check result
    PathRemoveFileSpecA(appConfigFilename);
    PathAppendA(appConfigFilename, "defaults.ini");

    configFileHandle = CreateFileA(appConfigFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (configFileHandle == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    // Get file size in bytes and allocate memory for read.
    // Add an extra two bytes for zero termination.
    configFileSize = GetFileSize(configFileHandle, NULL);
    configFileContentBuffer = HeapAlloc(GetProcessHeap(), 0, configFileSize + 1);

    // Read file and put terminating zeros at end.
    // TODO check if whole file was read
    ReadFile(configFileHandle, configFileContentBuffer, configFileSize, &bytesRead, NULL);
    CloseHandle(configFileHandle);
    configFileContentBuffer[configFileSize] = '\0';

    return configFileContentBuffer;
}

