#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h> // we must link with comctl32.lib
#include <commdlg.h>  // we must link with comdlg32.lib
#include <Richedit.h>
#include <wchar.h>
#include <stdio.h>
#include <Shlwapi.h>
#include <strsafe.h>

#include "editor.h"  // interface between editor implementation and container window
#include "apputils.h"

#include "resource.h"

HINSTANCE g_appModuleHandle; // we don't want to name it hInstance since this name is used in WinMain function parameter
wchar_t appWindowClassName[] = L"MultiTabPadWinClass";
wchar_t mainWindowTitle[512];

HWND g_mainWindowHandle;

// will use this structure to group fields which describe tab header and editor
struct TabEditorsInfo {
    int tabWindowIdentifier;
    int tabIncrementor;
    HWND parentWinHandle;
    HWND tabCtrlWinHandle;
    HMENU tabMenuHandle;
    LOGFONT editorFontProperties;
    HFONT editorFontHandle;
  };

// single global instance of TabEditorsInfo
struct TabEditorsInfo g_tabEditorsInfo;

HWND g_findReplaceDialogHandle;
// content of find string is persisted between find attempts
#define MAX_FIND_STRING_LEN   256
wchar_t g_findText[MAX_FIND_STRING_LEN];
wchar_t g_replText[MAX_FIND_STRING_LEN];

// application properties loaded from file
wchar_t fontPropertyVal[LF_FACESIZE];

void defaultApplicationConfig();
void readApplicationConfig();
LRESULT CALLBACK WinProc(HWND hWind, UINT msg, WPARAM wParam, LPARAM lParam);
HWND createTabControl(struct TabEditorsInfo *tabEditorsInfo, HWND parentWinHandle);
void createTab(HWND tabCtrlWinHandle, int suffix, int index);
void createTabWithEditor(struct TabEditorsInfo *tabEditorsInfo, BOOL visible);
void removeTabWithEditor(struct TabEditorsInfo *tabEditorsInfo, int i);
void moveTabWithEditor(struct TabEditorsInfo* tabEditorsInfo, int tabIndex, int newPosition);
void selectedTabToRightmost();
void selectedTabToRight();
void selectedTabToLeftmost();
void selectedTabToLeft();
HWND getEditorForTabItem(HWND tabCtrlWinHandle, int i);
wchar_t* getFilenameForTabItem(HWND tabCtrlWinHandle, int i);
void showEditorForSelectedTabItem(HWND tabCtrlWinHandle, int selected);
HWND getEditorForCurrentTab(struct TabEditorsInfo *tabEditorsInfo);
void setFileForTabItem(HWND tabCtrlWinHandle, int tabIndex, wchar_t* heapAllocatedFileName);
LRESULT processTabNotification(HWND tabCtrlWinHandle, HMENU tabMenuHandle, HWND menuCommandProcessorWindowHandle, int code);
LRESULT processEditorNotification(HWND editorWinHandle, LPNMHDR nmhdr);
HRESULT resizeTabControl(struct TabEditorsInfo* tabEditorsInfo, RECT rc);
BOOL setEditorPos(HWND editorWinHandle, RECT rectangle);
void selectTab(HWND tabCtrlWinHandle, int i);

void chooseFont(HWND topLevelWindowHandle, struct TabEditorsInfo* tabEditorsInfo);
void applyFontToAllEditors(HWND tabCtrlWinHandle, HFONT newFontHandle);
void fileSave(HWND editorWinHandle, wchar_t* prevFileName);
wchar_t* fileSaveAs(HWND topLevelWindowHandle, HWND editorWinHandle, wchar_t* prevFileName);
wchar_t* fileOpen(HWND topLevelWindowHandle, HWND editorWindowHandle);

HWND showFindDialog(HWND topLevelWindowHandle, wchar_t* searchStringBuffer, int searchStringBuffSize);
HWND showFindReplaceDialog(HWND topLevelWindowHandle, wchar_t* searchStringBuffer, int searchStringBuffSize, wchar_t* replaceStringBuffer, int replaceStringBuffSize);
BOOL findReplaceText(HWND editorWindowHandle, int* searchOffset, LPFINDREPLACE pfr);


void DebugRectMsgBox(RECT rc);

// Data associated with each tab control item. We will use it instead of TCITEM. First member must be TCITEMHEADER, other members we can freely define
typedef struct tagTCCUSTOMITEM {
    TCITEMHEADER tcitemheader;
    HWND editorWindowHandle;
    wchar_t* fileName;
} TCCUSTOMITEM;

