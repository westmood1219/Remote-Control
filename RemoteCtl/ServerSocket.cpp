#include "pch.h"
#include "ServerSocket.h"

//CServerSocket server;

CServerSocket* CServerSocket::m_instance = NULL;

CServerSocket::CHelper CServerSocket::m_helper;

CServerSocket* pserver = CServerSocket::getInstance();
std::string GetErrInfo(int wasErrCode)
{
    std::string ret;
    LPTSTR lpMsgBuf = NULL; // 使用 LPTSTR 自动适配字符集

    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        wasErrCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL
    );

    if (lpMsgBuf) {
#ifdef _UNICODE
        // 如果是 Unicode 项目，需要将宽字符转为多字节 string
        int size = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)lpMsgBuf, -1, NULL, 0, NULL, NULL);
        ret.resize(size);
        WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)lpMsgBuf, -1, &ret[0], size, NULL, NULL);
#else
        ret = (char*)lpMsgBuf;
#endif
        LocalFree(lpMsgBuf);
    }
    return ret;
}