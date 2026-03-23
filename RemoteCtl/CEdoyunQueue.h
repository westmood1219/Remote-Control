#pragma once
#include "pch.h"
#include <atomic>
#include <list>
#include "EdoyunThread.h"

// 线程安全的队列,利用IOCP实现
template <class T>
class CEdoyunQueue
{
public:
    enum {
        EQNone,
        EQPop,
        EQPush,
        EQSize,
        EQClear
    };

    typedef struct IocpParam {
        size_t nOperator;//操作
        T Data;//数据
        HANDLE hEvent;//pop操作需要
        IocpParam(size_t op, const T& data, HANDLE hEve = NULL) {
            nOperator = op;
            Data = data;
            hEvent = hEve;
        }
        IocpParam() {
            nOperator = EQNone;
        }
    }PPARAM;// Post Parameter 用于投递信息的结构体


public:
    CEdoyunQueue() {
        m_lock = false;
        m_hCompeletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);    //epoll只允许单线程,完全端口映射允许多线程
        m_hThread = INVALID_HANDLE_VALUE;
        if (m_hCompeletionPort != NULL) {
            m_hThread = (HANDLE)_beginthread(&CEdoyunQueue<T>::threadEntry,
                0, this);
        }
    }
    virtual ~CEdoyunQueue() {
        if (m_lock == true)return;
        m_lock = true;
        PostQueuedCompletionStatus(m_hCompeletionPort, 0, NULL, NULL);
        WaitForSingleObject(m_hThread, INFINITE);
        if (m_hCompeletionPort != NULL) {
            HANDLE temp = m_hCompeletionPort;
            m_hCompeletionPort = NULL;
            CloseHandle(temp);
        }
    }
    bool PushBack(const T& data) {
        IocpParam* pParam = new IocpParam(EQPush, data);
        if (m_lock == true) {
            delete pParam;
            return false;
        }
        BOOL ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
        if (ret == FALSE) delete pParam;
        //printf("push done %d %08p\r\n", ret, (void*)pParam);
        return ret;
    }
    virtual bool PopFront(T& data) { 
        HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        IocpParam Param(EQPop, data, hEvent);
        if (m_lock == true) {
            if (hEvent) CloseHandle(hEvent);
            return false;
        }
        BOOL ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
        if (ret == FALSE){ 
            CloseHandle(hEvent);
            return false;
        }
        ret = (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0);
        if (ret == TRUE) {
            data = Param.Data;
        }
        CloseHandle(hEvent);
        return ret;
    }
    size_t Size() {
        HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        IocpParam Param(EQSize, T(), hEvent);
        BOOL ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
        if (ret == FALSE) {
            CloseHandle(hEvent);
            return -1;
        }
        ret = (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0);
        if (ret == TRUE) {
            return Param.nOperator;
        }
        CloseHandle(hEvent);
        return -1;
    }
    bool Clear() {
        if (m_lock == true) return false;
        IocpParam* pParam = new IocpParam(EQClear, T());
        BOOL ret = PostQueuedCompletionStatus (m_hCompeletionPort , sizeof (PPARAM) , (ULONG_PTR) pParam , NULL);
        if (ret == FALSE) delete pParam;
        //printf("clear new  %08p\r\n", (void*)pParam);
        return ret;
    }
    protected:
        static void threadEntry (void* arg) {
            CEdoyunQueue<T>* thiz = (CEdoyunQueue<T>*)arg;
            thiz->threadMain ();
            _endthread ();
        }
        virtual void DealParam (PPARAM* pParam) {
            switch (pParam->nOperator) {
            case EQPush:
                m_lstData.push_back (pParam->Data);
                delete pParam;
                //printf("delete %08p\r\n", (void*)pParam);
                break;
            case EQPop:
                if (m_lstData.size () > 0) {
                    pParam->Data = m_lstData.front ();
                    m_lstData.pop_front ();
                }
                if (pParam->hEvent != NULL) SetEvent (pParam->hEvent);
                break;
            case EQSize:
                pParam->nOperator = m_lstData.size ();
                if (pParam->hEvent != NULL) {
                    SetEvent (pParam->hEvent);
                }
                break;
            case EQClear:
                m_lstData.clear ();
                delete pParam;
                break;
            default:
                OutputDebugString (_T ("Unkown operator!\r\n"));
                break;
            }
        }