int WINAPI WinMain(HINSTANCE appModuleHandle, HINSTANCE alwaysNull, LPSTR cmdLine, int cmdShow) {

    INITCOMMONCONTROLSEX icex;
    WNDCLASSEXW winClass;
    MSG msg;
    HACCEL acceleratorsHandle;

    LoadStringW(appModuleHandle, IDS_APP_TITLE, mainWindowTitle, 512);

    g_appModuleHandle = appModuleHandle;  // save app module handle in global variable

    defaultApplicationConfig();
    readApplicationConfig();

    // Initialize common controls.
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);


    // We need to manually load this library, so it's initialization code will create a window class for RichEditor
    initEditor();

    // since it is allocated on stack, we need to clear this memory before we use it
    memset(&winClass, 0, sizeof(winClass));

    winClass.cbSize = sizeof(WNDCLASSEX);
    winClass.hIconSm = LoadImageW(g_appModuleHandle, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 16, 16, 0);
    winClass.style = CS_HREDRAW | CS_VREDRAW ;
    winClass.lpfnWndProc = (WNDPROC)WinProc;
    winClass.cbClsExtra = 0;
    winClass.cbWndExtra = 0;
    winClass.hInstance = g_appModuleHandle;

    winClass.hIcon = LoadImage(g_appModuleHandle, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 32, 32, 0);
    winClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    winClass.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    winClass.lpszMenuName = MAKEINTRESOURCEW(IDM_MAINMENU);
    winClass.lpszClassName = appWindowClassName;

    if (!RegisterClassEx(&winClass)) {
        return FALSE;
    }

    g_mainWindowHandle = CreateWindowW(appWindowClassName, mainWindowTitle, WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS , CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, g_appModuleHandle, NULL);

    if (!g_mainWindowHandle) {
       return FALSE;
    }

    ShowWindow(g_mainWindowHandle, cmdShow);
    UpdateWindow(g_mainWindowHandle);

    acceleratorsHandle = LoadAcceleratorsW(g_appModuleHandle, L"MTPAD");

    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (g_findReplaceDialogHandle == NULL || !IsDialogMessage(g_findReplaceDialogHandle, &msg)) {
            if (!TranslateAcceleratorW(g_mainWindowHandle, acceleratorsHandle, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }
    return msg.wParam;

}

void defaultApplicationConfig() {
    StringCchCopyW(fontPropertyVal, sizeof(fontPropertyVal) / sizeof(wchar_t), &(L"Lucida console"));
    return;
}

void readApplicationConfig() {

    UINT codePage = CP_ACP; // ANSI code page
    char* fontPropertyStart, * fontPropertyEnd;
    BYTE* configFileContentBuffer;

    configFileContentBuffer = readConfigFile();

    fontPropertyStart = StrStrA(configFileContentBuffer, "Font=");
    if (fontPropertyStart != NULL) {
        fontPropertyStart = fontPropertyStart + 5; // length of "Font="
        fontPropertyEnd = StrChrA(fontPropertyStart, '\r');
        if (fontPropertyEnd != NULL) {
            MultiByteToWideChar(codePage, 0, fontPropertyStart, -1, fontPropertyVal, fontPropertyEnd - fontPropertyStart);
        }  else {
            // TODO support last lines in fine which don't end with \r
        }
    }

}

LRESULT CALLBACK WinProc(HWND windowHandle, UINT msg, WPARAM wParam, LPARAM lParam) {

    static UINT messageFromFindDialog;

    switch (msg) {
        case WM_CREATE:

            // "Find/Replace" dialog doesn't specify any constant window message
            // Instead, it uses RegisterWindowMessage for constant string to generate unique message id.
            // So, here we use the same, and should get the same message id
            messageFromFindDialog = RegisterWindowMessageW(FINDMSGSTRINGW);

            // here we specify default properties of font shared by all editor instances
            // these could be later changed via "Choose font" dialog
            g_tabEditorsInfo.editorFontProperties.lfHeight = -17; // this height seems fine
            g_tabEditorsInfo.editorFontProperties.lfWidth = 0;
            g_tabEditorsInfo.editorFontProperties.lfEscapement = 0;
            g_tabEditorsInfo.editorFontProperties.lfOrientation = 0;
            g_tabEditorsInfo.editorFontProperties.lfWeight = FW_NORMAL;
            g_tabEditorsInfo.editorFontProperties.lfItalic = FALSE;
            g_tabEditorsInfo.editorFontProperties.lfUnderline = FALSE;
            g_tabEditorsInfo.editorFontProperties.lfStrikeOut = FALSE;
            g_tabEditorsInfo.editorFontProperties.lfCharSet = ANSI_CHARSET;
            g_tabEditorsInfo.editorFontProperties.lfOutPrecision = OUT_DEFAULT_PRECIS;
            g_tabEditorsInfo.editorFontProperties.lfClipPrecision = CLIP_DEFAULT_PRECIS;
            g_tabEditorsInfo.editorFontProperties.lfQuality = DEFAULT_QUALITY;
            g_tabEditorsInfo.editorFontProperties.lfPitchAndFamily = DEFAULT_PITCH;
            wcscpy(g_tabEditorsInfo.editorFontProperties.lfFaceName, fontPropertyVal);
            g_tabEditorsInfo.editorFontHandle = CreateFontIndirectW(&(g_tabEditorsInfo.editorFontProperties));

            createTabControl(&g_tabEditorsInfo, windowHandle);

            if (g_tabEditorsInfo.tabCtrlWinHandle == NULL) {
                MessageBoxW(NULL, L"Error while creating main application window: could not create tab control", L"Note", MB_OK);
            } else {

                g_tabEditorsInfo.tabMenuHandle = LoadMenuW(g_appModuleHandle, MAKEINTRESOURCEW(IDM_TABMENU));
                g_tabEditorsInfo.tabMenuHandle = GetSubMenu(g_tabEditorsInfo.tabMenuHandle, 0); // we can't show top-level menu, we must use PopupMenu, which is a single child of this menu

                // we want a single tab to be present in new window
                createTabWithEditor(&g_tabEditorsInfo, TRUE);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE: {
            RECT rc;
            // WM_SIZE params contain width and height of main window's client area
            // Since client area's left and top coordinates are both 0, having width and height gives us absolute coordinates of client's area
            SetRect(&rc, 0, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            resizeTabControl(&g_tabEditorsInfo, rc);
            return 0;
        }
        case WM_SETFOCUS: {
            SetFocus(getEditorForCurrentTab(&g_tabEditorsInfo));
            return 0;
        }
        case WM_INITMENUPOPUP: {
            HWND activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);
            int selectionBegin, selectionEnd, enableCutCopy, enableFind;
            switch (lParam) {
                case 1:  {           // Edit menu
                    // Enable Undo if edit control can do it
                    EnableMenuItem((HMENU)wParam, ID_EDIT_UNDO, SendMessage(activeEditorWinHandle, EM_CANUNDO, 0, 0L) ? MF_ENABLED : MF_GRAYED);
                    // Enable Paste if text is in the clipboard
                    EnableMenuItem((HMENU)wParam, ID_EDIT_PASTE, IsClipboardFormatAvailable(CF_TEXT) ? MF_ENABLED : MF_GRAYED);

                    // Enable Cut, Copy, and Del if text is selected
                    SendMessageW(activeEditorWinHandle, EM_GETSEL, (WPARAM)&selectionBegin, (LPARAM)&selectionEnd);

                    enableCutCopy = selectionBegin != selectionEnd ? MF_ENABLED : MF_GRAYED;

                    EnableMenuItem((HMENU)wParam, ID_EDIT_CUT, enableCutCopy);
                    EnableMenuItem((HMENU)wParam, ID_EDIT_COPY, enableCutCopy);
                    break;
                }
                case 2:  {           // Search menu

                    // Enable Find, Next, and Replace if modeless dialogs are not already active
                    UINT enableFind = g_findReplaceDialogHandle == NULL ? MF_ENABLED : MF_GRAYED;

                    EnableMenuItem((HMENU)wParam, ID_SEARCH_FIND, enableFind);
                    EnableMenuItem((HMENU)wParam, ID_SEARCH_FINDNEXT, enableFind);
                    EnableMenuItem((HMENU)wParam, ID_SEARCH_REPLACE, enableFind);
                    break;
                }
             }
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_FILE_NEW:
                    createTabWithEditor(&g_tabEditorsInfo, FALSE);
                    return 0;
                case ID_FILE_EXIT: {
                    int tabItemsCount = TabCtrl_GetItemCount(g_tabEditorsInfo.tabCtrlWinHandle);
                    BOOL haveUnsavedEditors = FALSE;
                    HWND editorHandle;
                    int sureToExitResponse;
                    for (int i = 0; i < tabItemsCount; i++) {
                        editorHandle = getEditorForTabItem(g_tabEditorsInfo.tabCtrlWinHandle, i);
                        haveUnsavedEditors = haveUnsavedEditors | SendMessageW(editorHandle, EM_GETMODIFY, 0, 0);
                    }
                    if (haveUnsavedEditors) {
                        sureToExitResponse = MessageBoxW(windowHandle, L"Some tabs have been modified and not saved, are you sure you want to quit?", L"Note", MB_YESNO);
                        if (sureToExitResponse == IDYES) {
                            PostQuitMessage(0);
                        }
                    } else {
                        PostQuitMessage(0);
                    }
                    return 0;
                }
                case ID_FILE_SAVE: {
                    HWND activeEditorWinHandle;
                    wchar_t* heapAllocatedFileName;
                    int currentTab = TabCtrl_GetCurSel(g_tabEditorsInfo.tabCtrlWinHandle);
                    activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);
                    heapAllocatedFileName = getFilenameForTabItem(g_tabEditorsInfo.tabCtrlWinHandle, currentTab); // this will be NULL if tab content was never saved
                    if (heapAllocatedFileName != NULL) {
                        fileSave(activeEditorWinHandle, heapAllocatedFileName);
                    } else {
                        heapAllocatedFileName = fileSaveAs(windowHandle, activeEditorWinHandle, heapAllocatedFileName);
                        if (heapAllocatedFileName != NULL) {
                            setFileForTabItem(g_tabEditorsInfo.tabCtrlWinHandle, currentTab, heapAllocatedFileName);
                        }
                    }
                    return 0;
                }
                case ID_FILE_SAVEAS: {
                    HWND activeEditorWinHandle;
                    wchar_t* heapAllocatedFileName;
                    int currentTab = TabCtrl_GetCurSel(g_tabEditorsInfo.tabCtrlWinHandle);
                    activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);
                    heapAllocatedFileName = getFilenameForTabItem(g_tabEditorsInfo.tabCtrlWinHandle, currentTab); // this will be NULL if tab content was never saved
                    heapAllocatedFileName = fileSaveAs(windowHandle, activeEditorWinHandle, heapAllocatedFileName);
                    if (heapAllocatedFileName != NULL) {
                        setFileForTabItem(g_tabEditorsInfo.tabCtrlWinHandle, currentTab, heapAllocatedFileName);
                    }
                    return 0;
                }
                case ID_FILE_OPEN: {
                    HWND activeEditorWinHandle;
                    wchar_t* heapAllocatedFileName;
                    int tabs, currentTab, createdTab;
                    tabs = TabCtrl_GetItemCount(g_tabEditorsInfo.tabCtrlWinHandle);
                    currentTab = TabCtrl_GetCurSel(g_tabEditorsInfo.tabCtrlWinHandle);
                    wchar_t* file = getFilenameForTabItem(g_tabEditorsInfo.tabCtrlWinHandle, currentTab);
                    activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);
                    BOOL isModified = SendMessageW(activeEditorWinHandle, EM_GETMODIFY, 0, 0);
                    if ((tabs > 1) || (file != NULL) || (isModified)) {
                        // files are opened in new tabs
                        createTabWithEditor(&g_tabEditorsInfo, TRUE);
                        createdTab = TabCtrl_GetCurSel(g_tabEditorsInfo.tabCtrlWinHandle);
                        activeEditorWinHandle = getEditorForTabItem(g_tabEditorsInfo.tabCtrlWinHandle, createdTab);
                    } else {
                        // if there were a single empty tab, reuse it
                        createdTab = currentTab;
                    }
                    // now, when tab was created and activated, we can load data from file
                    heapAllocatedFileName = fileOpen(windowHandle,activeEditorWinHandle);
                    if (heapAllocatedFileName != NULL) {
                        setFileForTabItem(g_tabEditorsInfo.tabCtrlWinHandle, createdTab, heapAllocatedFileName);
                    } else if (createdTab != currentTab) {
                        removeTabWithEditor(&g_tabEditorsInfo, createdTab);
                        selectTab(g_tabEditorsInfo.tabCtrlWinHandle, currentTab);
                    }
                    return 0;
                }
                case ID_FILE_CLOSE:
                case ID_TAB_CLOSE: {
                    HWND activeEditorWinHandle;
                    LRESULT isModified;
                    int saveYesNoCancel;
                    BOOL saveOk;
                    wchar_t* fileName;
                    int currentTab = TabCtrl_GetCurSel(g_tabEditorsInfo.tabCtrlWinHandle);
                    activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);
                    isModified = SendMessageW(activeEditorWinHandle, EM_GETMODIFY, 0, 0);
                    if (isModified) {
                        saveYesNoCancel = MessageBoxW(windowHandle, L"Content has been modified, do you want to save it?", L"Note", MB_YESNOCANCEL);
                        if (saveYesNoCancel == IDYES) {
                            fileName = getFilenameForTabItem(g_tabEditorsInfo.tabCtrlWinHandle, currentTab);
                            if (fileName != NULL) {
                                fileSave(activeEditorWinHandle, fileName);
                                removeTabWithEditor(&g_tabEditorsInfo, currentTab);
                            } else {
                                fileName = fileSaveAs(windowHandle, activeEditorWinHandle, fileName);
                                if (fileName != NULL) {
                                    // no real point to perist filename into tab data, since tab will be removed,
                                    // but to avoid memory leak we must either put this filename into tab data, so it is released by removeTab(), or just free this memory
                                    setFileForTabItem(g_tabEditorsInfo.tabCtrlWinHandle, currentTab, fileName);
                                    removeTabWithEditor(&g_tabEditorsInfo, currentTab);
                                }
                            }
                            return 0;
                        } else if (saveYesNoCancel == IDNO) {
                            // User chose "NO", so don't save, just close active editor
                            removeTabWithEditor(&g_tabEditorsInfo, currentTab);
                            return 0;
                        } else {
                            // User chose "CANCEL", don't do anything
                            return 0;
                        }
                    } else {
                        removeTabWithEditor(&g_tabEditorsInfo, currentTab);
                        return 0;
                    }
                }
                case ID_EDIT_UNDO: {
                    HWND activeEditorWinHandle;
                    activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);
                    SendMessageW(activeEditorWinHandle, EM_UNDO, 0, 0L);
                    return 0L;
                }
                case ID_EDIT_CUT: {
                    HWND activeEditorWinHandle;
                    activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);
                    SendMessageW(activeEditorWinHandle, WM_CUT, 0, 0L);
                    return 0L;
                }
                case ID_EDIT_COPY: {
                    HWND activeEditorWinHandle;
                    activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);
                    SendMessageW(activeEditorWinHandle, WM_COPY, 0, 0L);
                    return 0L;
                }
                case ID_EDIT_PASTE: {
                    HWND activeEditorWinHandle;
                    activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);
                    SendMessageW(activeEditorWinHandle, WM_PASTE, 0, 0L);
                    return 0L;
                }
                case ID_EDIT_DELETE: {
                    HWND activeEditorWinHandle;
                    activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);
                    SendMessageW(activeEditorWinHandle, WM_CLEAR, 0, 0L);
                    return 0L;
                }
                case ID_EDIT_SELECTALL: {
                    CHARRANGE charr;
                    HWND activeEditorWinHandle;

                    activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);
 
                    charr.cpMin = 0;
                    charr.cpMax = -1;

                    // SendMessageW(activeEditorWinHandle, EM_EXSETSEL, 0, (LPARAM)&charr);
                    SendMessageW(activeEditorWinHandle, EM_SETSEL, (WPARAM)charr.cpMin, (LPARAM)charr.cpMax); // works for both basic and rich editors
                    return 0L;
                }
                case ID_SEARCH_FIND: {
                    g_findReplaceDialogHandle = showFindDialog(windowHandle, g_findText, sizeof(g_findText)/sizeof(wchar_t));
                    return 0;
                }
                case ID_SEARCH_FINDNEXT: {
                    int searchPosition;
                    HWND activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);
                    if (*g_findText != L'\0') {
                        SendMessageW(activeEditorWinHandle, EM_GETSEL, 0, (LPARAM)&searchPosition);
                        findText(activeEditorWinHandle, &searchPosition, g_findText);
                    } else {
                        g_findReplaceDialogHandle = showFindDialog(windowHandle, g_findText, sizeof(g_findText) / sizeof(wchar_t));
                    }
                    return 0;
                }
                case ID_SEARCH_REPLACE: {
                    HWND activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);
                    g_findReplaceDialogHandle = showFindReplaceDialog(windowHandle, g_findText, sizeof(g_findText) / sizeof(wchar_t), g_replText, sizeof(g_replText)/sizeof(wchar_t));
                    return 0;
                }
                case ID_FORMAT_FONT: {
                    chooseFont(g_mainWindowHandle, &g_tabEditorsInfo);
                    return 0L;
                }
                case ID_HELP_ABOUT: {
                    MessageBoxW(g_mainWindowHandle, L"MultiTabPad by kmatveev", L"MultiTabPad", MB_OK | MB_ICONINFORMATION);
                    return 0L;
                }
                case ID_TAB_MOVETOLEFT: {
                    selectedTabToLeft();
                    return 0;
                }
                case ID_TAB_MOVETOLEFTMOST: {
                    selectedTabToLeftmost();
                    return 0;
                }
                case ID_TAB_MOVETORIGHT: {
                    selectedTabToRight();
                    return 0;
                }
                case ID_TAB_MOVETORIGHTMOST: {
                    selectedTabToRightmost();
                    return 0;
                }

                default:
                    return 0;
            }
        case WM_NOTIFY: {
          HWND activeEditorWinHandle;
          wchar_t buf[256];


          // Notifications are sent from TabControl window to it's parent window, so windowHandle will be a handle of parent window.
          // To indentify a source of a message, we should use nmhdr->hwndFrom
          LPNMHDR lpnmh = (LPNMHDR) lParam;
          if (lpnmh->hwndFrom == g_tabEditorsInfo.tabCtrlWinHandle) {
              // wsprintf(buf, L"Notification for %d, %d", wParam, lpnmh->idFrom); 
              // MessageBox(NULL, buf, L"Note", MB_OK);
              return processTabNotification(g_tabEditorsInfo.tabCtrlWinHandle, g_tabEditorsInfo.tabMenuHandle, g_mainWindowHandle, lpnmh->code);
          } else {
              activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);
              if (lpnmh->hwndFrom == activeEditorWinHandle) {
                  // wsprintf(buf, L"Editor notification for %d, %d", wParam, lpnmh->idFrom); 
                  // MessageBox(NULL, buf, L"Note", MB_OK);

                  return processEditorNotification(activeEditorWinHandle, lpnmh);  
              } else {
                  MessageBox(NULL, L"Notification for unknown window", L"Note", MB_OK);
                  return 0;
              }
          }
        }
        case WM_CONTEXTMENU: {
            // this message is sent from Editor when a context menu is requested
            // TabControl doesn't send this message
        }
        default: {
            LPFINDREPLACEW pfr;
            HWND activeEditorWinHandle;
            int searchPosition;
            // Here we process messages from "Find/Replace" dialog which use generated message id
            if (msg == messageFromFindDialog) {
                activeEditorWinHandle = getEditorForCurrentTab(&g_tabEditorsInfo);

                pfr = (LPFINDREPLACEW)lParam;

                // if user closed find dialog, then we just forget window handle
                if (pfr->Flags & FR_DIALOGTERM) {
                    g_findReplaceDialogHandle = NULL;
                }

                if (pfr->Flags & FR_FINDNEXT) {
                    SendMessageW(activeEditorWinHandle, EM_GETSEL, 0, (LPARAM)&searchPosition);
                    if (!findText(activeEditorWinHandle, &searchPosition, pfr->lpstrFindWhat)) {
                        MessageBoxW(g_findReplaceDialogHandle, L"Text not found!", L"\0", MB_OK);
                    }
                }

                if (pfr->Flags & FR_REPLACE || pfr->Flags & FR_REPLACEALL) {
                    SendMessageW(activeEditorWinHandle, EM_GETSEL, 0, (LPARAM)&searchPosition);
                    if (!findReplaceText(activeEditorWinHandle, &searchPosition, pfr)) {
                        MessageBoxW(g_findReplaceDialogHandle, L"Text not found!", L"\0", MB_OK);
                    }
                }

                if (pfr->Flags & FR_REPLACEALL) {
                    SendMessageW(activeEditorWinHandle, EM_GETSEL, 0, (LPARAM)&searchPosition);
                    while (findReplaceText(activeEditorWinHandle, &searchPosition, pfr));
                }

                return 0;

            }
            return(DefWindowProcW(windowHandle, msg, wParam, lParam));
        }
    }
}


