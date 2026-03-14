#pragma once

#include "MyTool.h"
#include "RemoteClientDlg.h"
#include "ClientSocket.h"
#include "CWatchDialog.h"
#include "StatusDlg.h"
#include "map"
#include "resource.h"


//#define WM_SEND_DATA (WM_USER+2)// 发送数据
#define WM_SHOW_STATUS (WM_USER+3)// 展示状态
#define WM_SHOW_WATCH (WM_USER+4)// 远程监控
#define WM_SEND_MESSAGE (WM_USER+0x1000)// 自定义消息处理


class CClientController
{
public:
    //获取全局唯一对象
    static CClientController* getInstance();
    //初始化
    int InitController();
    // 启动
    int Invoke(CWnd*& pMainWnd);
    // 更新网络服务器地址
    void UpdateAddress(int nIP, int nPort) {
    CClientSocket::getInstance()->UpdateAddress(nIP, nPort);
    }
    // 处理命令
    int DealCommand() {
        return CClientSocket::getInstance()->DealCommand();
    }
    // 关闭套接字
    void CloseSocket() {
        CClientSocket::getInstance()->CloseSocket();
    }

    // 发命令包//1->查看磁盘分区 2->查看指定目录下的文件
    // 3->打开文件 4->下载文件 5->鼠标操作
    // 6->发送屏幕内容 7->锁 8->解锁 9->删除文件
    // 1981->测试连接 返回值是状态,true成功,false表示失败
    bool SendCommandPacket(
        HWND hWnd,// 收包后的应答窗口句柄
        int nCmd,
        bool bAutoClose = true,
        BYTE* pData = NULL,
        size_t nLength = 0,
        WPARAM wParam = 0
    );

    // 获得监控画面
    int GetImage(CImage& image) {
        return CMyTool::Bytes2Image(image, CClientSocket::getInstance()->GetPacket().strData);
    }

    // 下载文件
    int DownFile(CString strPath);
    void DownloadEnd();

    void StartWatchScreen();

protected:
    static void threadEntryForWatchData(void* arg);
    void threadWatchScreen();
    
    CClientController() :
        m_statusDlg(&m_remoteDlg),
        m_watchDlg(&m_remoteDlg)
    {
        m_isClosed = true;
        m_hThreadWatch = INVALID_HANDLE_VALUE;
        m_hThread = INVALID_HANDLE_VALUE;
        m_nThreadID = -1;
    }

    ~CClientController(){
        WaitForSingleObject(m_hThread, 100);
    }

    void threadFunc();
    static unsigned __stdcall threadEntry(void* arg);

    static void releaseInstance() {
        if (m_instance != NULL) {
            delete m_instance;
            m_instance = NULL;
            TRACE("CClientController(m_instance) has released\r\n" );
        }
    }

    LRESULT onShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam);
    LRESULT onShowWatcher(UINT nMsg, WPARAM wParam, LPARAM lParam);

private:
    // 含消息,事件句柄,返回值
    typedef struct MsgInfo{
        MSG msg;
        LRESULT result;
        MsgInfo(MSG m) {
            result = 0;
            memcpy(&msg, &m, sizeof(MSG));
        }
        MsgInfo(const MsgInfo& m) {
            result = m.result;
            memcpy(&msg, &m.msg, sizeof(MSG));
        }
        MsgInfo& operator=(const MsgInfo& m) {
            if (this != &m) {
                result = m.result;
                memcpy(&msg, &m.msg, sizeof(MSG));
            }
            return *this;
        }
    }MSGINFO;
    typedef LRESULT(CClientController::* MSGFUNC)(UINT nMsg, WPARAM wParam, LPARAM lParam);
    static std::map<UINT, MSGFUNC> m_mapFunc;
    CRemoteClientDlg m_remoteDlg;
    CWatchDialog m_watchDlg;
    CStatusDlg m_statusDlg;
    HANDLE m_hThread;
    HANDLE m_hThreadWatch;
    bool m_isClosed;	// 监视是否关闭
    // 下载文件的远程路径
    CString m_strRemote;
    // 下载文件的本地保存路径
    CString m_strLocal;
    unsigned m_nThreadID;
    static CClientController* m_instance;
    class CHelper {
    public:
        CHelper() {
            //CClientController::getInstance();
        }
        ~CHelper() {
            CClientController::releaseInstance();
        }
    };
    static CHelper m_helper;
};

