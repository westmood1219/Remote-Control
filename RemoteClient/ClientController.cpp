#include "pch.h"
#include "ClientController.h"

//CClientController* CClientController::m_instance = nullptr;
std::map<UINT, CClientController::MSGFUNC> CClientController::m_mapFunc;


// 建立消息映射
CClientController* CClientController::getInstance()
{
    if (m_instance == nullptr) {
        m_instance = new CClientController();
        struct { UINT nMsg; MSGFUNC func; }MsgFuncs[] =
        {
            {WM_SEND_PACK, &CClientController::onSendPack},
            {WM_SEND_DATA, &CClientController::onSendData},
            {WM_SHOW_STATUS, &CClientController::onShowStatus},
            {WM_SHOW_WATCH, &CClientController::onShowWatcher},
            {(UINT) - 1,nullptr}
        };
        for (int i = 0; MsgFuncs[i].func != nullptr;++i) {
            m_mapFunc.insert(std::pair<UINT, MSGFUNC>(MsgFuncs[i].nMsg, MsgFuncs[i].func));
        }
    }
    return nullptr;
}

int CClientController::InitController()
{
    m_hThread = (HANDLE)_beginthreadex(NULL, 0,
        &CClientController::threadEntry,
        this, 0, &m_nThreadID);// 填入线程id,并启动线程,ex版本需要自己手动关闭线程句柄
    m_statusDlg.Create(IDD_DLG_STATUS, &m_remoteDlg);
    return 0;
}

int CClientController::Invoke(CWnd*& pMainWnd)
{
    pMainWnd = &m_remoteDlg;
    return m_remoteDlg.DoModal();
}

LRESULT CClientController::SendMessage(MSG msg)
{
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (hEvent == NULL)return -2;
    MsgInfo info(msg);
    PostThreadMessage(m_nThreadID, WM_SEND_MESSAGE, (WPARAM)&info, (LPARAM)hEvent);
    WaitForSingleObject(hEvent, -1);
    return info.result;
}

// 控制层线程函数入口
unsigned __stdcall CClientController::threadEntry(void* arg)
{
    CClientController* thiz = (CClientController*)arg;
    thiz->threadFunc();
    _endthreadex(0);
    return 0;
}

LRESULT CClientController::onSendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
    return LRESULT();
}

LRESULT CClientController::onSendData(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
    return LRESULT();
}

LRESULT CClientController::onShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
    return m_statusDlg.ShowWindow(SW_SHOW);
}

LRESULT CClientController::onShowWatcher(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
    return m_watchDlg.DoModal();
}

// 控制层线程处理消息循环
void CClientController::threadFunc()
{
    MSG msg;
    while (::GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_SEND_MESSAGE) {
            MSGINFO* pmsg = (MSGINFO*)msg.wParam;
            HANDLE hEvent = (HANDLE)msg.lParam;
            std::map<UINT, MSGFUNC>::iterator it = m_mapFunc.find(msg.message);
            if (it != m_mapFunc.end()) {
                pmsg->result = (this->*it->second)(pmsg->msg.message, pmsg->msg.wParam, pmsg->msg.lParam);
            }
            else
            {
                pmsg->result = -1;
            }
            SetEvent(hEvent);
        }
        else {
            std::map<UINT, MSGFUNC>::iterator it = m_mapFunc.find(msg.message);
            if (it != m_mapFunc.end()) {
                (this->*it->second)(msg.message, msg.wParam, msg.lParam);
            }
        }
    }
}