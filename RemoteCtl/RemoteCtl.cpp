// RemoteCtl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtl.h"
#include "ServerSocket.h"
#include <direct.h>
#include <atlimage.h>//屏幕相关
#include "Command.h"
#include "MyTool.h"
#include <conio.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define INVOKE_PATH _T("C:\\Users\\wzdf\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtl.exe")
//#define INVOKE_PATH _T("C:\\Windows\\SysWOW64\\RemoteCtl.exe")
// 唯一的应用程序对象

CWinApp theApp;

using namespace std;

/*
    改bug思路
    1 观察现象
    2 确定范围
    3 分析错误的可能性
    4 调试/打日志,排查错误
    5 处理错误
    6 验证/长时间验证/多次/多条件        
*/

// 用户选择是否开机自启动
bool ChooseAutoInvoke(const CString& strPath)
{
    if (PathFileExists(strPath)) {
        return true;
    }
    CString strInfo = _T("该程序只允许用于合法用途!!\n");
    strInfo += _T("继续运行该程序,将使得这台机器处于被监控状态!\n");
    strInfo += _T("如果你不希望这样,请按\"取消\"按钮,退出程序!\n");
    strInfo += _T("按下\"是\",程序将被复制到你的机器上,并随系统启动自动运行!!\n");
    strInfo += _T("按下\"否\",程序只会运行一次,且不会留下任何东西!!\n");
    int ret = MessageBox(NULL, strInfo, _T("warning"), MB_YESNOCANCEL | MB_ICONWARNING | MB_TOPMOST);
    if (ret == IDYES) {
        //WriteRegisterTable(strPath);
        if (CMyTool::WriteStartupDir(strPath) == FALSE) {
            MessageBox(NULL, _T("复制文件失败,是否权限不足?\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
            return false;
        }
    }
    else if (ret == IDCANCEL) {
        return false;
    }
    return true;
}

enum {
    IocpListEmpty,
    IocpListPush,
    IocpListPop
};

void threadmain(HANDLE hIOCP)
{
    std::list<std::string> lstString;
    DWORD dwTransferrred = 0;
    ULONG_PTR CompletionKey = 0;
    OVERLAPPED* pOverlapped{ NULL };
    while (GetQueuedCompletionStatus(hIOCP, &dwTransferrred, &CompletionKey, &pOverlapped, INFINITE))
    {
        if (dwTransferrred == 0 && CompletionKey == NULL) {
            printf("thread is prepare to exit!\r\n");
            break;
        }
        IOCP_PARAM* pParam = (IOCP_PARAM*)CompletionKey;
        if (pParam->nOperator == IocpListPush) {
            lstString.push_back(pParam->strData);
        }
        else if (pParam->nOperator == IocpListPop) {
            std::string str;
            if (lstString.size() > 0)
            {
                str = lstString.front();
                lstString.pop_front();
            }
            if (pParam->cbFunc) {
                pParam->cbFunc(&str);
            }
        }
        else if (pParam->nOperator == IocpListEmpty)
        {
            lstString.clear();
        } 
        delete pParam;
    }
}

void threadQueueEntry(HANDLE hIOCP)
{
    threadmain(hIOCP);
    _endthread();// 代码到此为止会导致本地对象无法调用析构,导致内存泄漏
}

void func(void* arg) 
{
    std::string* pstr = (std::string*)arg;
    if (pstr != NULL) {
        printf("pop from list:%s\r\n", pstr->c_str());
    }
    else {
        printf("list is empty, no data\r\n!");
    }
}

int main()
{
    if (!CMyTool::Init()) return 1;
    printf("press any key to exit...\r\n");

    HANDLE hIOCP = INVALID_HANDLE_VALUE;// IO Completion Port
    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);    //epoll只允许单线程,完全端口映射允许多线程
    if (hIOCP == INVALID_HANDLE_VALUE || (hIOCP == NULL)) {
        printf("create iocp failed %d \r\n", GetLastError());
        return 1;
    }
    HANDLE hThread = (HANDLE)_beginthread(threadQueueEntry, 0, hIOCP);

    ULONGLONG tick = GetTickCount64();
    ULONGLONG tick0 = GetTickCount64();
    while (_kbhit() == 0)// iocp 把请求与实现分离了
    {
        if (GetTickCount64() - tick0 > 1300) {
            PostQueuedCompletionStatus(hIOCP, sizeof(IOCP_PARAM), (ULONG_PTR)new IOCP_PARAM(IocpListPop, "hello world",func), NULL);
            tick0 = GetTickCount64();
        }
        if (GetTickCount64() - tick > 2000) {
            PostQueuedCompletionStatus(hIOCP, sizeof(IOCP_PARAM), (ULONG_PTR)new IOCP_PARAM(IocpListPush, "hello world",func), NULL);
            tick = GetTickCount64();
        }
        Sleep(1);
    }
    if (hIOCP != NULL) {
        PostQueuedCompletionStatus(hIOCP, 0, NULL, NULL);
        WaitForSingleObject(hThread, INFINITE);
    }

    CloseHandle(hIOCP);

    printf("exit done!\r\n");
    exit(0);
    /*if (CMyTool::IsAdmin()) {
        if (!CMyTool::Init()) return 1;
        if (ChooseAutoInvoke(INVOKE_PATH)) {
            CCommand cmd;
            int ret = CServerSocket::getInstance()->Run(CCommand::RunCommand
                , &cmd);
            switch (ret)
            {
            case -1:
                MessageBox(NULL, _T("网络初始化异常,未能成功初始化,请检查网络状态!"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
                break;
            case -2:
                MessageBox(NULL, _T("多次无法正常接入用户"), _T("结束程序"), MB_OK | MB_ICONERROR);
                break;
            default:
                break;
            }
        }
    }
    else {
        if (CMyTool::RunAsAdmin() == false) {
            CMyTool::ShowError();
            return 1;
        }
    }*/
    return 0; 
}
