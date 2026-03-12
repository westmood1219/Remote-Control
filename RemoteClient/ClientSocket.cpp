#include "pch.h"
#include "ClientSocket.h"

CClientSocket* CClientSocket::m_instance = NULL;

CClientSocket::CHelper CClientSocket::m_helper;

CClientSocket* pclient = CClientSocket::getInstance();

std::string GetErrInfo(int wasErrCode)
{
    std::string ret;
    LPVOID lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL,
        wasErrCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL
    );
    ret = (char*)lpMsgBuf;
    LocalFree(lpMsgBuf);
    return ret;

}

void CClientSocket::threadEntry(void* arg)
{
    CClientSocket* thiz = (CClientSocket*)arg;
    thiz->threadFunc();
}

bool CClientSocket::InitSocket()
{
    if (m_sock != INVALID_SOCKET) CloseSocket();
    m_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (m_sock == -1)return false;
    //TODO: 校验
    sockaddr_in serv_adr;
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_addr.s_addr = htonl(m_nIP);
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_port = htons(m_nPort);
    if (serv_adr.sin_addr.s_addr == INADDR_NONE) {
        AfxMessageBox(_T("指定的IP地址不存在"));
        return false;
    }
    int ret = connect(m_sock, (sockaddr*)&serv_adr, sizeof(serv_adr));
    if (ret == -1) {
        AfxMessageBox(_T("连接服务端失败"));
        TRACE("连接失败: %d %s \r\n", WSAGetLastError(), GetErrInfo(WSAGetLastError()).c_str());
    }
    return true;
}

bool CClientSocket::SendPacket(const CPacket& pack, std::list<CPacket>& lstPacks, bool isAutoClosed)
{
    // 发包的时候一定需要网络 这时候可以初始化socket并开启包处理线程
    if (m_sock == INVALID_SOCKET && m_hThread == INVALID_HANDLE_VALUE) {
        //if (InitSocket() == false) return false;
        _beginthread(&CClientSocket::threadEntry, 0, this);// 控制层线程里再开一个model层线程
    }
    m_lock.lock();
    // 应答包 插入到哈希表 pr结果接收 bool
    auto pr = m_mapAck.insert(std::pair<HANDLE, std::list<CPacket>&>(pack.hEvent, lstPacks));
    m_mapAutoClosed.insert(std::pair<HANDLE, bool>(pack.hEvent, isAutoClosed));
    // 追加 包列表 无限等待包解析处理 事件激活
    m_lstSend.push_back(pack);
    m_lock.unlock();
    WaitForSingleObject(pack.hEvent, INFINITE);

    // 找到事件对应的包后增加列表包?什么鬼 /取消对应事件应答
    std::map<HANDLE, std::list<CPacket>&>::iterator it;

    it = m_mapAck.find(pack.hEvent);
    if (it != m_mapAck.end()) {
        m_mapAck.erase(it);
        return true;
    }
    return false;
}

void CClientSocket::threadFunc()
{
    std::string strBuffer;
    strBuffer.resize(BUFFER_SIZE);
    char* pBuffer = (char*)strBuffer.c_str();
    int index{};// 表示缓冲区里最后数据的地址
    InitSocket();
    while (m_sock != INVALID_SOCKET) {
        // 检查 发送包 队列大小
        if (m_lstSend.size() > 0) {
            // 发送 队首包
            m_lock.lock();
            CPacket& head = m_lstSend.front();
            m_lock.unlock();
            if (Send(head) == false) {
                TRACE("发送失败!!!\r\n");
                continue;
            }
            // 找到事件对应的包后增加列表包 /取消对应事件应答
            std::map<HANDLE, std::list<CPacket>&>::iterator it;
            it = m_mapAck.find(head.hEvent);// size=2第一次进来有两个包?
            std::map<HANDLE, bool>::iterator it0 = m_mapAutoClosed.find(head.hEvent);
            do 
            {
                // recv接收包长
                int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0);
                if (length > 0 || index > 0) {
                    index += length;
                    size_t size = (size_t)index;
                    // 解析包 到 pack ,解析到的包长给到size
                    CPacket pack((BYTE*)pBuffer, size);
                    // 收包 激活事件 追加应答包
                    if (size > 0) {// 由于粘包,size可能会比index小
                        pack.hEvent = head.hEvent;
                        it->second.push_back(pack);
                        memmove(pBuffer, pBuffer + size, index - size);// 把用完的包的size扔掉,把这次length未解析的部分放到前面
                        index -= size;// 减去这次的包长size
                        if (it0->second) {
                            SetEvent(head.hEvent);
                            break;
                        }
                    }
                }// 缓冲区没有了,断开套接字连接
                else if (length <= 0 && index <= 0)
                {
                    CloseSocket();// 没包断开连接 退出当前收包循环
                    SetEvent(head.hEvent);// 等到服务器关闭命令之后再通知事件完成
                }
                // 无论如何pop出该包
            } while (it0->second== false);
            m_lock.lock();
            m_mapAutoClosed.erase(head.hEvent);
            m_lstSend.pop_front();
            m_lock.unlock();
            if(InitSocket() == false) InitSocket();
        }
            Sleep(1);
    }
    CloseSocket();// exit 断开连接
    TRACE("threadFunc[TID: %lu]\r\n", GetCurrentThreadId());//  线程id
}

void CClientSocket::threadFunc2()
{
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (m_mapFunc.find(msg.message) != m_mapFunc.end()) {
            (this->*m_mapFunc[msg.message])(msg.message, msg.wParam, msg.lParam);
        }
    }
}

bool CClientSocket::Send(const CPacket& packet) {
    TRACE("m_sock= %d \r\n", m_sock);
    if (m_sock == -1) return false;
    std::string strOut;
    packet.Data(strOut);
    return send(m_sock, strOut.c_str(), strOut.size(), 0) > 0;
}

void CClientSocket::SendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
{//TODO:需要定义消息/回调消息的数据结构(消息需要数据和数据长度,模式)(回调消息需要句柄HWND MESSAGE)
    if (InitSocket() == true) {
        TRACE("m_sock= %d \r\n", m_sock);
        if (m_sock == -1) return;
        int ret = send(m_sock, (char*)wParam, (int)lParam, 0);
        if (ret > 0) {

        }
        else {
            CloseSocket();
            //网络终止处理
        }
    }
    else//错误处理
    {

    }
}