// Creates a tab control, sized to fit the specified parent window's client area
// Returns the handle to the tab control. 
// parentWinHandle - parent window (the application's main window). 
// 
HWND createTabControl(struct TabEditorsInfo *tabEditorsInfo, HWND parentWinHandle) {
 
    HWND tabCtrlWinHandle; 
    LOGFONTW tabCaptionFont;
    HFONT tabCaptionFontHandle;  // consider exposing this to TabEditorsInfo

    tabEditorsInfo->tabWindowIdentifier = 3000;  // just some value which I've chosen. This value is passed as param to CreateWindow for tabCtrl, and will be returned in WM_NOTIFY messages
    tabEditorsInfo->tabIncrementor = 0;

    tabEditorsInfo->parentWinHandle = parentWinHandle;
    
    tabCtrlWinHandle = CreateWindowW(WC_TABCONTROL, L"", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, parentWinHandle, tabEditorsInfo->tabWindowIdentifier, g_appModuleHandle, NULL);
    if (tabCtrlWinHandle == NULL) { 
        return NULL; // Error happened, and we don't handle it here, invoker should call GetLastError()
    }
    tabEditorsInfo->tabCtrlWinHandle = tabCtrlWinHandle;

    // We are going to store custom application data associated with each tab item. To achieve that, we need to specify once how many bytes do we need for app data
    TabCtrl_SetItemExtra(tabCtrlWinHandle, sizeof(TCCUSTOMITEM) - sizeof(TCITEMHEADER));

    // Here we specify properties of font used in tab captions
    tabCaptionFont.lfHeight = -17; // this height seems fine
    tabCaptionFont.lfWidth = 0;
    tabCaptionFont.lfEscapement = 0;
    tabCaptionFont.lfOrientation = 0;
    tabCaptionFont.lfWeight = FW_NORMAL;
    tabCaptionFont.lfItalic = FALSE;
    tabCaptionFont.lfUnderline = FALSE;
    tabCaptionFont.lfStrikeOut = FALSE;
    tabCaptionFont.lfCharSet = ANSI_CHARSET;
    tabCaptionFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    tabCaptionFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    tabCaptionFont.lfQuality = DEFAULT_QUALITY;
    tabCaptionFont.lfPitchAndFamily = DEFAULT_PITCH;
    wcscpy(tabCaptionFont.lfFaceName, &(L"MS Shell dlg")); // this font is used by dialog controls
    tabCaptionFontHandle = CreateFontIndirectW(&tabCaptionFont);

    SendMessageW(tabCtrlWinHandle, WM_SETFONT, (WPARAM)tabCaptionFontHandle, FALSE);

    return tabCtrlWinHandle; 
}

