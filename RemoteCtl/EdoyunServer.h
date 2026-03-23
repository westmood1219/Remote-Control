#pragma once
#include "EdoyunThread.h"
#include <map>
#include "CEdoyunQueue.h"
#include <MSWSock.h>

// 操作枚举
enum EdoyunOperator {
    ENone,
    EAccept,
    ERecv,
    ESend,
    EError
};

class EdoyunServer;
class EdoyunClient;
typedef std::shared_ptr<EdoyunClient> PCLIENT;

class EdoyunOverlapped {
public:
    OVERLAPPED m_overlapped;
    DWORD m_operator;//操作参见 EdoyunOperator
    std::vector<char> m_buffer;//缓冲区
    ThreadWorker m_worker;// 处理函数
    EdoyunServer* m_server;// 服务器对象
    EdoyunClient* m_client;// 对应的客户端
    WSABUF m_wsabuffer;
    virtual ~EdoyunOverlapped () {
        m_buffer.clear ();
    }
};

//前向声明模板
template<EdoyunOperator> class AcceptOverlapped;
template<EdoyunOperator> class SendOverlapped;
template<EdoyunOperator> class RecvOverlapped;
template<EdoyunOperator> class ErrorOverlapped;
// 类定义
typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;
typedef SendOverlapped<ESend> SENDOVERLAPPED;
typedef RecvOverlapped<ERecv> RECVOVERLAPPED;
typedef ErrorOverlapped<EError> ERROROVERLAPPED;



// 客户端
class EdoyunClient : public ThreadFuncBase{
public:
    EdoyunClient();
    ~EdoyunClient() {
        m_buffer.clear ();
        closesocket(m_sock);
        m_recv.reset ();
        m_send.reset ();
        m_overlapped.reset ();
        m_vecSend.Clear ();
    }

    void SetOverlapped(PCLIENT& ptr);

    operator SOCKET() {
        return m_sock;
    }

    operator PVOID() {
        return &m_buffer[0];
    }

    operator LPOVERLAPPED();

    operator LPDWORD() {
        return &m_received;
    }

    LPWSABUF RecvWSABuffer ();
    LPWSAOVERLAPPED RecvOverlapped ();
    LPWSABUF SendWSABuffer ();
    LPWSAOVERLAPPED SendOverlapped ();

    DWORD& flags () { return m_flags; }

    sockaddr_in* GetLocalAddr ( ) { return &m_laddr; }
    sockaddr_in* GetRemoteAddr ( ) { return &m_raddr; }

    size_t GetBufferSize () const { return m_buffer.size (); }
    int  Recv ();

    int Send (void* buffer, size_t nSize);
    int SendData (std::vector<char>& data);

private:
    SOCKET m_sock;
    DWORD m_received;
    DWORD m_flags;
    std::shared_ptr<ACCEPTOVERLAPPED> m_overlapped;
    std::shared_ptr<RECVOVERLAPPED> m_recv; 
    std::shared_ptr<SENDOVERLAPPED> m_send; 
    std::shared_ptr<ERROROVERLAPPED> m_error; 
    std::vector<char>m_buffer;
    size_t m_used;
    sockaddr_in m_laddr;
    sockaddr_in m_raddr;
    bool m_isbusy;
    EdoyunSendQueue<std::vector<char>> m_vecSend;//发送数据队列
};


// 建立连接accept操作
template<EdoyunOperator>
class AcceptOverlapped :public EdoyunOverlapped, ThreadFuncBase {
public:
    AcceptOverlapped ( );
    int AcceptWorker(); 
};

// 接收RECV
template<EdoyunOperator>
class RecvOverlapped :public EdoyunOverlapped, ThreadFuncBase {
public:
    RecvOverlapped ();
    int RecvWorker() {
        int ret = m_client->Recv ();
        return ret;
    }
};

// 发送
template<EdoyunOperator>
class SendOverlapped :public EdoyunOverlapped, ThreadFuncBase {
public:
    SendOverlapped ();
    int SendWorker() {
        // 与同步的直接发送不同
        // 1 send 可能不会立即完成:(几百毫秒内)几千个同时连接(每个要发生几百个字节)需要几百kB/s->几十Mb/s,除了视频服务器,一般带宽没这么大
        return -1;
    }
};

// 错误处理
template<EdoyunOperator>
class ErrorOverlapped :public EdoyunOverlapped, ThreadFuncBase {
public:
    ErrorOverlapped(): m_operator(EError), m_worker(this, &ErrorOverlapped::ErrorWorker){
        memset(&m_overlapped, 0, sizeof(m_overlapped));
        m_buffer.resize(1024);
    }
    int ErrorWorker() {
        return -1;
    }
};


// 服务类
class EdoyunServer :
    public ThreadFuncBase
{
public:
    EdoyunServer(const std::string& ip = "0.0.0.0", short port = 9327) : m_pool(10) {
        m_hIOCP = INVALID_HANDLE_VALUE;
        m_sock = INVALID_SOCKET;
        m_addr.sin_family = AF_INET;
        m_addr.sin_addr.s_addr = inet_addr(ip.c_str());
        m_addr.sin_port = htons(port);
    }
    ~EdoyunServer ();

    bool StartService ();

    bool NewAccept ();

    void BindNewSocket (SOCKET s);

private:
    void CreateSocket ();
    int threadIocp ();

private:
    EdoyunThreadPool m_pool;
    HANDLE m_hIOCP;
    SOCKET m_sock;
    sockaddr_in m_addr;
    std::map<SOCKET, std::shared_ptr<EdoyunClient>> m_client;
};
