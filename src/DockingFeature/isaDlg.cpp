#include "isaDlg.h"
#include "../PluginInterface.h"
#include "../PluginDefinition.h"
#include "../SciLexer.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <shellscalingapi.h>
#pragma comment(lib, "Shcore.lib")

extern NppData nppData;
static HWND hScintilla = NULL;

UINT GetWindowDpi(HWND hwnd)
{
    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    UINT dpiX = 96, dpiY = 96;
    GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    return dpiX;
}

HWND getCurrentScintilla()
{
    int which = -1;
    SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

std::string getCurrentFileContent()
{
    HWND mainScintilla = getCurrentScintilla();

    const LRESULT textLength = SendMessage(mainScintilla, SCI_GETTEXTLENGTH, 0, 0);

    std::string utf8Text;
    utf8Text.resize(textLength + 1);

    SendMessage(mainScintilla, SCI_GETTEXT, (WPARAM)(textLength + 1), (LPARAM)utf8Text.data());

    return utf8Text;
}

bool fileExists(const std::string& path)
{
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool directoryExists(const std::string& path)
{
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

std::string findNewestFile(const std::string& folder, const std::string& pattern)
{
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA((folder + "\\" + pattern).c_str(), &fd);

    if (hFind == INVALID_HANDLE_VALUE)
        return "";

    std::string newestFile = fd.cFileName;
    FILETIME newestTime = fd.ftLastWriteTime;

    while (FindNextFileA(hFind, &fd))
    {
        if (CompareFileTime(&fd.ftLastWriteTime, &newestTime) > 0) {
            newestTime = fd.ftLastWriteTime;
            newestFile = fd.cFileName;
        }
    }
    FindClose(hFind);
    return folder + "\\" + newestFile;
}

std::string readFileToString(const std::string& path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
    {
        return "Failed to open file: " + path;
    }

    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

std::wstring readEditText(HWND hDlg, int ID)
{
    HWND hEdit = GetDlgItem(hDlg, ID);

    int length = GetWindowTextLength(hEdit);
    if (length == 0)
    {
        return L"";
    }
    std::wstring text;
    text.resize(length + 1);
    GetWindowText(hEdit, &text[0], length + 1);
    return text;
}

bool runProcessCaptureOutputA(const std::string& cmdLine, std::string& out, DWORD timeoutMs = INFINITE, DWORD* exitCode = nullptr)
{
    out.clear();
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HANDLE hChildStdout_R = NULL;
    HANDLE hChildStdout_W = NULL;

    if (!CreatePipe(&hChildStdout_R, &hChildStdout_W, &sa, 0))
    {
        return false;
    }

    if (!SetHandleInformation(hChildStdout_R, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandle(hChildStdout_R);
        CloseHandle(hChildStdout_W);
        return false;
    }

    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hChildStdout_W;
    si.hStdError = hChildStdout_W;
    si.hStdInput = NULL;

    std::vector<char> cmdlineMutable(cmdLine.begin(), cmdLine.end());
    cmdlineMutable.push_back('\0');

    BOOL created = CreateProcessA(
        NULL,
        cmdlineMutable.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );

    CloseHandle(hChildStdout_W);
    hChildStdout_W = NULL;

    if (!created)
    {
        CloseHandle(hChildStdout_R);
        return false;
    }

    const DWORD bufSize = 4096;
    std::vector<char> buffer(bufSize);
    DWORD bytesRead = 0;
    BOOL readResult = FALSE;

    for (;;)
    {
        readResult = ReadFile(hChildStdout_R, buffer.data(), bufSize, &bytesRead, NULL);
        if (!readResult)
        {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE)
            {
                break;
            }
            else
            {
                break;
            }
        }

        if (bytesRead == 0)
        {
            break;
        }

        out.append(buffer.data(), buffer.data() + bytesRead);
    }

    DWORD waitRes = WaitForSingleObject(pi.hProcess, timeoutMs);
    if (waitRes == WAIT_TIMEOUT)
    {
        //TerminateProcess(pi.hProcess, 1);
    }

    if (exitCode)
    {
        DWORD code = 0;
        if (GetExitCodeProcess(pi.hProcess, &code))
            *exitCode = code;
        else
            *exitCode = (DWORD)-1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStdout_R);

    return true;
}

int getSelectedShader(HWND hDlg)
{
    int shader = 0;
    for (int i = IDC_RADIO1; i <= IDC_RADIO6; i++)
    {
        if (IsDlgButtonChecked(hDlg, i) == BST_CHECKED)
        {
            shader = i - IDC_RADIO1;
            break;
        }
    }
	return shader;
}

int getSelectedMode(HWND hDlg)
{
    int mode = 0;
    for (int i = IDC_RADIO_ISA; i <= IDC_RADIO_LIVEREG; i++)
    {
        if (IsDlgButtonChecked(hDlg, i) == BST_CHECKED)
        {
            mode = i - IDC_RADIO_ISA;
            break;
        }
    }
    return mode;
}

std::string shaderName(int shaderInd)
{
    switch (shaderInd)
    {
    case 0:
		return "vs";
    case 1:
		return "hs";
	case 2: 
        return "ds";
    case 3:
        return "gs";
    case 4:
        return "ps";
    case 5:
		return "cs";

    default:
        return "vs";
    }
}

std::string IsaViewerDlg::compile()
{
    saveConfig();

    std::string rdaPath = wstringToUtf8(loadFolderPath());

    if (!directoryExists(rdaPath) || !fileExists(rdaPath + "\\rga.exe"))
    {
        return "Incorrect path to the RadeonDeveloperToolSuite directory.\nPlease specify the correct directory using the \"Plugins\" menu.";
    }

    std::string source = getCurrentFileContent();

    char tempPath[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, tempPath))
    {
        return std::string("Failed to get temporary folder path");
    }

    std::string sourceFile = std::string(tempPath) + "RGAShaderSource.hlsl";
    std::string isaFile = std::string(tempPath) + "RGASgaderOut.isa";
    std::string liveregFile = std::string(tempPath) + "RGASgaderOut.livereg";

    std::string findLiveregOut = findNewestFile(std::string(tempPath), "*RGASgaderOut*.livereg");
    std::string findIsaOut = findNewestFile(std::string(tempPath), "*RGASgaderOut*.isa");

    DeleteFileA(findLiveregOut.c_str());
    DeleteFileA(findIsaOut.c_str());

    std::ofstream ofs(sourceFile, std::ios::binary);
    if (!ofs.is_open())
    {
        return std::string("Failed to open temporary file for writing.");
    }
    ofs.write(source.data(), source.size() - 1);
    ofs.close();

    std::string shaderStr = shaderName(getSelectedShader(_hSelf));
    std::string asicStr = wstringToUtf8(readEditText(_hSelf, ID_ASIC));
    std::string entryStr = wstringToUtf8(readEditText(_hSelf, ID_ENTRY_POINT));
    std::string profileStr = wstringToUtf8(readEditText(_hSelf, ID_SHADER_PROFILE));
    std::string extraStr = wstringToUtf8(readEditText(_hSelf, ID_EXTRA_ARGS));

    std::string cmd = rdaPath + "\\rga.exe"
        + " -s dx12 -c " + asicStr
        + " --" + shaderStr + " \"" + sourceFile + "\""
        + " --" + shaderStr + "-entry \"" + entryStr + "\""
        + " --all-model " + profileStr
        + " --isa \"" + isaFile + "\""
        + " --livereg \"" + liveregFile + "\""
        + " --offline "
        + extraStr
        ;

	std::string output;

    DWORD exitCode = 0;
    if (!runProcessCaptureOutputA(cmd, output, 9999, &exitCode))
    {
        return std::string("Failed compilation.");
    }

    findLiveregOut = findNewestFile(std::string(tempPath), "*RGASgaderOut*.livereg");
    findIsaOut = findNewestFile(std::string(tempPath), "*RGASgaderOut*.isa");

    if (fileExists(findLiveregOut))
    {
        std::string livereg = readFileToString(findLiveregOut);
        std::string isa = readFileToString(findIsaOut);

        DeleteFileA(findIsaOut.c_str());
        DeleteFileA(findLiveregOut.c_str());

        return getSelectedMode(_hSelf) == 0 ? isa  : livereg;
    }
    else
    {
        return output;
    }
}

#define INDIC_MATCH 20

class Accessor
{
    HWND hwnd;
public:
    Accessor(HWND h) : hwnd(h) {}

    wchar_t operator[](size_t pos) const
    {
        return (wchar_t)SendMessage(hwnd, SCI_GETCHARAT, (WPARAM)pos, 0);
    }

    size_t length() const
    {
        return (size_t)SendMessage(hwnd, SCI_GETTEXTLENGTH, 0, 0);
    }
};

void HighlightSelectionMatches(HWND scintilla)
{
    Sci_PositionU selStart = (Sci_PositionU)SendMessage(scintilla, SCI_GETSELECTIONSTART, 0, 0);
    Sci_PositionU selEnd = (Sci_PositionU)SendMessage(scintilla, SCI_GETSELECTIONEND, 0, 0);
    Sci_PositionU selLen = selEnd - selStart;

    if (selLen < 1)
    {
        SendMessage(scintilla, SCI_SETINDICATORCURRENT, INDIC_MATCH, 0);
        SendMessage(scintilla, SCI_INDICATORCLEARRANGE, 0, SendMessage(scintilla, SCI_GETTEXTLENGTH, 0, 0));
        return;
    }

    //-- Read text
    Accessor acc(scintilla);
    std::wstring selectedText(selLen, L'\0');
    for (Sci_PositionU i = 0; i < selLen; ++i)
        selectedText[i] = acc[selStart + i];

    //-- Clear old highlight
    SendMessage(scintilla, SCI_SETINDICATORCURRENT, INDIC_MATCH, 0);
    SendMessage(scintilla, SCI_INDICATORCLEARRANGE, 0, SendMessage(scintilla, SCI_GETTEXTLENGTH, 0, 0));

    //-- Search matches
    Sci_PositionU docLength = (Sci_Position)SendMessage(scintilla, SCI_GETTEXTLENGTH, 0, 0);
    Sci_PositionU startPos = 0;

    while (startPos <= docLength - selLen)
    {
        bool match = true;
        for (Sci_PositionU i = 0; i < selLen; ++i)
        {
            if (acc[startPos + i] != selectedText[i])
            {
                match = false;
                break;
            }
        }

        if (match)
        {
            //-- Highligh
            SendMessage(scintilla, SCI_INDICATORFILLRANGE, startPos, selLen);
            startPos += selLen;
        }
        else
        {
            startPos++;
        }
    }
}

INT_PTR CALLBACK IsaViewerDlg::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) 
	{
        case WM_INITDIALOG:
        {
            HINSTANCE hInst = (HINSTANCE)GetModuleHandle(NULL);

            hScintilla = CreateWindowEx(
                0,
                L"Scintilla",
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | WS_BORDER,
                0, 100, 500, 500,
                _hSelf,
                NULL,
                hInst,
                NULL
            );

            SendMessage(hScintilla, SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)"Consolas");
            SendMessage(hScintilla, SCI_STYLESETSIZE, STYLE_DEFAULT, 10);
            SendMessage(hScintilla, SCI_STYLECLEARALL, 0, 0);

            void* ilexer_ptr = (void*)SendMessage(nppData._nppHandle, NPPM_CREATELEXER, 0, (WPARAM)L"asm");
            SendMessage(hScintilla, SCI_SETILEXER, 0, (LPARAM)ilexer_ptr);
            
            SendMessage(hScintilla, SCI_STYLESETFORE, SCE_ASM_STRING, RGB(160, 32, 240));
            SendMessage(hScintilla, SCI_STYLESETFORE, SCE_ASM_COMMENT, RGB(0, 128, 0));
            SendMessage(hScintilla, SCI_STYLESETFORE, SCE_ASM_DIRECTIVE, RGB(0, 0, 255));
            SendMessage(hScintilla, SCI_STYLESETFORE, SCE_ASM_REGISTER, RGB(128, 0, 128));
            SendMessage(hScintilla, SCI_STYLESETFORE, SCE_ASM_NUMBER, RGB(255, 128, 0));

            //SendMessage(hScintilla, SCI_STYLESETFORE, SCE_ASM_OPERATOR, RGB(255, 128, 255));
            //SendMessage(hScintilla, SCI_STYLESETFORE, SCE_ASM_IDENTIFIER, RGB(255, 128, 255));
            //SendMessage(hScintilla, SCI_STYLESETFORE, SCE_ASM_CPUINSTRUCTION, RGB(255, 128, 255));

            SendMessage(hScintilla, SCI_STYLESETBOLD, SCE_ASM_CPUINSTRUCTION, TRUE);
            SendMessage(hScintilla, SCI_STYLESETFORE, SCE_ASM_CPUINSTRUCTION, RGB(80, 0, 0));

            SendMessage(hScintilla, SCI_INDICSETSTYLE, INDIC_MATCH, INDIC_ROUNDBOX);
            SendMessage(hScintilla, SCI_INDICSETFORE,  INDIC_MATCH, RGB(0, 255, 0));
            SendMessage(hScintilla, SCI_INDICSETALPHA, INDIC_MATCH, 60);
            SendMessage(hScintilla, SCI_INDICSETUNDER, INDIC_MATCH, FALSE);

            std::string keywords = "s v null m0 exec_lo vcc_lo";
            for (int i = 0; i < 256; i++)
            {
                keywords += " v" + std::to_string(i);
                keywords += " s" + std::to_string(i);
			}
			SendMessage(hScintilla, SCI_SETKEYWORDS, 0, (LPARAM)keywords.c_str());


            SendMessage(hScintilla, SCI_SETREADONLY, TRUE, 0);

            loadConfig();
            CheckRadioButton(_hSelf, IDC_RADIO_ISA, IDC_RADIO_LIVEREG, IDC_RADIO_ISA);

            return TRUE;
        }
		case WM_COMMAND : 
		{
			switch (wParam)
			{
				case IDOK :
				{
					if (hScintilla)
					{
                        std::string str = compile();
                        SendMessage(hScintilla, SCI_SETREADONLY, FALSE, 0);
                        SendMessage(hScintilla, SCI_SETTEXT, (WPARAM)(str.length() + 1), (LPARAM)str.data());
                        SendMessage(hScintilla, SCI_GOTOPOS, (WPARAM)-1, 0);
                        SendMessage(hScintilla, SCI_COLOURISE, 0, -1);
                        SendMessage(hScintilla, SCI_SETREADONLY, TRUE, 0);
					}
					return TRUE;
				}
			}
				return FALSE;
		}
        case WM_NOTIFY:
        {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->hwndFrom == hScintilla)
            {
                if (pnmh->code == SCN_UPDATEUI)
                {
                    SCNotification* scn = (SCNotification*)lParam;

                    if (scn->updated & SC_UPDATE_SELECTION)
                    {
                        HighlightSelectionMatches(hScintilla);
                    }
                }
                return TRUE;
            }
			return TRUE;
        }
		case WM_SIZE :
		{
			if (hScintilla)
			{
				RECT rc;
				GetClientRect(_hSelf, &rc);

                float scale = (float)GetWindowDpi(_hSelf) / 96.0f;

				const int upMargin = (int)(135.0f * scale);
				const int margin = 4;

				MoveWindow(
                    hScintilla,
					margin,
					upMargin + margin,
					rc.right - rc.left - margin * 2,
					rc.bottom - rc.top - margin * 2 - upMargin,
					TRUE
				);
			}
			return TRUE;
		}

		default :
			return DockingDlgInterface::run_dlgProc(message, wParam, lParam);
	}
}