void createTab(HWND tabCtrlWinHandle, int suffix, int index) {


    TCCUSTOMITEM tabCtrlItemInfo; 
    wchar_t tabNameBuf[256];  // Temporary buffer for strings.

    wsprintf(tabNameBuf, L"untitled%d", suffix);
 
    tabCtrlItemInfo.tcitemheader.mask = TCIF_TEXT | TCIF_IMAGE; 
    tabCtrlItemInfo.tcitemheader.iImage = -1; 
    tabCtrlItemInfo.tcitemheader.pszText = tabNameBuf;  // we can use temp buf, it's content will be copied by tab control
    // Since we set item's text (we don't read it), we don't need to specify cchTextMax. TabCtrl expects 0-terminated string

    tabCtrlItemInfo.editorWindowHandle = 0; // we have not yet created editorWindowHandle, so set 0 as "absent flag"
    tabCtrlItemInfo.fileName = NULL; // no file is associated with this tab yet
    
    TabCtrl_InsertItem(tabCtrlWinHandle, index, &tabCtrlItemInfo); // content of tabControlItemInfo will be copied 

}

void createTabWithEditor(struct TabEditorsInfo *tabEditorsInfo, BOOL visible) {

    RECT rc;
    POINT p;
    TCCUSTOMITEM tabCtrlItemInfo;
    int newTabIndex;
    HWND editWinHandle, tabCtrlWinHandle;

    tabCtrlWinHandle = tabEditorsInfo->tabCtrlWinHandle;

    newTabIndex = TabCtrl_GetItemCount(tabCtrlWinHandle);
    createTab(tabCtrlWinHandle, tabEditorsInfo->tabIncrementor, newTabIndex);  // we use num of current tabs as a suffix for default tab name
    editWinHandle = createEditorWindow(g_appModuleHandle, tabEditorsInfo->parentWinHandle, tabEditorsInfo->tabIncrementor == 0);
    SendMessageW(editWinHandle, WM_SETFONT, (WPARAM)(tabEditorsInfo->editorFontHandle), 0);

    if (editWinHandle == NULL) {
        MessageBoxW(NULL, L"Could not create edit window", L"Note", MB_OK);
        return;
    } else {

        // we need to associate window handle of rich edit with tab control item. We do that by using TabCtrl_SetItem with mask which specifies that only app data should be set
        tabCtrlItemInfo.tcitemheader.mask = TCIF_PARAM;
        tabCtrlItemInfo.editorWindowHandle = editWinHandle;
        tabCtrlItemInfo.fileName = NULL;
        TabCtrl_SetItem(tabCtrlWinHandle, newTabIndex, &tabCtrlItemInfo);

        GetClientRect(tabEditorsInfo->parentWinHandle, &rc);
        TabCtrl_AdjustRect(tabCtrlWinHandle, FALSE, &rc);
        setEditorPos(editWinHandle, rc);

        // newly created tab will become active
        selectTab(tabCtrlWinHandle, newTabIndex);
    }
    (tabEditorsInfo->tabIncrementor)++;
}

