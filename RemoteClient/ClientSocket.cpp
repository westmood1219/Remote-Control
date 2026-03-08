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

void CClientSocket::threadFunc()
{
    std::string strBuffer;
    strBuffer.resize(BUFFER_SIZE);
    char* pBuffer = (char*)strBuffer.c_str();
    int index{};
    while (m_sock != INVALID_SOCKET) {
        // 检查 发送包 队列大小
        if (m_lstSend.size() > 0) {
            TRACE("list send size: %d\r\n",m_lstSend.size());
            // 发送 队首
            CPacket& head = m_lstSend.front();
            if (Send(head) == false) {
                TRACE("send FAILED!!!");
                continue;
            }
            // 应答包 插入到哈希表 结果接收 
            auto pr = m_mapAck.insert(std::pair<HANDLE, std::list<CPacket>>(head.hEvent,std::list<CPacket>()));
            // recv接收包长
            int length = recv(m_sock, pBuffer+index, BUFFER_SIZE - index, 0);
            if (length > 0 || index > 0) {
                index += length;
                size_t size = (size_t)index;
                CPacket pack((BYTE*)pBuffer, size);
                // 收包 激活事件 追加应答包
                if (size > 0) {
                    CPacket pack((BYTE*)pBuffer, size);
                    pr.first->second.push_back(pack);
                    SetEvent(head.hEvent);
                }
            }
            else if(length <= 0 && index <= 0)
            {
                CloseSocket();// 没包断开连接 (
            }
            m_lstSend.pop_front();
        }

    }
}

bool CClientSocket::Send(const CPacket& packet) {
    if (m_sock == -1) return false;
    std::string strOut;
    packet.Data(strOut);
    return send(m_sock, strOut.c_str(), strOut.size(), 0) > 0;
}