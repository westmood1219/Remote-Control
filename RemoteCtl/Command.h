#pragma once
#include "resource.h"
#include <map>
#include "ServerSocket.h"
#include <io.h>
#include <list>
#include <atlimage.h>
#include <direct.h>
#include "MyTool.h"
#include "LockInfoDialog.h"

#pragma warning(disable:4966)
class CCommand
{
public:
    CCommand();
    ~CCommand() {}
    int ExcuteCommand(int nCmd);
protected:
    typedef int(CCommand::* CMDFUNC)(); // 成员函数指针
    std::map<int, CMDFUNC> m_mapFunction; // 从命令号到功能的映射
    CLockInfoDialog dlg;
    unsigned threadid;
protected:
    static unsigned _stdcall threadLockDlg(void* arg)
    {
        CCommand* thiz = (CCommand*)arg;
        thiz->threadLockDlgMain();
        _endthreadex(0);
        return 0;
    }

    void threadLockDlgMain() {
        //TRACE("%s(%d): %d\r\n", __FUNCTION__, __LINE__, GetCurrentThreadId());
        // 创建覆盖全屏dialog
        dlg.Create(IDD_DIALOG_INFO, NULL);
        dlg.ShowWindow(SW_SHOW);
        CRect rect;
        rect.left = 0;
        rect.right = GetSystemMetrics(SM_CXFULLSCREEN);
        rect.top = 0;
        rect.bottom = GetSystemMetrics(SM_CYSCREEN);
        dlg.MoveWindow(rect);
        // 文本框居中
        CWnd* pText = dlg.GetDlgItem(IDC_STATIC);
        if (pText) {
            CRect rtText;
            pText->GetWindowRect(rtText);
            int nWidth = rtText.Width();
            int x = (rect.right - nWidth) / 2;
            int nHeight = rtText.Height();
            int y = (rect.bottom - nHeight) / 2;
            pText->MoveWindow(x, y, rtText.Width(), rtText.Height());
        }
        // 窗口置顶
        dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        // 限制鼠标功能
        ShowCursor(false);
        // 隐藏任务栏
        ShowWindow(FindWindow(_T("Shell_TrayWnd"), NULL), SW_HIDE);
        // 限制鼠标活动范围
        rect.right = rect.left + 1;
        rect.top = rect.bottom + 1;
        dlg.GetWindowRect(rect);
        //限制鼠标范围
        ClipCursor(rect);
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_KEYDOWN) {
                TRACE("msg: %08X wparam: %08X lparam: %08X\r\n", msg.message, msg.wParam, msg.lParam);
                if (msg.wParam == VK_ESCAPE) {// 按下ESC退出
                    break;
                }
            }
        }
        ClipCursor(NULL);
        // 恢复鼠标
        ShowCursor(true);
        // 恢复任务栏
        ShowWindow(FindWindow(_T("Shell_TrayWnd"), NULL), SW_SHOW);
        dlg.DestroyWindow();
    }

    int MakeDriverInfo()
    {
        std::string result;
        for (int i = 1; i <= 26; i++)
        {// 1->A 2->B 3->C...
            if (_chdrive(i) == 0) {
                if (result.size() > 0) {
                    result += ',';
                }
                result += 'A' + i - 1;
            }
        }
        result += ',';
        CPacket pack(1, (BYTE*)result.c_str(), result.size()); //打包
        CMyTool::Dump((BYTE*)pack.Data(), pack.Size());
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int MakeDirectoryInfo()
    {
        std::string strPath;
        //std::list<FILEINFO> lstFileInfos;
        if (CServerSocket::getInstance()->GetFilePath(strPath) == false) {
            OutputDebugString(_T("当前命令不是获取文件列表,命令解析错误!!!"));
            return -1;
        }
        if (_chdir(strPath.c_str()) != 0) {
            FILEINFO finfo;
            finfo.HasNext = FALSE;
            CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
            CServerSocket::getInstance()->Send(pack);
            OutputDebugString(_T("没有权限访问目录!!!"));
            return -2;
        }
        _finddata_t fdata;
        intptr_t hfind = 0;
        if ((hfind = _findfirst("*", &fdata)) == -1) {
            OutputDebugString(_T("没有找到任何文件!!!"));
            FILEINFO finfo;
            finfo.HasNext = FALSE;
            CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
            CServerSocket::getInstance()->Send(pack);
            return -3;
        }
        int Count{};
        do
        {
            FILEINFO finfo;
            finfo.IsDiretory = (fdata.attrib & _A_SUBDIR) != 0;
            memcpy(finfo.szFileName, fdata.name, strlen(fdata.name));
            TRACE("[[%s]] \r\n", finfo.szFileName);
            CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
            CServerSocket::getInstance()->Send(pack);
            Count++;
        } while (!_findnext(hfind, &fdata));
        TRACE("send:  %d\r\n", Count);
        FILEINFO finfo;
        finfo.HasNext = FALSE;
        CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int RunFile()
    {
        std::string strPath;
        CServerSocket::getInstance()->GetFilePath(strPath);
        ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        CPacket pack(3, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int DownloadFile()
    {
        std::string strPath;
        CServerSocket::getInstance()->GetFilePath(strPath);
        long long data = 0;
        FILE* pFile = NULL;
        errno_t err = fopen_s(&pFile, strPath.c_str(), "rb");
        if (err != 0) {
            CPacket pack(4, NULL, 0);
            CServerSocket::getInstance()->Send(pack);
            return -1;
        }
        if (pFile != NULL) {
            fseek(pFile, 0, SEEK_END);
            data = _ftelli64(pFile);
            CPacket head(4, (BYTE*)&data, 8);
            CServerSocket::getInstance()->Send(head);
            fseek(pFile, 0, SEEK_SET);
            char buffer[1024] = "";
            size_t rlen = 0;
            do
            {
                rlen = fread(buffer, 1, 1024, pFile);
                CPacket pack(4, (BYTE*)buffer, rlen);
                CServerSocket::getInstance()->Send(pack);
            } while (rlen >= 1024);
            fclose(pFile);
        }
        CPacket pack(4, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int MouseEvent()
    {
        MOUSEEV mouse;
        if (CServerSocket::getInstance()->GetMouseEvent(mouse)) {
            SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);
            DWORD nFlags = 0;
            switch (mouse.nButton)
            {
            case 0:// 左键
                nFlags = 1;
                break;
            case 1:// 右键
                nFlags = 2;
                break;
            case 2:// 中键
                nFlags = 4;
                break;
            case 4:// 没有按键
                nFlags = 8;
                break;
            default:
                break;
            }

            // 设置坐标
            if (nFlags != 8) {
                SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);
            }
            switch (mouse.nAction)
            {
            case 0:// 单击
                nFlags |= 0x10;
                break;
            case 1:// 双击
                nFlags |= 0x20;
                break;
            case 2:// 按下
                nFlags |= 0x40;
                break;
            case 3:// 弹起/放开
                nFlags |= 0x80;
                break;
            default:
                break;
            }

            TRACE("mouse event : %08X x:%d y:%d\r\n", nFlags, mouse.ptXY.x, mouse.ptXY.y);

            // 处理组合后的逻辑状态
            switch (nFlags)
            {
            case 0x21://左键双击
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            case 0x11://左键单击
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x41://左键按下
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x81://左键放开
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x22://右键双击
                mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
                mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            case 0x12://右键单击
                mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
                mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x42://右键按下
                mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x82://右键放开
                mouse_event(MOUSEEVENTF_RIGHTUP, mouse.ptXY.x, mouse.ptXY.y, 0, GetMessageExtraInfo());
                break;
            case 0x24://中键双击
                mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
                mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            case 0x14://中键单击
                mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
                mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x44://中键按下
                mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x84://中键放开
                mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x08://单纯的鼠标移动
                mouse_event(MOUSEEVENTF_MOVE, 0, 0, 0, GetMessageExtraInfo());
                break;
            }
            // 发送鼠标状态
            CPacket pack(4, NULL, 0);
            CServerSocket::getInstance()->Send(pack);
        }
        else {
            OutputDebugString(_T("获取鼠标操作参数 失败!!!"));
            return -1;
        }
        return 0;
    }

    int SendScreen()
    {
        CImage screen;
        HDC hScreen = GetDC(NULL);
        int nBitPerPixel = GetDeviceCaps(hScreen, BITSPIXEL);// 查询颜色深度
        int nWidth = GetDeviceCaps(hScreen, HORZRES);
        int nHeight = GetDeviceCaps(hScreen, VERTRES);
        screen.Create(nWidth, nHeight, nBitPerPixel);
        BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY);
        ReleaseDC(NULL, hScreen);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);
        IStream* pStream = NULL;
        HRESULT ret = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
        if (ret == S_OK) {
            screen.Save(pStream, Gdiplus::ImageFormatPNG);
            LARGE_INTEGER bg{};
            pStream->Seek(bg, STREAM_SEEK_SET, NULL);
            PBYTE pData = (PBYTE)GlobalLock(hMem);
            SIZE_T nSize = GlobalSize(hMem);
            CPacket pack(6, pData, nSize);
            CServerSocket::getInstance()->Send(pack);
            GlobalUnlock(hMem);
        }
        pStream->Release();
        GlobalFree(hMem);
        screen.ReleaseDC();
        return 0;
        /*//DWORD tick = GetTickCount64();//测速相关
        //screen.Save(_T("test2026.png"), Gdiplus::ImageFormatPNG);// PNG速度更快
        TRACE("PNG %d\r\n", GetTickCount64() - tick);
        screen.Save(_T("test2027.jpg"), Gdiplus::ImageFormatJPEG);
        TRACE("JPG %d\r\n", GetTickCount64() - tick);*/
    }

    int LockMachine()
    {
        if ((dlg.m_hWnd == NULL) || (dlg.m_hWnd == INVALID_HANDLE_VALUE)) {
            //_beginthread(threadLockDlg, 0, NULL);
            _beginthreadex(NULL, 0, &CCommand::threadLockDlg, this, 0, &threadid);
            TRACE("threadid = %d\r\n", threadid);
        }
        CPacket pack(7, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int UnlockMachine()
    {
        PostThreadMessage(threadid, WM_KEYDOWN, VK_ESCAPE, 0);// 没有hwnd,用线程id传
        CPacket pack(8, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int DeleteLocalFile()
    {
        std::string strPath;
        CServerSocket::getInstance()->GetFilePath(strPath);
        TCHAR sPath[MAX_PATH] = _T("");
        MultiByteToWideChar(
            CP_UTF8,
            0,
            strPath.c_str(),
            -1,             // 数到 "\0"
            sPath,
            sizeof(sPath) / sizeof(TCHAR)
        );
        DeleteFile(sPath);;
        CPacket pack(9, NULL, 0);
        bool ret = CServerSocket::getInstance()->Send(pack);
        TRACE("DeleteLocalFile send ret = %d\r\n", ret);
        return 0;
    }

    int TestConnect() {
        CPacket pack(1981, NULL, 0);
        bool ret = CServerSocket::getInstance()->Send(pack);
        TRACE("TestConnect send ret = %d\r\n", ret);
        return 0;
    }

};