void removeTabWithEditor(struct TabEditorsInfo *tabEditorsInfo, int i) {
    int newTabItemsCount;
    int newSelectedTab;
    HWND tabCtrlWinHandle = tabEditorsInfo->tabCtrlWinHandle;

    // we should release those resouces associated with tab, so first we get references
    HWND editorWinHandle = getEditorForTabItem(tabCtrlWinHandle, i);
    wchar_t* heapAllocatedFileName = getFilenameForTabItem(tabCtrlWinHandle, i);
    // then we remove tab
    TabCtrl_DeleteItem(tabCtrlWinHandle, i);
    // and, finally, release tab-associated resources
    DestroyWindow(editorWinHandle);
    if (heapAllocatedFileName != NULL) {
        HeapFree(GetProcessHeap(), 0, heapAllocatedFileName);
    }

    newTabItemsCount = TabCtrl_GetItemCount(tabCtrlWinHandle);

    if (newTabItemsCount == 0) {
        createTabWithEditor(tabEditorsInfo, TRUE);
    } else {
        // if last item was removed, select previous item, otherwise select next item
        newSelectedTab = (i == newTabItemsCount) ? (i - 1) : i;
        selectTab(tabCtrlWinHandle, newSelectedTab);
    }
    
}

void moveTabWithEditor(struct TabEditorsInfo* tabEditorsInfo, int tabIndex, int newPosition) {

    HWND tabCtrlWinHandle = tabEditorsInfo->tabCtrlWinHandle;
    TCCUSTOMITEM tabCtrlItemInfo;
    wchar_t tabNameBuf[512];  // Temporary buffer for strings.
    tabCtrlItemInfo.tcitemheader.pszText = &tabNameBuf; 
    tabCtrlItemInfo.tcitemheader.cchTextMax = sizeof(tabNameBuf)/sizeof(wchar_t);

    // we want to get a copy of all the info for tab, so we need to specify all info item keys here
    tabCtrlItemInfo.tcitemheader.mask = TCIF_IMAGE | TCIF_PARAM | TCIF_RTLREADING | TCIF_STATE | TCIF_TEXT;
    // retrieve information about tab control item with tabIndex
    TabCtrl_GetItem(tabCtrlWinHandle, tabIndex, &tabCtrlItemInfo);
    // delete item on old position
    TabCtrl_DeleteItem(tabCtrlWinHandle, tabIndex);
    // insert new tab item into specified location
    TabCtrl_InsertItem(tabCtrlWinHandle, newPosition, &tabCtrlItemInfo); // content of tabControlItemInfo will be copied 
    selectTab(tabCtrlWinHandle, newPosition);

}

