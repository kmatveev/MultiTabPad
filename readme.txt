MultiTabPad is a minimalistic text editor with tabs. It is implemented in plain C using Win32 API. Purpose of this program is to be small (in both source code and resulting binary executable), simple (to use and to understand) yet useful. Compact size of source code allows easy understanding of internal workings, and allows to use this program as a base for more advanced editors.

It is possible to use either standard plain text editor control, or RichEdit.

How to build:

If you are using Microsoft Visual C compiler:

rc.exe multitabpad.rc
If you want to use basic plain text editor control:
cl multitabpad.c basiceditor.c apputils.c user32.lib comctl32.lib comdlg32.lib gdi32.lib shlwapi.lib advapi32.lib multitabpad.res
If you want to use RichEdit control:
cl multitabpad.c rtfeditor.c apputils.c user32.lib comctl32.lib comdlg32.lib gdi32.lib shlwapi.lib advapi32.lib multitabpad.res


If you are using Microsoft Visual Studio Community:
Create a project via File -> Create -> project from existing sources
From menu Build -> Configurations create two targets: "Debug BasicEditor" and "Debug RichTextEdit"
Select a project in solution explorer window, open project properties window (either from context menu, or from menu project->properties), then
   for both configs, go to Linker->Input, and add following additional dependencies: comctl32.lib;comdlg32.lib;shlwapi.lib
   for both configs, go to Linker->Manifest and add following additional manifest dependency: "type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'"
Select file "basiceditor.c", open properties, and for configuration "Debug RichTextEdit" exclude it from build
Select file "rtfeditor.c", open properties, and for configuration "Debug BasicEditor" exclude it from build

Things to do:
1. Support command-line parameters: try to open all files supplied from command line.
2. Text buffer memory management: there is a limit for text size in editor control, and notifications are sent when this limit is reached.
3. Support different text encodings and line endings.
4. Quick (+) "add tab" button in tab row
5. Mouse wheel support
6. Tab drag
7. Rename without saving


Future plans:

I don't want to have this program to be bloated with functionality. My plan is to develop a more advanced editor as a separate project based on this one. Any improvements in base functionality will be done here.