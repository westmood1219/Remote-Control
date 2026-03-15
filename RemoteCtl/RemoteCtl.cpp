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

bool ChooseAutoInvoke(const CString& strPath)
{
    if (PathFileExists(strPath)) {
        return false;
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




int main()
{
    if (CMyTool::IsAdmin()) {
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
    }
    return 0; 
}