void selectedTabToRightmost() {
    int currentTab = TabCtrl_GetCurSel(g_tabEditorsInfo.tabCtrlWinHandle);
    int newTabItemsCount = TabCtrl_GetItemCount(g_tabEditorsInfo.tabCtrlWinHandle);
    if (currentTab < newTabItemsCount - 1) {
        moveTabWithEditor(&g_tabEditorsInfo, currentTab, newTabItemsCount - 1);
    }
}

void selectedTabToRight() {
    int currentTab = TabCtrl_GetCurSel(g_tabEditorsInfo.tabCtrlWinHandle);
    int newTabItemsCount = TabCtrl_GetItemCount(g_tabEditorsInfo.tabCtrlWinHandle);
    if (currentTab < newTabItemsCount - 1) {
        moveTabWithEditor(&g_tabEditorsInfo, currentTab, currentTab + 1);
    }
}

void selectedTabToLeftmost() {
    int currentTab = TabCtrl_GetCurSel(g_tabEditorsInfo.tabCtrlWinHandle);
    int newTabItemsCount = TabCtrl_GetItemCount(g_tabEditorsInfo.tabCtrlWinHandle);
    if (currentTab > 0) {
        moveTabWithEditor(&g_tabEditorsInfo, currentTab, 0);
    }
}

void selectedTabToLeft() {
    int currentTab = TabCtrl_GetCurSel(g_tabEditorsInfo.tabCtrlWinHandle);
    int newTabItemsCount = TabCtrl_GetItemCount(g_tabEditorsInfo.tabCtrlWinHandle);
    if (currentTab > 0) {
        moveTabWithEditor(&g_tabEditorsInfo, currentTab, currentTab - 1);
    }
}

wchar_t* getFilenameForTabItem(HWND tabCtrlWinHandle, int i) {
    TCCUSTOMITEM tabCtrlItemInfo;

    // set mask so we are interested only in app data associated with tab item
    tabCtrlItemInfo.tcitemheader.mask = TCIF_PARAM;
    // retrieve information about tab control item with index i
    TabCtrl_GetItem(tabCtrlWinHandle, i, &tabCtrlItemInfo);
    return tabCtrlItemInfo.fileName;
}

HWND getEditorForTabItem(HWND tabCtrlWinHandle, int i) {
    TCCUSTOMITEM tabCtrlItemInfo;

     // set mask so we are interested only in app data associated with tab item
     tabCtrlItemInfo.tcitemheader.mask = TCIF_PARAM;
     // retrieve information about tab control item with index i
     TabCtrl_GetItem(tabCtrlWinHandle, i, &tabCtrlItemInfo);
     return tabCtrlItemInfo.editorWindowHandle;
}

HWND getEditorForCurrentTab(struct TabEditorsInfo *tabEditorsInfo) {
    HWND tabCtrlWinHandle = tabEditorsInfo->tabCtrlWinHandle;
    getEditorForTabItem(tabCtrlWinHandle, TabCtrl_GetCurSel(tabCtrlWinHandle));
}

void selectTab(HWND tabCtrlWinHandle, int tabIndex) {
    TabCtrl_SetCurSel(tabCtrlWinHandle, tabIndex);
    showEditorForSelectedTabItem(tabCtrlWinHandle, tabIndex);
}

// Resize tab container so it fits provided RECT
// RECT is specified in parent window's client coordinates
HRESULT resizeTabControl(struct TabEditorsInfo *tabEditorsInfo, RECT rc) {
    int numTabs, i;

    HWND editorWinHandle;
    RECT editorRectangle = rc;  // initilize with total area of tab ctrl and editors
    // TCHAR debugStr[256];

    HWND tabCtrlWinHandle = tabEditorsInfo->tabCtrlWinHandle;
    TabCtrl_AdjustRect(tabCtrlWinHandle, FALSE, &editorRectangle); // values in editorRectangle are updated with dimensions of display area of tabCtrl

    // Resize the tab control
     if (!SetWindowPos(tabCtrlWinHandle, HWND_TOP, rc.left, rc.top, rc.right - rc.left, editorRectangle.top - rc.top, SWP_DEFERERASE | SWP_NOREPOSITION | SWP_NOOWNERZORDER))
        return E_FAIL;

     numTabs = TabCtrl_GetItemCount(tabCtrlWinHandle);

     for(i = 0; i < numTabs; i++) {
         editorWinHandle = getEditorForTabItem(tabCtrlWinHandle, i);
         setEditorPos(editorWinHandle, editorRectangle);
     }

    return S_OK;

}

BOOL setEditorPos(HWND editorWinHandle, RECT rc) {
    return SetWindowPos(editorWinHandle, HWND_TOP, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_DEFERERASE | SWP_NOOWNERZORDER);
}

