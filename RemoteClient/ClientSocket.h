#pragma once
#include "pch.h"
#include "framework.h"
#include <string>
#include <vector>
#include <list>
#include <map>
#include <mutex>

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
    WORD nAction;// 单击, 双击, 按下, 弹起  (点击.移动.双击.弹起)
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
    CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
    // 拷贝构造
    CPacket(const CPacket& packet) {
        sHead = packet.sHead;
        nLength = packet.nLength;
        sCmd = packet.sCmd;
        strData = packet.strData;
        sSum = packet.sSum;
        hEvent = packet.hEvent;
    }
    // 赋值运算符重载
    CPacket& operator=(const CPacket& packet) {
        if (this != &packet) {
            sHead = packet.sHead;
            nLength = packet.nLength;
            sCmd = packet.sCmd;
            strData = packet.strData;
            sSum = packet.sSum;
            hEvent = packet.hEvent;
        }
        return *this;
    }
    // 打包
    CPacket(WORD nCmd, const BYTE* pData, size_t nSize, HANDLE hEvent) {
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
        this->hEvent = hEvent;
    }
    // 包解析
    CPacket(const BYTE* pData, size_t& nSize): hEvent(INVALID_HANDLE_VALUE){//nSize是寻找包头的范围
        size_t i = 0;
        for (i = 0; i < nSize; i++) {
            if (*(WORD*)(pData + i) == 0xFEFF) {
                sHead = *(WORD*)(pData + i);//找到了包头,赋值给sHead
                i += 2;//防止只有一个包头即nSize=2的情况
                break;
            }
        }
        if (i + 8 > nSize) {//DWORD是4个字节,即一个长度,一个命令,一个和校验(数据另说)
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
        if (nLength > 4) {
            strData.resize(nLength - 2 - 2);
            memcpy((void*)strData.c_str(), pData + i, nLength - 4);
            i += nLength - 4;
        }
        sSum = *(WORD*)(pData + i); i += 2;
        WORD sum = 0;
        for (size_t j = 0; j < strData.size(); j++)
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
    // 包数据的值给到传入的strOut
    const char* Data(std::string& strOut) const{
        strOut.resize(nLength + 6);//nLength不包括nlength本身和包头
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
    //std::string strOut;//整个包的数据
    HANDLE hEvent;
};
#pragma pack(pop)


#pragma warning(push)
#pragma warning(disable: 4267)// 暂时禁用 size_t 转 DWORD 的警告

// 查询网络连接错误码含义
std::string GetErrInfo(int wasErrCode);

#define BUFFER_SIZE 4096000
#define WM_SEND_PACK (WM_USER+1)// 发送包数据

class CClientSocket
{
public:
    static CClientSocket* getInstance() {
        if (m_instance == NULL) {//静态函数没有this指针,无法直接访问成员变量
            m_instance = new CClientSocket();
        }
        return m_instance;
    }

    bool InitSocket();


    int DealCommand() {
        if (m_sock == -1) return -1;
        char* buffer = m_buffer.data();// 多线程发送命令时可以出现冲突
        static size_t index = 0;
        while (true) {
            size_t len = recv(m_sock, buffer + index, BUFFER_SIZE - index, 0);//新接收了多少字节
            //Dump((BYTE*)buffer,index);
            if ((int)len <= 0 && (int)index <= 0) {
                return -1;
            }
            index += len;//这里是总长度
            len = index;//一len两用,下面的len表示总长度
            m_packet = CPacket((BYTE*)buffer, len);//改变len为使用的len的长度
            if (len > 0) {
                memmove(buffer, buffer + len, index - len);
                index -= len;//剩余的缓冲区字节数
                return m_packet.sCmd;
            }
        }
        return -1;
    }

    bool SendPacket(const CPacket& pack, std::list<CPacket>& lstPacks, bool isAutoClosed = true);

    bool GetFilePath(std::string& strPath) {
        if ((m_packet.sCmd >= 2) && (m_packet.sCmd <= 4)) {
            strPath = m_packet.strData;
            return true;
        }
        return false;
    }

    bool GetMouseEvent(const MOUSEEV& mouse) {
        if (m_packet.sCmd == 5) {
            memcpy((void*)&mouse, m_packet.strData.c_str(), sizeof(MOUSEEV));
            return true;
        }
        return false;
    }

    CPacket& GetPacket()
    {
        return m_packet;
    }

    void CloseSocket() {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }

    void UpdateAddress(int nIP, int nPort) {
        if ((m_nIP != nIP) || (m_nPort != nPort)) {

            m_nIP = nIP;
            m_nPort = nPort;
        }
    }

private:
    typedef void(CClientSocket::* MSGFUNC)(UINT nMsg, WPARAM wParam, LPARAM lParam);
    std::map<UINT, MSGFUNC> m_mapFunc;
    HANDLE m_hThread;
    std::mutex m_lock;
    bool m_bAutoCLose;
    std::list<CPacket> m_lstSend;// 发送 包列表
    std::map<HANDLE, std::list<CPacket>&> m_mapAck; // 事件映射哈希表
    std::map<HANDLE, bool> m_mapAutoClosed;
    int m_nIP;//地址
    int m_nPort;//端口
    SOCKET m_sock;// 套接字
    CPacket m_packet; // 正在处理的包
    std::vector<char> m_buffer; // recv 包的缓存区
    CClientSocket& operator=(const CClientSocket& ss) {}
    // 构造析构
    CClientSocket(const CClientSocket& ss)
    {
        m_sock = ss.m_sock;
        m_nIP = ss.m_nIP;
        m_nPort = ss.m_nPort;
        m_bAutoCLose = ss.m_bAutoCLose;
        m_hThread = INVALID_HANDLE_VALUE;
        struct
        {
            UINT message;
            MSGFUNC func;
        }funcs[] = {
            {WM_SEND_PACK,&CClientSocket::SendPack},
            {0,NULL}
        };
        for (int i = 0;funcs[i].message!=0;++i)
        {
            if (m_mapFunc.insert(std::pair<UINT, MSGFUNC>(funcs[i].message, funcs[i].func)).second == false) {
                TRACE("插入消息回调失败!!!消息:%d--函数%d--序号%d",funcs[i].message,funcs[i].func,i);
            }
        }
    }
    CClientSocket() :
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
        m_buffer.resize(BUFFER_SIZE);
        memset(m_buffer.data(), 0, BUFFER_SIZE);
    }
    ~CClientSocket() {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        WSACleanup();
    }
    // 发包新开的线程的函数
    static void threadEntry(void* arg);
    void threadFunc();
    void threadFunc2();
    // 加载windows网络功能相关库
    BOOL InitSockEnv()
    {
        WSADATA data;
        if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
            return FALSE;
        }
        return TRUE;
    }
    bool Send(const char* pData, int nSize) {
        return send(m_sock, pData, nSize, 0) > 0;
    }
    bool Send(const CPacket& packet);
    void SendPack(UINT nMsg, WPARAM wParam/*缓冲区的值*/, LPARAM lParam/*缓冲区长度*/);
    // 单例相关
    static void releaseInstance() {
        if (m_instance != NULL) {
            CClientSocket* tmp = m_instance;
            m_instance = NULL;
            delete tmp;
            //TRACE("CClientSocket has released\r\n");
        }
    }
    static CClientSocket* m_instance;


    class CHelper {
    public:
        CHelper() {
            CClientSocket::getInstance();
        }
        ~CHelper() {
            CClientSocket::releaseInstance();
        }
    };
    static CHelper m_helper;
};
#pragma warning(pop) // 恢复之前的警告设置

