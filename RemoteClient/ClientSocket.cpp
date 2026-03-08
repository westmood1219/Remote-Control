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
    if (InitSocket() == false) {
        return;
    }
    std::string strBuffer;
    strBuffer.resize(BUFFER_SIZE);
    char* pBuffer = (char*)strBuffer.c_str();
    int index{};
    // connect success
    while (m_sock != INVALID_SOCKET) {
        // send deque has value
        if (m_lstSend.size() > 0) {
            CPacket& head = m_lstSend.front();
            if (Send(head) == false) {
                TRACE("send FAILED!!!");
                continue;
            }
            auto pr = m_mapAck.insert(std::pair<HANDLE, std::list<CPacket>>(head.hEvent,std::list<CPacket>()));
            int length = recv(m_sock, pBuffer+index, BUFFER_SIZE - index, 0);
            if (length > 0 || index > 0) {
                index += length;
                size_t size = (size_t)index;
                if (size > 0) {
                    CPacket pack((BYTE*)pBuffer, size);
                    pr.first->second.push_back(pack);
                    SetEvent(head.hEvent);
                }
                continue;
            }
            else
            {
                CloseSocket();
            }
            m_lstSend.pop_front();
        }

    }
}