#pragma once
#include "pch.h"
#include "framework.h"
#include <list>
#include "Packet.h"

#pragma warning(push)
#pragma warning(disable: 4267)// 暂时禁用 size_t 转 int 的警告
// 查询网络连接错误码含义
std::string GetErrInfo(int wasErrCode);

typedef void(*SOCKET_CALLBACK)(void* arg, int status, std::list<CPacket>&, CPacket&);// arg,status,可以不写

class CServerSocket
{
public:
    static CServerSocket* getInstance() {
        if (m_instance == NULL) {//静态函数没有this指针,无法直接访问成员变量
            m_instance = new CServerSocket();
        }
        return m_instance;
    }

    int Run(SOCKET_CALLBACK callback, void* arg, short port = 9327) {
        bool ret = InitSocket(port);
        if (ret == false) return -1;
        m_callback = callback;
        m_arg = arg;
        std::list<CPacket> lstPackets;
        int count{};
        while (true)
        {
            if (AcceptClient() == false) {
                if (count >= 3) {
                    return -2;
                }
                count++;
            }
            int ret = DealCommand();
            if (ret > 0) {
                m_callback(m_arg, ret, lstPackets, m_packet);
                while (lstPackets.size() > 0) {
                    Send(lstPackets.front());
                    lstPackets.pop_front();
                }
            }
            CloseClient();
        }
        return 0;
    }

protected:
    bool InitSocket(short port) {
        if (m_sock == -1)return false;
        //TODO: 校验
        sockaddr_in serv_adr;
        memset(&serv_adr, 0, sizeof(serv_adr));
        serv_adr.sin_addr.s_addr = INADDR_ANY;
        serv_adr.sin_family = AF_INET;
        serv_adr.sin_port = htons(9327);
        //绑定
        if (bind(m_sock, (const sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {
            TRACE("连接失败: %d %s \r\n", WSAGetLastError(), GetErrInfo(WSAGetLastError()).c_str());
            return false;
        }//TODO:
        if (listen(m_sock, 5) == -1) return false;
        return true;
    }

    bool AcceptClient() {
        //TRACE("enter accept Client\r\n");
        sockaddr_in client_adr;
        //char buffer[1024];
        int cli_sz = sizeof(client_adr);
        m_client = accept(m_sock, (sockaddr*)&client_adr, &cli_sz);
        if (m_client == -1) return false;
        return true;
    }

#define BUFFER_SIZE 4096

    int DealCommand() {
        if (m_client == -1) return -1;
        char* buffer = new char[BUFFER_SIZE];
        //TRACE("new buffer \r\n");
        if (buffer == NULL) {
            TRACE("内存不足\r\n");
            return -2;
        }
        memset(buffer,0, BUFFER_SIZE);
        size_t index = 0;
        while (true) {
            size_t len = recv(m_client, buffer + index, BUFFER_SIZE - index, 0);//新接收了多少字节
            if (len <= 0) {
                delete[] buffer;
                TRACE("delete buffer 1\r\n");
                return -1;
            }
            TRACE("recv %d\r\n", len);
            index += len;//这里是总长度
            len = index;//一len两用,下面的len表示总长度
            m_packet = CPacket((BYTE*)buffer, len);//改变len为使用的len的长度
            if (len > 0) {
                memmove(buffer, buffer + len, BUFFER_SIZE - len);
                index -= len;//剩余的缓冲区字节数
                delete[] buffer;
                //TRACE("delete buffer 2\r\n");
                return m_packet.sCmd;
            }
        }
        delete[] buffer;
        TRACE("delete buffer 3\r\n");
        return -1;
    }

    bool Send(const char* pData, int nSize) {
        return send(m_client, pData, nSize, 0) > 0;
    }
    bool Send(CPacket& packet) {
        if (m_client == -1) return false;
        //Dump((BYTE*)packet.Data(), packet.Size());
        return send(m_client, packet.Data(), packet.Size(), 0) > 0;
    }

    void CloseClient() {
        if (m_client != INVALID_SOCKET) {
            closesocket(m_client);
            m_client = INVALID_SOCKET;
        }
    }

private:
    SOCKET_CALLBACK m_callback;
    void* m_arg;
    SOCKET m_sock;
    SOCKET m_client;
    CPacket m_packet;
    CServerSocket& operator=(const CServerSocket& ss) {}
    CServerSocket(const CServerSocket& ss) {
        m_sock = ss.m_sock;
        m_client = ss.m_client;
    }
    CServerSocket() {
        m_client = INVALID_SOCKET;
        if (InitSockEnv() == FALSE) {
            MessageBox(NULL, _T("无法初始化套接字环境,请检查网络设置"), _T("初始化错误!"), MB_OK | MB_ICONERROR);
            exit(0);
        }
        m_sock = socket(PF_INET, SOCK_STREAM, 0);
    }
    ~CServerSocket() {
        closesocket(m_sock);
        WSACleanup();
    }
    BOOL InitSockEnv()
    {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2,0), &data) != 0) {
            return FALSE;
        }
        return TRUE;
    }
    static void releaseInstance() {
        if (m_instance != NULL) {
            CServerSocket* tmp = m_instance;
            m_instance = NULL;
            delete tmp;
        }
    }
    static CServerSocket* m_instance;
    class CHelper {
    public:
        CHelper() {
            CServerSocket::getInstance();
        }
        ~CHelper() {
            CServerSocket::releaseInstance();
        }
    };
    static CHelper m_helper;
}; 
#pragma warning(pop) // 恢复之前的警告设置