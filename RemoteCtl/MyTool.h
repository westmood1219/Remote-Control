#pragma once
class CMyTool
{
public:
    static void Dump(BYTE* pData, size_t nSize)
    {
        std::string strOut;
        for (size_t i = 0; i < nSize; i++) {
            char buf[8] = "0";
            if (i > 0 && (i % 16 == 0)) strOut += "\n";
            snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
            strOut += buf;
        }
        strOut += "\n";
        OutputDebugStringA(strOut.c_str());
    }

    // 检查是否有管理员权限
     static bool IsAdmin()
    {
        HANDLE hToken = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) == FALSE) {
            ShowError();
            return false;
        }
        TOKEN_ELEVATION eve;
        DWORD len = 0;
        if (GetTokenInformation(hToken, TokenElevation, &eve, sizeof(eve), &len) == false) {
            ShowError();
            return false;
        }
        CloseHandle(hToken);
        if (len == sizeof(eve)) {
            return eve.TokenIsElevated;
        }
        printf("length of tokeninformation is %d\r\n", len);
        return false;
    }

     // 获得管理权限并运行
     static bool RunAsAdmin()
     {
         //本地策略组 开启administrator账户 禁止空密码只能登录本地控制台
         STARTUPINFO si = { 0 };
         PROCESS_INFORMATION pi = { 0 };
         TCHAR sPath[MAX_PATH] = _T("");
         GetCurrentDirectoryW(MAX_PATH, sPath);
         GetModuleFileName(NULL, sPath, MAX_PATH);
         BOOL ret = CreateProcessWithLogonW(_T("Administrator"), NULL, NULL, LOGON_WITH_PROFILE, NULL, sPath, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi);
         if (!ret) {
             ShowError();// TODO:去掉调试信息
             MessageBox(NULL, _T("创建进程失败"), _T("程序错误"), 0);
             return false;
         }
         WaitForSingleObject(pi.hProcess, INFINITE);
         CloseHandle(pi.hProcess);
         CloseHandle(pi.hThread);
         return true;
     }

     // 格式化并输出错误信息
     static void ShowError()
     {
         LPWSTR lpMessageBuf = NULL;
         //strerror(errno);//标准c语言库
         FormatMessage(
             FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
             NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
             (LPWSTR)&lpMessageBuf, 0, NULL);
         OutputDebugString(lpMessageBuf);
         LocalFree(lpMessageBuf);
         exit(0);
     }

     /*
         1 开机启动时,程序权限是跟随用户的,如果两者权限不一样,程序启动失败
         2 开机启动对环境变量有影响,如果依赖dll,则需要软链接或者直接静态库编译
     */

     // 修改开机启动文件夹实现开机启动
     static BOOL WriteStartupDir(const CString& strPath)
     {
         TCHAR sPath[MAX_PATH] = _T("");
         GetModuleFileName(NULL, sPath, MAX_PATH);
         return CopyFileW(sPath, strPath, FALSE);
     }

     // 通过修改注册表实现开机启动
     static bool WriteRegisterTable(const CString& strPath)
     {
         CString strSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
         TCHAR sPath[MAX_PATH] = _T("");
         GetModuleFileName(NULL, sPath, MAX_PATH);
         BOOL ret = CopyFile(sPath, strPath, FALSE);
         if (ret == FALSE) {
             MessageBox(NULL, _T("复制文件失败,是否权限不足?\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
             return false;
         }
         HKEY hKey = NULL;
         ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strSubKey, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey);
         if (ret != ERROR_SUCCESS) {
             RegCloseKey(hKey);
             MessageBox(NULL, _T("设置自动开机启动失败! 是否权限不足?\r\n程序启动失败! "), _T("error"), MB_ICONERROR | MB_TOPMOST);
             return false;
         }
         ret = RegSetValueEx(hKey, _T("RemoteCtrl"), 0, REG_EXPAND_SZ, (BYTE*)(LPCTSTR)strPath, strPath.GetLength() * sizeof(TCHAR));
         RegCloseKey(hKey);
         if (ret != ERROR_SUCCESS) {
             MessageBox(NULL, _T("设置自动开机启动失败! 是否权限不足?\r\n程序启动失败! "), _T("error"), MB_ICONERROR | MB_TOPMOST);
             return false;
         }
         return true;
     }

     //  用于带MFC命令行初始化
     static bool Init() {
         HMODULE hModule = ::GetModuleHandle(nullptr);
         if (hModule == nullptr) {
             wprintf(L"错误: GetModuleHandle 失败\n");
             return false;
         }
         if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
         {
             // TODO: 在此处为应用程序的行为编写代码。
             wprintf(L"错误: MFC 初始化失败\n");
             return false;
         }
         return true;
     }
};

