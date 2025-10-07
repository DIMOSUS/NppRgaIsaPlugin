//this file is part of notepad++
//Copyright (C)2022 Don HO <don.h@free.fr>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "PluginDefinition.h"
#include "menuCmdID.h"
#include "DockingFeature\isaDlg.h"
#include <shobjidl.h>
#include <shlwapi.h>

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

IsaViewerDlg dlg;

HINSTANCE g_hMod;

void GetLastError_()
{
    std::wstring errorMsg(L"");

    DWORD errorCode = GetLastError();
    if (errorCode == 0)
        return; //No error message has been recorded

    LPWSTR messageBuffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, nullptr);

    errorMsg += messageBuffer;

    //Free the buffer.
    LocalFree(messageBuffer);

    ::MessageBox(NULL, errorMsg.c_str(), TEXT("In StaticDialog::create()"), MB_OK);
}

//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE hModule)
{
    LoadLibrary(TEXT("RICHED20.DLL"));

    g_hMod = (HINSTANCE)hModule;
    dlg.init(g_hMod, nppData._nppHandle);
}

void showDock()
{
    dlg.setParent(nppData._nppHandle);

    if (!dlg.isCreated())
    {
        tTbData	data = { 0 };
        dlg.create(&data, false);

        data.hClient = dlg.getHSelf();

        data.uMask = DWS_DF_CONT_RIGHT;

        data.pszModuleName = dlg.getPluginFileName();
        data.pszName = L"AMD ISA Viewer";

        // the dlgDlg should be the index of funcItem where the current function pointer is
        data.dlgID = 0;
        ::SendMessage(nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&data);

        dlg.display();
        return;
    }

    dlg.display(!dlg.isVisible());
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
}

//
// Initialization of your plugin commands
// You should fill your plugins commands here
void commandMenuInit()
{

    //--------------------------------------------//
    //-- STEP 3. CUSTOMIZE YOUR PLUGIN COMMANDS --//
    //--------------------------------------------//
    // with function :
    // setCommand(int index,                      // zero based number to indicate the order of command
    //            TCHAR *commandName,             // the command name that you want to see in plugin menu
    //            PFUNCPLUGINCMD functionPointer, // the symbol of function (function pointer) associated with this command. The body should be defined below. See Step 4.
    //            ShortcutKey *shortcut,          // optional. Define a shortcut to trigger this command
    //            bool check0nInit                // optional. Make this menu item be checked visually
    //            );

    setCommand(0, TEXT("ISA Panel"), hello, NULL, false);
    setCommand(1, TEXT("RGA Path"), setRGAPath, NULL, false);
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
}

void commandToolBar()
{
    const int MY_CMD_INDEX = 0; //-- zero command
    UINT cmdId = funcItem[MY_CMD_INDEX]._cmdID;

    HICON hIcon = (HICON)LoadImage(g_hMod, MAKEINTRESOURCE(IDB_ISA_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    HICON hIconDark = (HICON)LoadImage(g_hMod, MAKEINTRESOURCE(IDB_ISA_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    HBITMAP hBmp = (HBITMAP)LoadImage(g_hMod, MAKEINTRESOURCE(IDB_ISA_BMP), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
    toolbarIconsWithDarkMode tb = { hBmp, hIcon, hIconDark };

    ::SendMessage(nppData._nppHandle, NPPM_ADDTOOLBARICON_FORDARKMODE, (WPARAM)cmdId, (LPARAM)&tb);
}

//
// This function help you to initialize your plugin commands
//
bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit) 
{
    if (index >= nbFunc)
        return false;

    if (!pFunc)
        return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;

    return true;
}

//----------------------------------------------//
//-- STEP 4. DEFINE YOUR ASSOCIATED FUNCTIONS --//
//----------------------------------------------//
void hello()
{
    showDock();
}

std::string wstringToUtf8(const std::wstring& wstr)
{
    if (wstr.empty())
    {
        return std::string();
    }

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size_needed == 0)
    {
        return std::string();
    }

    std::string strTo(size_needed, 0);
    int result = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    if (result == 0)
    {
        return std::string();
    }

    strTo.erase(std::find_if(strTo.rbegin(), strTo.rend(),
        [](unsigned char ch) { return ch != '\0' && !std::isspace(ch); }).base(),
        strTo.end());

    return strTo;
}

std::wstring getConfigFilePath()
{
    wchar_t configDir[MAX_PATH];
    ::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)configDir);

    ::CreateDirectory(configDir, NULL);

    wchar_t iniPath[MAX_PATH];
    ::PathCombine(iniPath, configDir, L"RGAISACompiler.ini");

    return iniPath;
}

void saveFolderPath(const std::wstring& path)
{
    std::wstring iniFile = getConfigFilePath();
    WritePrivateProfileString(L"Settings", L"RGAPath", path.c_str(), iniFile.c_str());
}

std::wstring loadFolderPath()
{
    std::wstring iniFile = getConfigFilePath();

    wchar_t buffer[MAX_PATH]{ 0 };
    GetPrivateProfileString(L"Settings", L"RGAPath", L"", buffer, MAX_PATH, iniFile.c_str());
    return buffer;
}

void setRGAPath()
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));

    if (SUCCEEDED(hr))
    {
        DWORD options;
        pfd->GetOptions(&options);
        pfd->SetOptions(options | FOS_PICKFOLDERS);

        hr = pfd->Show(NULL);
        if (SUCCEEDED(hr))
        {
            IShellItem* psi;
            hr = pfd->GetResult(&psi);
            if (SUCCEEDED(hr))
            {
                PWSTR pszPath = NULL;
                psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                if (pszPath)
                {
                    saveFolderPath(pszPath);
                    CoTaskMemFree(pszPath);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }

    CoUninitialize();
}