void setFileForTabItem(HWND tabCtrlWinHandle, int tabIndex, wchar_t* heapAllocatedFileName) {
    TCCUSTOMITEM tabCtrlItemInfo;
    wchar_t buffer[512]; // should be enough
    wchar_t* filepart;

    // since we are going to update several tab properties, we request current values for all properties first

    // set mask so we are interested only in app data associated with tab item
    tabCtrlItemInfo.tcitemheader.mask = TCIF_PARAM;
    // retrieve information about tab control item with index i
    TabCtrl_GetItem(tabCtrlWinHandle, tabIndex, &tabCtrlItemInfo);

    // now we will update only those properties which we need

    // update tab name with filename without extension
    // copy to local buffer so we can change content
    StringCchCopyW(buffer, 512, heapAllocatedFileName);
    filepart = PathFindFileNameW(buffer);  // this proc doesn't allocate memory, it returns pointer to location within buffer
    // PathRemoveExtensionW(filepart);  // this proc modifies buffer
    tabCtrlItemInfo.tcitemheader.pszText = filepart; // this value will be copied

    // we will update both tab name and data
    tabCtrlItemInfo.tcitemheader.mask = TCIF_TEXT | TCIF_PARAM;

    if (tabCtrlItemInfo.fileName != NULL) {
        HeapFree(GetProcessHeap(), 0, tabCtrlItemInfo.fileName);
    }
    tabCtrlItemInfo.fileName = heapAllocatedFileName;

    TabCtrl_SetItem(tabCtrlWinHandle, tabIndex, &tabCtrlItemInfo);

    // Invoking SetItem causes tabCtrl to repaint with background color on top of editor, so we need to repaint
    InvalidateRect(tabCtrlWinHandle, NULL, TRUE);

}


LRESULT processTabNotification(HWND tabCtrlWinHandle, HMENU tabMenuHandle, HWND menuCommandProcessorWindowHandle, int code) {

    POINT cursorPos, absCursorPos;
    TCHITTESTINFO tabControlHitTestInfo;
    
    switch (code) {
            case TCN_SELCHANGING:  {
                // Return 0 to allow the selection to change.
                return 0;
            }

            case TCN_SELCHANGE: {
                showEditorForSelectedTabItem(tabCtrlWinHandle, -1); 
                return 1;
            }
            case NM_RCLICK: {
                GetCursorPos(&absCursorPos);
                cursorPos = absCursorPos;
                // since tab control is a child window itself (no self menu, no self border, ...) so it's client area corresponds to whole tab control window
                ScreenToClient(tabCtrlWinHandle, &cursorPos);
                tabControlHitTestInfo.pt = cursorPos;
                int tabIndex = TabCtrl_HitTest(tabCtrlWinHandle, &tabControlHitTestInfo);
                int numTabs = TabCtrl_GetItemCount(tabCtrlWinHandle);

                selectTab(tabCtrlWinHandle, tabIndex);

                // enabling/disabling popup menu entries depending on number of tabs and index of selected tab
                EnableMenuItem(tabMenuHandle, ID_TAB_MOVETOLEFT, !(tabIndex > 0));
                EnableMenuItem(tabMenuHandle, ID_TAB_MOVETOLEFTMOST, !(tabIndex > 0));
                EnableMenuItem(tabMenuHandle, ID_TAB_MOVETORIGHT, !(tabIndex < (numTabs - 1)));
                EnableMenuItem(tabMenuHandle, ID_TAB_MOVETORIGHTMOST, !(tabIndex < (numTabs - 1)));

                TrackPopupMenu(tabMenuHandle, TPM_RIGHTBUTTON, absCursorPos.x, absCursorPos.y, 0, menuCommandProcessorWindowHandle, NULL);

                return 1;
            }
            case TCN_GETOBJECT: {
                MessageBoxW(NULL, L"GetObject", L"Note", MB_OK);
            }
    }
    return 0;
}

void showEditorForSelectedTabItem(HWND tabCtrlWinHandle, int selected) {
    int iPage = selected < 0 ? TabCtrl_GetCurSel(tabCtrlWinHandle) : selected;
    int numTabs = TabCtrl_GetItemCount(tabCtrlWinHandle);
    int i;
    HWND editorWinHandle; 
    for (i = 0; i < numTabs; i++) {
        editorWinHandle = getEditorForTabItem(tabCtrlWinHandle, i);
        if (i == iPage) {
            ShowWindow(editorWinHandle, SW_SHOW);
            SetFocus(editorWinHandle);
        } else {
            ShowWindow(editorWinHandle, SW_HIDE);
        }
    }

}

LRESULT processEditorNotification(HWND editorWinHandle, LPNMHDR nmhdr) {
    switch (nmhdr->code) {
        case EN_SELCHANGE: {
            SELCHANGE* selchange = (SELCHANGE*)nmhdr;
        }
    }
    return 0;
}

void chooseFont(HWND topLevelWindowHandle, struct TabEditorsInfo *tabEditorsInfo) {

    HWND tabCtrlWinHandle = tabEditorsInfo->tabCtrlWinHandle;
    CHOOSEFONTW cf;
    BOOL chooseFontResult;
    HFONT newFontHandle;

    cf.lStructSize = sizeof(CHOOSEFONTW);
    cf.hwndOwner = topLevelWindowHandle;
    cf.hDC = NULL;
    cf.lpLogFont = &(tabEditorsInfo->editorFontProperties);
    cf.iPointSize = 0;
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS;
    cf.rgbColors = 0;
    cf.lCustData = 0;
    cf.lpfnHook = NULL;
    cf.lpTemplateName = NULL;
    cf.hInstance = NULL;
    cf.lpszStyle = NULL;
    cf.nFontType = 0;
    cf.nSizeMin = 0;
    cf.nSizeMax = 0;

    chooseFontResult = ChooseFontW(&cf);

    if (chooseFontResult) {
        newFontHandle = CreateFontIndirectW(&(tabEditorsInfo->editorFontProperties));
        applyFontToAllEditors(tabCtrlWinHandle, newFontHandle);
        DeleteObject(tabEditorsInfo->editorFontHandle);
        tabEditorsInfo->editorFontHandle = newFontHandle;
    }

}

void applyFontToAllEditors(HWND tabCtrlWinHandle, HFONT newFontHandle) {
    int tabItemsCount, selectedItem;
    HWND editorWindowHandle, activeEditorHandle;

    tabItemsCount = TabCtrl_GetItemCount(tabCtrlWinHandle);
    selectedItem = TabCtrl_GetCurSel(tabCtrlWinHandle);
    activeEditorHandle = NULL;
    for (int i = 0; i < tabItemsCount; i++) {
        editorWindowHandle = getEditorForTabItem(tabCtrlWinHandle, i);
        if (i == selectedItem) {
            activeEditorHandle = editorWindowHandle;
        }
        SendMessageW(editorWindowHandle, WM_SETFONT, (WPARAM)newFontHandle, 0);
    }

    if (activeEditorHandle != NULL) {
        InvalidateRect(activeEditorHandle, NULL, TRUE);
    }

}

