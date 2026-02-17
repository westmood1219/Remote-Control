#pragma once
#include "pch.h"
#include "framework.h"

// 包定义与解析类
class CPacket
{
public:
    // 空包初始化
    CPacket():sHead(0), nLength(0), sCmd(0), sSum(0) {}
    // 拷贝构造
    CPacket(const CPacket& packet) {
        sHead = packet.sHead;
        nLength = packet.nLength;
        sCmd = packet.sCmd;
        strData = packet.strData;
        sSum = packet.sSum;
    }
    // 赋值运算符重载
    CPacket& operator=(const CPacket& packet) {
        if (this != &packet) {
            sHead = packet.sHead;
            nLength = packet.nLength;
            sCmd = packet.sCmd;
            strData = packet.strData;
            sSum = packet.sSum;
        }
        return *this;
    }
    // 包解析
    CPacket(const BYTE* pData, size_t& nSize) {//nSize是寻找包头的范围
        size_t i = 0;
        for ( i = 0;i<nSize;i++){
            if (*(WORD*)(pData + i) == 0xFEFF) {
                sHead = *(WORD*)(pData + i);//找到了包头,赋值给sHead
                i += 2;//防止只有一个包头即nSize=2的情况
                break;
            }
        }
        if (i + 8 >= nSize) {//DWORD是4个字节,即一个长度,一个命令,一个和校验(数据另说)
            //包无法接收完全
            nSize = 0;//没使用到缓冲区,用到了0个字节
            return;
        }
        nLength = *(DWORD*)(pData + i); i += 4;
        if (nLength + i > nSize) {//缓冲区不够长,包未完全接收到
            nSize = 0;
            return;
        }
        sCmd = *(WORD*)(pData + i);     i += 2;
        if(nLength > 4) {
            strData.resize(nLength - 2 - 2);
            memcpy((void*)strData.c_str(), pData + i, nLength - 4);
            i += nLength - 4;
        }
        sSum = *(WORD*)(pData + i); i += 2;
        WORD sum = 0;
        for (size_t j = 0;j < strData.size();j++)
        {
            sum += BYTE(strData[j]) & 0xFF;
        }
        if (sum == sSum) {
            nSize = i;
            return;
        }
        nSize = 0;
    }
    ~CPacket() {}

public:
    WORD sHead;//固定位 FE FF
    DWORD nLength;//包长度 从控制命令开始,到和校验结束
    WORD sCmd;//控制命令
    std::string strData;//包数据
    WORD sSum;//和校验
};

class CServerSocket
{
public:
    static CServerSocket* getInstance() {
        if (m_instance == NULL) {//静态函数没有this指针,无法直接访问成员变量
            m_instance = new CServerSocket();
        }
        return m_instance;
    }

    bool InitSocket() {
        if (m_sock == -1)return false;
        //TODO: 校验
        sockaddr_in serv_adr;
        memset(&serv_adr, 0, sizeof(serv_adr));
        serv_adr.sin_addr.s_addr = INADDR_ANY;
        serv_adr.sin_family = AF_INET;
        serv_adr.sin_port = htons(9527);
        //绑定
        if (bind(m_sock, (sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) return false;//TODO:
        if (listen(m_sock, 5) == -1) return false;
        return true;
    }

    bool AcceptClient() {
        sockaddr_in client_adr;
        char buffer[1024];
        int cli_sz = sizeof(client_adr);
        m_client = accept(m_sock, (sockaddr*)&client_adr, &cli_sz);
        if (m_client == -1) return false;
        return true;
    }

#define BUFFER_SIZE 4096

    int DealCommand() {
        if (m_client == -1) return -1;
        char* buffer = new char[BUFFER_SIZE];
        memset(buffer,0, BUFFER_SIZE);
        size_t index = 0;
        while (true) {
            size_t len = recv(m_client, buffer + index, BUFFER_SIZE - index, 0);//新接收了多少字节
            if (len <= 0) {
                return -1;
            }
            index += len;//这里是总长度
            len = index;//一len两用,下面的len表示总长度
            m_packet = CPacket((BYTE*)buffer, len);//改变len为使用的len的长度
            if (len > 0) {
                memmove(buffer, buffer + len, BUFFER_SIZE - len);
                index -= len;//剩余的缓冲区字节数
                return m_packet.sCmd;
            }
        }
        return -1;
    }

    bool Send(const char* pData, int nSize) {
        return send(m_client, pData, nSize, 0) > 0;
    }

private:
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
        if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
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

//extern CServerSocket server;