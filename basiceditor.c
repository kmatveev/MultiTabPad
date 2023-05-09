#include <windows.h>
#include "editor.h"

#define UNICODE
#define _UNICODE

void initEditor() {
    // nothing special is needed for basic editor
}


HWND createEditorWindow(HINSTANCE appModuleHandle, HWND parentWinHandle, int visible) {

    HWND editorWinHandle;
    wchar_t buf[256];
    DWORD editorStyle;

    // appModuleHandle = (appModuleHandle == NULL) ? (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE) : appModuleHandle;

    // ES_NOHIDESEL is important because without it selection will not be shown when "Find" dialog is active
    editorStyle = WS_CHILD | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_NOHIDESEL | ES_MULTILINE | ES_AUTOVSCROLL;
    editorStyle = visible ? (editorStyle | WS_VISIBLE) : editorStyle;
    // We just use predefined window class "EDIT" provided by common controls library
    editorWinHandle = CreateWindowExW(0, L"EDIT", NULL, editorStyle, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, parentWinHandle, NULL,  appModuleHandle, NULL);

    if (editorWinHandle == NULL) {
        // wsprintfW(buf, L"Error %d", GetLastError());
        // MessageBoxW(NULL, &buf, L"Note", MB_OK);
        return NULL;
    }

    return editorWinHandle;

}

BOOL loadFromFile(HWND activeEditorHandle, wchar_t* fileName) {
    BYTE   bySwap;
    DWORD  bytesRead;
    HANDLE inputFileHandle;
    int    i, fileSize, iUniTest;
    PBYTE  byteBuffer, textBuffer;
    PTSTR  convertedTextBuffer;
    UINT codePage;

    // Open the file.
    
    inputFileHandle = CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (inputFileHandle == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    // Get file size in bytes and allocate memory for read.
    // Add an extra two bytes for zero termination.
    fileSize = GetFileSize(inputFileHandle, NULL);
    byteBuffer = HeapAlloc(GetProcessHeap(), 0, fileSize + 2);

    // Read file and put terminating zeros at end.

    ReadFile(inputFileHandle, byteBuffer, fileSize, &bytesRead, NULL);
    CloseHandle(inputFileHandle);
    byteBuffer[fileSize] = '\0';
    byteBuffer[fileSize + 1] = '\0';

    // Test to see if the text is Unicode

    iUniTest = IS_TEXT_UNICODE_SIGNATURE | IS_TEXT_UNICODE_REVERSE_SIGNATURE;

    if (IsTextUnicode(byteBuffer, fileSize, &iUniTest)) {
        textBuffer = byteBuffer + 2;
        fileSize -= 2;

        if (iUniTest & IS_TEXT_UNICODE_REVERSE_SIGNATURE) {
            for (i = 0; i < fileSize / 2; i++)  {
                bySwap = ((BYTE*)textBuffer)[2 * i];
                ((BYTE*)textBuffer)[2 * i] = ((BYTE*)textBuffer)[2 * i + 1];
                ((BYTE*)textBuffer)[2 * i + 1] = bySwap;
            }
        }

        // Allocate memory for possibly converted string
        convertedTextBuffer = HeapAlloc(GetProcessHeap(), 0, fileSize + 2);

        // Since the edit control is Unicode, just copy the string
        // we use byte-level copying
        StringCchCopyA(convertedTextBuffer, fileSize + 1, textBuffer);

    } else  {
        // the file is not Unicode

        textBuffer = byteBuffer;
        // Allocate memory for converted string.
        convertedTextBuffer = HeapAlloc(GetProcessHeap(), 0, (2 * fileSize + 2));
        codePage = CP_ACP; // ANSI code page
        MultiByteToWideChar(codePage, 0, textBuffer, -1, convertedTextBuffer, fileSize + 1);

    }

    SetWindowTextW(activeEditorHandle, convertedTextBuffer);
    HeapFree(GetProcessHeap(), 0, byteBuffer);
    HeapFree(GetProcessHeap(), 0, convertedTextBuffer);

    return TRUE;
}

BOOL saveToFile(HWND activeEditorHandle, wchar_t* fileName) {
    DWORD  bytesWritten;
    HANDLE fileHandle;
    int    textSize;
    PTSTR  textBuffer;
    WORD   wByteOrderMark = 0xFEFF;

    // Open the file, creating it if necessary
    fileHandle = CreateFileW(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);


    if (fileHandle == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    // Get the number of characters in the edit control and allocate
    // memory for them.

    textSize = GetWindowTextLengthW(activeEditorHandle);
    textBuffer = HeapAlloc(GetProcessHeap(), 0, ((textSize + 1) * sizeof(wchar_t)));

    if (!textBuffer) {
        CloseHandle(fileHandle);
        return FALSE;
    }

    // If the edit control will return Unicode text, write the
    // byte order mark to the file.

    WriteFile(fileHandle, &wByteOrderMark, 2, &bytesWritten, NULL);

    // Get the edit buffer and write that out to the file.

    GetWindowTextW(activeEditorHandle, textBuffer, textSize + 1);
    WriteFile(fileHandle, textBuffer, textSize * 2, &bytesWritten, NULL);

    if ((textSize * 2) != (int)bytesWritten) {
        CloseHandle(fileHandle);
        HeapFree(GetProcessHeap(), 0, textBuffer);
        return FALSE;
    }

    CloseHandle(fileHandle);
    HeapFree(GetProcessHeap(), 0, textBuffer);
    return TRUE;
}

BOOL findText(HWND editorWindowHandle, int* searchOffset, wchar_t* searchString) {
    int    textLength, foundOccurenceStartOffset;
    wchar_t  *textBuffer, *textPosition;

    HLOCAL editorBufferHandle = SendMessageW(editorWindowHandle, EM_GETHANDLE, 0, 0);
    textBuffer = LocalLock(editorBufferHandle);

    textPosition = wcsstr(textBuffer + *searchOffset, searchString);
    LocalUnlock(editorBufferHandle);

    // Return an indication code if the string cannot be found
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