void fileSave(HWND editorWinHandle, wchar_t* prevFileName) {
    saveToFile(editorWinHandle, prevFileName);
    SendMessageW(editorWinHandle, EM_SETMODIFY, FALSE, 0L);
}

wchar_t* fileSaveAs(HWND topLevelWindowHandle, HWND editorWinHandle, wchar_t* prevFileName) {

    OPENFILENAMEW ofn;
    wchar_t fileName[MAX_PATH] = L"untitled.txt";
    wchar_t dirName[512];
    wchar_t szFileTitle[MAX_PATH];
  
    wchar_t szFilter[256] = L"Text Files\0*.txt\0Any Files\0*.*\0";

    int heapAllocatedFileNameSize;
    wchar_t* heapAllocatedFileName;

    if (prevFileName != NULL) {
        StringCchCopyW(fileName, MAX_PATH, prevFileName);
    }

    memset(&ofn, 0, sizeof(OPENFILENAME));
    
    GetCurrentDirectoryW(sizeof(dirName), dirName);
  
    ofn.lStructSize     = sizeof(OPENFILENAME);
    ofn.hwndOwner       = topLevelWindowHandle;
    ofn.lpstrFilter     = szFilter;
    ofn.lpstrInitialDir = dirName;
    ofn.nFilterIndex    = 1;
    ofn.lpstrFile       = fileName;
    ofn.nMaxFile        = sizeof(fileName) / sizeof(wchar_t);
    ofn.lpstrFileTitle  = szFileTitle;
    ofn.nMaxFileTitle   = sizeof(szFileTitle);
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;

    if (GetSaveFileNameW(&ofn)) {
        if (*ofn.lpstrFile) {
            saveToFile(editorWinHandle, ofn.lpstrFile);
            SendMessageW(editorWinHandle, EM_SETMODIFY, FALSE, 0L);
            heapAllocatedFileNameSize = wcslen(ofn.lpstrFile) + 1;
            heapAllocatedFileName = HeapAlloc(GetProcessHeap(), 0, (heapAllocatedFileNameSize ) * sizeof(wchar_t));
            StringCchCopyW(heapAllocatedFileName, heapAllocatedFileNameSize, ofn.lpstrFile);
            return heapAllocatedFileName;
        }
    }
    return NULL;
}

wchar_t* fileOpen(HWND topLevelWindowHandle, HWND editorWindowHandle) {
    OPENFILENAME ofn;
    wchar_t fileName[MAX_PATH];
    wchar_t dirName[MAX_PATH];
    wchar_t szFileTitle[512];
    wchar_t szFilter[512] = L"Text Files\0*.txt\0Any Files\0*.*\0";

    int heapAllocatedFileNameSize;
    wchar_t* heapAllocatedFileName;

    memset(&ofn, 0, sizeof(OPENFILENAME));
    GetCurrentDirectoryW(sizeof(dirName), dirName);
    fileName[0] = L'\0';

    ofn.lStructSize     = sizeof(OPENFILENAME);
    ofn.hwndOwner       = topLevelWindowHandle;
    ofn.lpstrFilter     = szFilter;
    ofn.lpstrInitialDir = dirName;
    ofn.nFilterIndex    = 1;
    ofn.lpstrFile       = fileName;
    ofn.nMaxFile        = sizeof(fileName);
    ofn.lpstrFileTitle  = szFileTitle;
    ofn.nMaxFileTitle   = sizeof(szFileTitle);
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if(GetOpenFileNameW(&ofn))  {
        if (*ofn.lpstrFile) {
            loadFromFile(editorWindowHandle, ofn.lpstrFile);
            SendMessageW(editorWindowHandle, EM_SETMODIFY, FALSE, 0L);
            heapAllocatedFileNameSize = wcslen(ofn.lpstrFile) + 1;
            heapAllocatedFileName = HeapAlloc(GetProcessHeap(), 0, (heapAllocatedFileNameSize) * sizeof(wchar_t));
            StringCchCopyW(heapAllocatedFileName, heapAllocatedFileNameSize, ofn.lpstrFile);
            return heapAllocatedFileName;
        }
    } else {
        return NULL;
    }
    
}

HWND showFindDialog(HWND topLevelWindowHandle, wchar_t* searchStringBuffer, int searchStringBuffSize)  {
    // since we use modeless dialog, memory for this structure should not be de-allocated after exiting,
    // so we declare it static

    static FINDREPLACE fr;

    fr.lStructSize = sizeof(FINDREPLACE);
    fr.hwndOwner = topLevelWindowHandle;
    fr.hInstance = NULL;
    fr.Flags = FR_HIDEUPDOWN | FR_HIDEMATCHCASE | FR_HIDEWHOLEWORD;
    fr.lpstrFindWhat = searchStringBuffer;
    fr.lpstrReplaceWith = NULL;
    fr.wFindWhatLen = searchStringBuffSize;
    fr.wReplaceWithLen = 0;
    fr.lCustData = 0;
    fr.lpfnHook = NULL;
    fr.lpTemplateName = NULL;

    return FindText(&fr);
}

HWND showFindReplaceDialog(HWND topLevelWindowHandle, wchar_t* searchStringBuffer, int searchStringBuffSize, wchar_t* replaceStringBuffer, int replaceStringBuffSize) {
    // since we use modeless dialog, memory for this structure should not be de-allocated after exiting,
    // so we declare it static
    static FINDREPLACE fr;

    fr.lStructSize = sizeof(FINDREPLACE);
    fr.hwndOwner = topLevelWindowHandle;
    fr.hInstance = NULL;
    fr.Flags = FR_HIDEUPDOWN | FR_HIDEMATCHCASE | FR_HIDEWHOLEWORD;
    fr.lpstrFindWhat = searchStringBuffer;
    fr.lpstrReplaceWith = replaceStringBuffer;
    fr.wFindWhatLen = searchStringBuffSize;
    fr.wReplaceWithLen = replaceStringBuffSize;
    fr.lCustData = 0;
    fr.lpfnHook = NULL;
    fr.lpTemplateName = NULL;

    return ReplaceText(&fr);
}

BOOL findReplaceText(HWND editorWindowHandle, int* searchOffset, LPFINDREPLACE pfr) {
    // Find the text
    if (!findText(editorWindowHandle, searchOffset, pfr->lpstrFindWhat)) {
        return FALSE;
    }

    // Replace it
    SendMessageW(editorWindowHandle, EM_REPLACESEL, 0, (LPARAM)pfr->lpstrReplaceWith);

    return TRUE;
}

