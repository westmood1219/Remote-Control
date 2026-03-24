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
#include "CEdoyunQueue.h"
#include <mswsock.h>
#include "EdoyunServer.h"

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

void test()
{
    /* 
        性能对比:CEdoyunQueue push性能高,pop性能仅push1/4
        std::list pop高,push低
    */
    CEdoyunQueue<std::string> lstStrings;
    ULONGLONG tick0 = GetTickCount64(), tick = GetTickCount64(), total = GetTickCount64();
    while (GetTickCount64() - total <= 1000) {
        //if (GetTickCount64() - tick0 >= 10) {
            lstStrings.PushBack("hello world");
            tick0 = GetTickCount64();
        //}
        //Sleep(1);
    }
    size_t count = lstStrings.Size();// 接近200000
    printf("lstStrings push done! size = %d\r\n", count);
    total = GetTickCount64();
    while (GetTickCount64() - total <= 1000) { 
        //if (GetTickCount64() - tick >= 10 ) {
            std::string str;
            lstStrings.PopFront(str);
            tick = GetTickCount64(); 
        //}
        //Sleep(1);
    }
    printf("lstStrings pop  size = %d\r\n", count - lstStrings.Size()); 
    lstStrings.Clear();
    count = 0;
    std::list <std::string>lstData;
    total = GetTickCount64();
    while (GetTickCount64() - total <= 1000) {
        lstData.push_back("hello world");// 才比CEdoyunQueue高一点?演示怎么有快1000000
    }
    printf("list push!  size = %d\r\n", count = lstData.size());
    while (GetTickCount64() - total <= 250) {
        if (lstData.size() > 0)
            {lstData.pop_front();}
    }
    printf("list pop!  size = %d\r\n", (count-lstData.size())*4);
}
/*
    1 bug测试/功能测试
    2 关键因素的测试(内存泄漏,运行稳定性,条件性)
    3 压力测试(可靠性)
    4 性能测试
*/

void iocp();

void udp_server ();
void udp_client (bool ishost = true);

int main(int argc, char* argv[])
{
    if (!CMyTool::Init()) return 1;

    if (argc == 1) {
        char  strDir[ MAX_PATH ];
        GetCurrentDirectoryA (MAX_PATH , strDir);
        STARTUPINFOA si;
        memset (&si , 0 , sizeof (si));
        PROCESS_INFORMATION pi;
        memset (&pi , 0 , sizeof (pi));
        string strCmd = argv[ 0 ];
        strCmd += " 1";
        BOOL bRet = CreateProcessA(NULL , (LPSTR)strCmd.c_str() , NULL , NULL , FALSE , 0 , NULL , strDir , &si , &pi);
        if (bRet) {
            CloseHandle (pi.hThread);
            CloseHandle (pi.hProcess);
            TRACE (_T("进程id: %d\r\n") , pi.dwProcessId);
            TRACE (_T("线程id: %d\r\n") , pi.dwThreadId);
            strCmd += " 2";
            bRet = CreateProcessA(NULL , (LPSTR) strCmd.c_str () , NULL , NULL , FALSE , 0 , NULL , strDir , &si , &pi);
            if (bRet) {
                CloseHandle (pi.hThread);
                CloseHandle (pi.hProcess);
                TRACE (_T("进程id: %d\r\n") , pi.dwProcessId);
                TRACE (_T("线程id: %d\r\n") , pi.dwThreadId);
            }
            udp_server ();// 服务器
        } 
    }else if (argc == 2)// 主客户端
    {
        udp_client ();
    }
    else {// 从客户端
        udp_client (false);
    }

    //test();
    //iocp();

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

void iocp()
{
    EdoyunServer server;
    server.StartService();
    getchar();
}

void udp_server () {
    printf("%s(%d):%s\r\n" , __FILE__ , __LINE__ , __FUNCTION__);
    getchar ();
}

void udp_client (bool ishost)
{
    if (ishost) {
        printf ("%s(%d):%s\r\n" , __FILE__ , __LINE__ , __FUNCTION__);
    }
    else {
        printf ("%s(%d):%s\r\n" , __FILE__ , __LINE__ , __FUNCTION__);
    }
}
