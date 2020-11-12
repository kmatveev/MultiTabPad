void initEditor();

HWND createEditorWindow(HINSTANCE appModuleHandle, HWND parentWinHandle, int visible);

BOOL saveToFile(HWND activeEditorHandle, wchar_t* fileName);
BOOL loadFromFile(HWND activeEditorHandle, wchar_t* fileName);

BOOL findText(HWND editorWindowHandle, int* searchOffset, wchar_t* searchString);