// RemoteCtl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtl.h"
#include "ServerSocket.h"
#include <direct.h>
#include <atlimage.h>//屏幕相关
#include "Command.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 唯一的应用程序对象

CWinApp theApp;

using namespace std;

/*
    1 开机启动时,程序权限是跟随用户的,如果两者权限不一样,程序启动失败
    2 开机启动对环境变量有影响,如果依赖dll,则需要软链接或者直接静态库编译
*/

void WriteRegisterTable(const CString& strPath)
{
    CString strSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
    char sPath[MAX_PATH] = "";
    char sSys[MAX_PATH] = "";
    std::string strExe = "\\RemoteCtl.exe ";// 我这里是Ctl不是Ctrl!
    GetCurrentDirectoryA(MAX_PATH, sPath);
    GetSystemDirectoryA(sSys, sizeof(sSys));
    std::string strCmd = "mklink " + std::string(sSys) + strExe + std::string(sPath) + strExe;
    int ret = system(strCmd.c_str());
    TRACE("ret = %d\r\n", ret);
    HKEY hKey = NULL;
    ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strSubKey, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey);
    if (ret != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        MessageBox(NULL, _T("设置自动开机启动失败! 是否权限不足?\r\n程序启动失败! "), _T("error"), MB_ICONERROR | MB_TOPMOST);
        exit(0);
    }
    ret = RegSetValueEx(hKey, _T("RemoteCtrl"), 0, REG_EXPAND_SZ, (BYTE*)(LPCTSTR)strPath, strPath.GetLength() * sizeof(TCHAR));
    RegCloseKey(hKey);
    if (ret != ERROR_SUCCESS) {
        MessageBox(NULL, _T("设置自动开机启动失败! 是否权限不足?\r\n程序启动失败! "), _T("error"), MB_ICONERROR | MB_TOPMOST);
        exit(0);
    }
}

/*
    改bug思路
    1 观察现象
    2 确定范围
    3 分析错误的可能性
    4 调试/打日志,排查错误
    5 处理错误
    6 验证/长时间验证/多次/多条件        
*/
void WriteStartupDir(const CString& strPath)
{
    CString strCmd = GetCommandLineW();
    strCmd.Replace(_T("\""), _T(""));
    BOOL ret = CopyFile(strCmd, strPath, FALSE);
    if (ret == FALSE) {
        MessageBox(NULL, _T("复制文件失败,是否权限不足?\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
        exit(0);
    }
}

void ChooseAutoInvoke()
{
    //CString strPath = CString(_T("C:\\Windows\\SysWOW64\\RemoteCtrl.exe"));
    CString strPath("C:\\Users\\wzdf\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtl.exe");
    if (PathFileExists(strPath)) {
        return;
    }
    CString strInfo = _T("该程序只允许用于合法用途!!\n");
    strInfo += _T("继续运行该程序,将使得这台机器处于被监控状态!\n");
    strInfo += _T("如果你不希望这样,请按\"取消\"按钮,退出程序!\n");
    strInfo += _T("按下\"是\",程序将被复制到你的机器上,并随系统启动自动运行!!\n");
    strInfo += _T("按下\"否\",程序只会运行一次,且不会留下任何东西!!\n");
    int ret = MessageBox(NULL, strInfo, _T("warning"), MB_YESNOCANCEL | MB_ICONWARNING | MB_TOPMOST);
    if (ret == IDYES) {
        //WriteRegisterTable(strPath);
        WriteStartupDir(strPath);
    }
    else if (ret == IDCANCEL){
        exit(0);
    }
    return;
}

// 格式化并输出错误信息
void ShowError()
{
    LPWSTR lpMessageBuf = NULL;
    //strerror(errno);//标准c语言库
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 
        NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&lpMessageBuf, 0, NULL );
    OutputDebugString(lpMessageBuf);
    LocalFree(lpMessageBuf);
    exit(0);
}

// 检查是否有管理员权限
bool IsAdmin()
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

void RunAsAdmin()
{
    HANDLE hToken = NULL;
    BOOL ret = LogonUser(L"Administrator", NULL, NULL, LOGON32_LOGON_BATCH, LOGON32_PROVIDER_DEFAULT, &hToken);
    if (!ret) {
        ShowError();
        MessageBox(NULL, _T("登录错误"), _T("程序错误"), 0);
        exit(0);
    }
    OutputDebugString(L"Login as administrator Success!\r\n");
    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    TCHAR sPath[MAX_PATH] = _T("");
    GetCurrentDirectoryW(MAX_PATH, sPath);
    CString strCmd = sPath;
    strCmd += _T("\\RemoteCtl.exe");
    //ret = CreateProcessWithTokenW(hToken, LOGON_WITH_PROFILE, NULL, (LPWSTR)(LPCWSTR)strCmd,  CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi);
    ret = CreateProcessWithLogonW(_T("Administrator"), NULL, NULL, LOGON_WITH_PROFILE, NULL, (LPWSTR)(LPCWSTR)strCmd, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi);
    CloseHandle(hToken);
    if (!ret) {
        ShowError();
        MessageBox(NULL, _T("创建进程失败"), _T("程序错误"), 0);
        exit(0);
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

int main()
{
    int nRetCode = 0;
    HMODULE hModule = ::GetModuleHandle(nullptr);
    if (hModule != nullptr)
    {
        // 初始化 MFC 并在失败时显示错误
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: 在此处为应用程序的行为编写代码。
            wprintf(L"错误: MFC 初始化失败\n");
            nRetCode = 1;
        }
        else
        {
            if (IsAdmin()) {
                OutputDebugString(L"current is run as administrator!\r\n");
                MessageBox(NULL, _T("管理员"), _T("当前用户状态"), 0);
            }
            else {
                OutputDebugString(L"current is run as normal user!\r\n");
                //todo:获取管理员权限,使用该权限创建进程
                RunAsAdmin();
                MessageBox(NULL, _T("普通用户"), _T("当前用户状态"), 0);
                return nRetCode;
            }
            CCommand cmd; 
            ChooseAutoInvoke();
            CServerSocket* pserver = CServerSocket::getInstance();
            int ret = CServerSocket::getInstance()->Run(CCommand::RunCommand
            , &cmd);
            switch (ret)
            {
            case -1:
                MessageBox(NULL, _T("网络初始化异常,未能成功初始化,请检查网络状态!"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
                exit(0);
                break;
            case -2:
                MessageBox(NULL, _T("多次无法正常接入用户"), _T("结束程序"), MB_OK | MB_ICONERROR);
                exit(0);
                break;
            default:
                break;
            }
        }
    }
    else
    {
        // TODO: 更改错误代码以符合需要
        wprintf(L"错误: GetModuleHandle 失败\n");
        nRetCode = 1;
    }

    return nRetCode;
}
