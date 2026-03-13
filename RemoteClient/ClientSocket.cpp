#include "pch.h"
#include "ClientSocket.h"

CClientSocket* CClientSocket::m_instance = NULL;

CClientSocket::CHelper CClientSocket::m_helper;

CClientSocket* pclient = CClientSocket::getInstance();
// 详细错误信息
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

unsigned CClientSocket::threadEntry(void* arg)
{
    CClientSocket* thiz = (CClientSocket*)arg;
    thiz->threadFunc2();
    _endthread();
    return 0;
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

// 开启处理控制层包命令线程
bool CClientSocket::SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed, WPARAM wParam)
{
    UINT nMode = isAutoClosed ? CSM_AUTOCLOSE : 0 ;
    std::string strOut;
    pack.Data(strOut);
    TRACE("SendPacket=====开启处理控制层包命令线程\r\n");
    bool ret = PostThreadMessage(m_nThreadID, WM_SEND_PACK, (WPARAM)new PACKET_DATA(strOut.c_str(), strOut.size(), nMode, wParam), (LPARAM)hWnd);
    TRACE("threadid : [%d]\r\n", GetCurrentThreadId());
    return ret;
}

// 分发消息给消息处理函数
void CClientSocket::threadFunc2()
{
    MSG msg;
    SetEvent(m_eventInvoke);
    TRACE("threadid : [%d]\r\n", GetCurrentThreadId());
    BOOL bRet{};
    while ((bRet = GetMessage(&msg, NULL, 0, 0))!= 0) {
        TranslateMessage(&msg);
        TRACE("GET Message :%08X\r\n", msg.message);
        DispatchMessage(&msg);
        if (m_mapFunc.find(msg.message) != m_mapFunc.end()) {
            (this->*m_mapFunc[msg.message])(msg.message, msg.wParam, msg.lParam);
        }
    }
}

// WM_SEND_PACK的消息函数
void CClientSocket::SendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
{//需要定义消息/回调消息的数据结构(消息需要数据和数据长度,模式)(回调消息需要句柄HWND MESSAGE)
    // 避免局部变量传过来后销毁,所以wParam传过来应该是堆内存,为了避免内存泄漏,所以利用RALL提前delete
    PACKET_DATA data = *(PACKET_DATA*)wParam;
    delete(PACKET_DATA*)wParam;
    HWND hWnd = (HWND)lParam;
    if (InitSocket() == true) {
        int ret = send(m_sock, (char*)data.strData.c_str(), (int)data.strData.size(), 0);
        if (ret > 0) {
            size_t  index{};
            std::string strBuffer{};
            strBuffer.resize(BUFFER_SIZE);
            char* pBuffer = (char*)strBuffer.c_str();
            while (m_sock != INVALID_SOCKET) {
                int length = recv(m_sock, pBuffer+index, BUFFER_SIZE-index, 0);
                if (length > 0 || index > 0) {
                    index += (size_t)length;// 更新buffer索引
                    size_t nLen = index;    // 避免更改索引
                    CPacket pack((BYTE*)pBuffer, nLen);// parse packet
                    if (nLen > 0) {// 如果解析到了, 给到View层处理渲染
                        SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(pack), (LPARAM)data.wParam);// 同时lparam也用来给remotedLg要处理的文件树句柄
                        if (data.nMode & CSM_AUTOCLOSE) {
                            CloseSocket();
                            return;
                        }
                    }
                    index -= nLen;
                    memmove(pBuffer, pBuffer + nLen, index);
                }
                else {// 对方关闭了套接字或网络设备异常
                    CloseSocket();
                    SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, 1);// lparam 作为返回值用来检查服务端回复情况
                }
            }
        }
        else {
            //网络终止处理
            CloseSocket();
            SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -1);
        }
    }
    else//错误处理
    {
        SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -2);
    }
}

// 默认构造   建立自定义消息映射
CClientSocket::CClientSocket() :
    m_nIP(INADDR_ANY),
    m_nPort(0),
    m_sock(INVALID_SOCKET),
    m_bAutoCLose(true),
    m_hThread(INVALID_HANDLE_VALUE)
{
    if (InitSockEnv() == FALSE) {
        MessageBox(NULL, _T("无法初始化套接字环境,请检查网络设置"), _T("初始化错误!"), MB_OK | MB_ICONERROR);
        exit(0);
    }
    m_eventInvoke = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientSocket::threadEntry, this, 0, &m_nThreadID);
    if (WaitForSingleObject(m_eventInvoke, 100) == WAIT_TIMEOUT) {
        TRACE("网络消息处理线程启动失败!");
    }
    CloseHandle(m_eventInvoke);
    m_buffer.resize(BUFFER_SIZE);
    memset(m_buffer.data(), 0, BUFFER_SIZE);
    // 建立自定义消息映射
    struct
    {
        UINT message;
        MSGFUNC func;
    }funcs[] = {
        {WM_SEND_PACK,&CClientSocket::SendPack},
        {0,NULL}
    };
    for (int i = 0; funcs[i].func != NULL; ++i) {
        m_mapFunc.insert(std::make_pair(funcs[i].message, funcs[i].func));
    }
}
// 拷贝构造
CClientSocket::CClientSocket(const CClientSocket& ss)
{
    m_sock = ss.m_sock;
    m_nIP = ss.m_nIP;
    m_nPort = ss.m_nPort;
    m_bAutoCLose = ss.m_bAutoCLose;
    m_hThread = INVALID_HANDLE_VALUE;// 为什么拷贝构造这里也初始化为非法值
    m_nThreadID = 0;
    std::map<UINT, CClientSocket::MSGFUNC>::const_iterator it = ss.m_mapFunc.begin();
    for (; it != ss.m_mapFunc.end(); ++it) {
        m_mapFunc.insert(std::pair<UINT, MSGFUNC>(it->first, it->second));
    }
}


// 弃用的发包函数
bool CClientSocket::Send(const CPacket& packet) {
    TRACE("m_sock= %d \r\n", m_sock);
    if (m_sock == -1) return false;
    std::string strOut;
    packet.Data(strOut);
    return send(m_sock, strOut.c_str(), strOut.size(), 0) > 0;
}
