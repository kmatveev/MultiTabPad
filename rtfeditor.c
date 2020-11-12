#include <richedit.h>
#include "editor.h"

DWORD CALLBACK saveCallback(DWORD dwCookie, LPBYTE byteBuffer, LONG bufferOffset, LONG* resultingBufferOffset);
DWORD CALLBACK loadCallback(DWORD dwCookie, LPBYTE byteBuffer, LONG bufferOffset, LONG* resultingBufferOffset);

void initEditor() {
    HINSTANCE richEditLibModuleHandle;
    richEditLibModuleHandle = LoadLibraryW(L"RICHED32.DLL");
    if (!richEditLibModuleHandle) {
        MessageBoxW(NULL, L"Error while starting application: could not load RICHED32.DLL", L"Note", MB_OK);
    }
}


HWND createEditorWindow(HINSTANCE appModuleHandle, HWND parentWinHandle, int visible) {

    HWND editorWinHandle;
    wchar_t buf[256];

    // ES_NOHIDESEL is important because without it selection will not be shown when "Find" dialog is active
    DWORD richEditStyle = WS_CHILD | WS_BORDER | WS_HSCROLL | WS_VSCROLL | ES_NOHIDESEL | ES_AUTOVSCROLL | ES_MULTILINE | ES_SAVESEL | ES_SUNKEN;
    richEditStyle = visible ? (richEditStyle | WS_VISIBLE) : richEditStyle;
    editorWinHandle = CreateWindowExW(0L, L"RICHEDIT", L"", richEditStyle, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, parentWinHandle, NULL, appModuleHandle, NULL);

    if (editorWinHandle == NULL) {
        // wsprintfW(buf, L"Error %d", GetLastError());
        // MessageBoxW(NULL, &buf, L"Note", MB_OK);
        return NULL;
    }

    // SetFocus(editorWinHandle);

    SendMessageW(editorWinHandle, EM_SETEVENTMASK, 0, ENM_SELCHANGE);

    return editorWinHandle;

}

BOOL saveToFile(HWND activeEditorHandle, wchar_t* fileName) {
    HANDLE outputFileHandle;
    EDITSTREAM es;

    outputFileHandle = CreateFileW(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (outputFileHandle == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    es.dwCookie = (DWORD)outputFileHandle;
    es.dwError = 0;
    es.pfnCallback = saveCallback;

        // wcsupr(&ofn.lpstrFile[ofn.nFileExtension]); 

        // if(!strncmp(&ofn.lpstrFile[ofn.nFileExtension],L"RTF",3))
        //    SendMessage(hwndEdit,EM_STREAMOUT,SF_RTF, (LPARAM)&es);
        //else
    SendMessageW(activeEditorHandle, EM_STREAMOUT, SF_TEXT, (LPARAM)&es);

    CloseHandle(outputFileHandle);

    return TRUE;
}

DWORD CALLBACK saveCallback(DWORD dwCookie, LPBYTE byteBuffer, LONG bytesToWrite, LONG* bytesActuallyWritten) {
    BOOL result;
    result = WriteFile((HFILE)dwCookie, byteBuffer, bytesToWrite, bytesActuallyWritten, NULL);
    // TODO analyze result
    return 0;
}

BOOL loadFromFile(HWND activeEditorHandle, wchar_t* fileName) {

    HANDLE inputFileHandle;
    EDITSTREAM es;

    inputFileHandle = CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (inputFileHandle == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    es.dwCookie = (DWORD)inputFileHandle;
    es.dwError = 0;
    es.pfnCallback = loadCallback;

    // _strupr(&ofn.lpstrFile[ofn.nFileExtension]); 
    // if(!strncmp(&ofn.lpstrFile[ofn.nFileExtension],"RTF",3))
    // SendMessage(hwndEdit,EM_STREAMIN,SF_RTF,(LPARAM)&es);
    // else
    SendMessageW(activeEditorHandle, EM_STREAMIN, SF_TEXT, (LPARAM)&es);

    CloseHandle(inputFileHandle);

}

DWORD CALLBACK loadCallback(DWORD dwCookie, LPBYTE byteBuffer, LONG bytesToRead, LONG* bytesActuallyRead) {
    BOOL result;
    result = ReadFile((HANDLE)dwCookie, byteBuffer, bytesToRead, bytesActuallyRead, NULL);
    // TODO analyze result
    if (*bytesActuallyRead <= 0) {
        *bytesActuallyRead = 0;
    }
    return 0;
}

BOOL findText(HWND editorWindowHandle, int* searchOffset, wchar_t* searchString) {
    int    textLength, foundOccurenceStartOffset;
    wchar_t *textBuffer, *textPosition;

    // Read in the edit document

    textLength = GetWindowTextLengthW(editorWindowHandle);
    // allocate memory for text from editor
    textBuffer = HeapAlloc(GetProcessHeap(), 0, (textLength + 1) * sizeof(wchar_t));

    GetWindowTextW(editorWindowHandle, textBuffer, textLength + 1);

    textPosition = wcsstr(textBuffer + *searchOffset, searchString);
    HeapFree(GetProcessHeap(), 0, textBuffer);

    // Return an error code if the string cannot be found

    if (textPosition == NULL) {
        return FALSE;
    }

    // Find the position in the document and the new start offset

    foundOccurenceStartOffset = textPosition - textBuffer;
    *searchOffset = foundOccurenceStartOffset + wcslen(searchString);

    // Select the found text
    SendMessageW(editorWindowHandle, EM_SETSEL, foundOccurenceStartOffset, *searchOffset);
    SendMessageW(editorWindowHandle, EM_SCROLLCARET, 0, 0);

    return TRUE;
}
