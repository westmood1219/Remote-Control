#pragma once
#include "pch.h"
#include "framework.h"

void Dump(BYTE* pData, size_t nSize);

typedef struct file_info {
    file_info() {
        IsInvalid = FALSE;
        IsDiretory = -1;
        HasNext = TRUE;
        memset(szFileName, 0, sizeof(szFileName));
    }
    BOOL IsInvalid;     // 是否有效
    BOOL IsDiretory;// 是否为目录 0->否
    BOOL HasNext;   // 是否有子文件 1->has
    char szFileName[256];// 文件名
} FILEINFO, * PFILEINFO;

typedef struct MouseEvent
{
    MouseEvent() {
        nAction = 0;
        nButton = -1;
        ptXY.x = 0;
        ptXY.y = 0;
    }
    WORD nAction;// 点击, 移动, 双击
    WORD nButton;// 左键, 右键, 中键
    POINT ptXY;// 坐标
}MOUSEEV, * PMOUSEEV;

#pragma pack(push)
#pragma pack(1)
#pragma warning(disable: 4267)// 暂时禁用 size_t 转 DWORD 的警告

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
    // 打包
    CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {
        sHead = 0xFEFF;
        nLength = nSize + 4;
        sCmd = nCmd;
        if (nSize > 0) {
            strData.resize(nSize);
            memcpy((void*)strData.c_str(), pData, nSize);
        }
        else {
            strData.clear();
        }
        sSum = 0;
        for (size_t j = 0; j < strData.size(); j++)
        {
            sSum += BYTE(strData[j]) & 0xFF;
        }
    }
    // 包解析
    CPacket(const BYTE* pData, size_t& nSize) {//nSize是寻找包头的范围
        size_t i = 0;
        for ( i = 0 ; i < nSize; i++){
            if (*(WORD*)(pData + i) == 0xFEFF) {
                sHead = *(WORD*)(pData + i);//找到了包头,赋值给sHead
                i += 2;//防止只有一个包头即nSize=2的情况
                break;
            }
        }
        if (i + 4 +2 +2 > nSize) {//DWORD是4个字节,即一个长度,一个命令,一个和校验(数据另说)
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
    // 包数据的大小
    int Size() {
        return nLength + 6;
    }
    // 包数据的值
    const char* Data() {
        strOut.resize(nLength + 6);
        BYTE* pData = (BYTE*)strOut.c_str();
        *(WORD*)pData = sHead; pData += 2;
        *(DWORD*)(pData) = nLength; pData += 4;
        *(WORD*)pData = sCmd; pData += 2;
        memcpy(pData, strData.c_str(), strData.size()); pData += strData.size();
        *(WORD*)pData = sSum;
        return strOut.c_str();
    }
public:
    WORD sHead;//固定位 FE FF
    DWORD nLength;//包长度 从控制命令开始,到和校验结束
    WORD sCmd;//控制命令
    std::string strData;//包数据
    WORD sSum;//和校验
    std::string strOut;//整个包的数据
};
#pragma pack(pop)

#pragma warning(push)
#pragma warning(disable: 4267)// 暂时禁用 size_t 转 DWORD 的警告


// 查询网络连接错误码含义
std::string GetErrInfo(int wasErrCode);

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
        TRACE("enter accept Client\r\n");
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
        TRACE("new buffer \r\n");
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
                TRACE("delete buffer 2\r\n");
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
        Dump((BYTE*)packet.Data(), packet.Size());
        return send(m_client, packet.Data(), packet.Size(), 0) > 0;
    }

    bool GetFilePath(std::string& strPath) {
        if (((m_packet.sCmd >= 2) && (m_packet.sCmd <= 4)) || m_packet.sCmd == 9) {
            strPath = m_packet.strData;
            return true;
        }
        return false;
    }

    bool GetMouseEvent(const MOUSEEV& mouse) {
        if (m_packet.sCmd == 5) {
            memcpy((void*) & mouse, m_packet.strData.c_str(), sizeof(MOUSEEV));
            return true;
        }
        return false;
    }

    CPacket& GetPacket()
    {
        return m_packet;
    }

    void CloseClient() {
        closesocket(m_client);
        m_client = INVALID_SOCKET;
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
#pragma warning(pop) // 恢复之前的警告设置