public:
    virtual void threadMain () {
        DWORD dwTransferred = 0;
        PPARAM* pParam = NULL;
        ULONG_PTR CompletionKey = 0;
        OVERLAPPED* pOverlapped = NULL;
        while (GetQueuedCompletionStatus (m_hCompeletionPort , &dwTransferred , &CompletionKey , &pOverlapped , INFINITE))
        {
            if (dwTransferred == 0 && CompletionKey == NULL) {
                //printf("thread is prepare to exit!\r\n");
                break;
            }
            pParam = (PPARAM*) CompletionKey;
            DealParam (pParam);
        }
        while (GetQueuedCompletionStatus (m_hCompeletionPort , &dwTransferred , &CompletionKey , &pOverlapped , 0)) {
            if (dwTransferred == 0 && CompletionKey == NULL) {
                printf ("thread is prepare to exit!\r\n");
                continue;
            }
            pParam = (PPARAM*) CompletionKey;
            //printf("%08p\r\n", (void*)pParam);
            DealParam (pParam);
        }
        HANDLE temp = m_hCompeletionPort;
        m_hCompeletionPort = NULL;
        CloseHandle (temp);
    }
protected:
    std::list<T> m_lstData;
    HANDLE m_hCompeletionPort;
    HANDLE m_hThread;
    std::atomic<bool> m_lock;// 队列正在析构
};


template<class T>
class EdoyunSendQueue : public ThreadFuncBase , public CEdoyunQueue<T>
{
public:
    typedef int (ThreadFuncBase::* EDYCALLBACK)(T& data); 
    EdoyunSendQueue (ThreadFuncBase* obj , EDYCALLBACK callback) :CEdoyunQueue<T> () ,
        m_base (obj) ,
        m_callback (callback)
    {
        m_thread.Start ();
        m_thread.UpdateWorker(::ThreadWorker(this , (FUNCTYPE)&EdoyunSendQueue<T>::threadTick));
    }

    virtual ~EdoyunSendQueue(){
        m_base = NULL;
        m_callback = NULL;
        m_thread.Stop ();
    }
protected:

    virtual bool PopFront (T& data) { return false; }

    bool PopFront ( ) {
        typename CEdoyunQueue<T>::IocpParam* Param = new typename CEdoyunQueue<T>::IocpParam (CEdoyunQueue<T>::EQPop , T());
        if (CEdoyunQueue<T>::m_lock == true) {
            delete Param;
            return false;
        }
        BOOL ret = PostQueuedCompletionStatus (CEdoyunQueue<T>::m_hCompeletionPort , sizeof (*Param) , (ULONG_PTR) &Param , NULL);
        if (ret == FALSE) {
            delete Param;
            return false;
        }
        return ret;
    }

    int threadTick () {
        if(WaitForSingleObject (CEdoyunQueue<T>::m_hThread , 0) != WAIT_TIMEOUT) return 0;
        if (CEdoyunQueue<T>::m_lstData.size () > 0) {
            PopFront ();
        } 
        return 0;
    }

    virtual void DealParam (typename CEdoyunQueue<T>::PPARAM* pParam) {
        switch (pParam->nOperator) {
        case CEdoyunQueue<T>::EQPush:
            CEdoyunQueue<T>::m_lstData.push_back (pParam->Data);
            delete pParam; 
            //printf("delete %08p\r\n", (void*)pParam);
            break;
        case CEdoyunQueue<T>::EQPop:
            if (CEdoyunQueue<T>::m_lstData.size () > 0) {
                pParam->Data = CEdoyunQueue<T>::m_lstData.front ();
                if ((m_base->*m_callback)(pParam->Data) == 0) {
                    CEdoyunQueue<T>::m_lstData.pop_front ();
                }
            }
            delete pParam;
            break;
        case CEdoyunQueue<T>::EQSize:
            pParam->nOperator = CEdoyunQueue<T>::m_lstData.size ();
            if (pParam->hEvent != NULL) {
                SetEvent (pParam->hEvent);
            }
            break;
        case CEdoyunQueue<T>::EQClear:
            CEdoyunQueue<T>::m_lstData.clear ();
            delete pParam;
            break;
        default:
            OutputDebugString (_T ("Unkown operator!\r\n"));
            break;
        }
    }

private:
    ThreadFuncBase* m_base;
    EDYCALLBACK m_callback;
    EdoyunThread m_thread;
};

typedef EdoyunSendQueue<std::vector<char>>::EDYCALLBACK SENDCALLBACK;