std::wstring ReadIniString(const std::wstring& section,
    const std::wstring& key,
    const std::wstring& defaultValue,
    const std::wstring& filePath)
{
    DWORD bufferSize = 256;
    std::wstring buffer(bufferSize, L'\0');

    while (true)
    {
        DWORD charsRead = GetPrivateProfileStringW(
            section.c_str(),
            key.c_str(),
            defaultValue.c_str(),
            &buffer[0],
            bufferSize,
            filePath.c_str());

        if (charsRead < 1)
        {
            return defaultValue;
        }

        if (charsRead < bufferSize - 1)
        {
            buffer.resize(charsRead);
            return buffer;
        }

        bufferSize *= 2;
        buffer.resize(bufferSize);
    }
}

void IsaViewerDlg::saveConfig()
{
    int shader = getSelectedShader(_hSelf);

    std::wstring iniFile = getConfigFilePath();
    WritePrivateProfileString(L"Settings", L"ShaderProfile",    readEditText(_hSelf, ID_SHADER_PROFILE).c_str(), iniFile.c_str());
    WritePrivateProfileString(L"Settings", L"EntryPoint",       readEditText(_hSelf, ID_ENTRY_POINT).c_str(), iniFile.c_str());
    WritePrivateProfileString(L"Settings", L"ExtraArgs",        readEditText(_hSelf, ID_EXTRA_ARGS).c_str(), iniFile.c_str());
    WritePrivateProfileString(L"Settings", L"Asic",             readEditText(_hSelf, ID_ASIC).c_str(), iniFile.c_str());
    WritePrivateProfileString(L"Settings", L"Shader",           std::to_wstring(shader).c_str(), iniFile.c_str());
}

void IsaViewerDlg::loadConfig()
{
    std::wstring iniFile = getConfigFilePath();

    std::wstring shaderProfile =    ReadIniString(L"Settings", L"ShaderProfile",    L"6_0",     iniFile);
    std::wstring entryPoint =       ReadIniString(L"Settings", L"EntryPoint",       L"VS_Main", iniFile);
    std::wstring extraArgs =        ReadIniString(L"Settings", L"ExtraArgs",        L"",        iniFile);
    std::wstring asic =             ReadIniString(L"Settings", L"Asic",             L"gfx1030", iniFile);
    std::wstring shader =           ReadIniString(L"Settings", L"Shader",           L"0",       iniFile);

    CheckRadioButton(_hSelf, IDC_RADIO1, IDC_RADIO6, IDC_RADIO1 + std::stoi(shader));

    SetDlgItemText(_hSelf, ID_SHADER_PROFILE,   shaderProfile.c_str());
    SetDlgItemText(_hSelf, ID_ENTRY_POINT,      entryPoint.c_str());
    SetDlgItemText(_hSelf, ID_EXTRA_ARGS,       extraArgs.c_str());
    SetDlgItemText(_hSelf, ID_ASIC,             asic.c_str());
}
