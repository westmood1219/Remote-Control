
// RemoteClientDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "RemoteClient.h"
#include "RemoteClientDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#include "CWatchDialog.h"


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
public:
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)

END_MESSAGE_MAP()


// CRemoteClientDlg 对话框



CRemoteClientDlg::CRemoteClientDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_REMOTECLIENT_DIALOG, pParent)
	, m_server_address(0)
	, m_nPort(_T(""))
	, m_isFull(false)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CRemoteClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_IPAddress(pDX, IDC_IPADDRESS_SERV, m_server_address);
	DDX_Text(pDX, IDC_EDIT_PORT, m_nPort);
	DDX_Control(pDX, IDC_TREE_DIR, m_Tree);
	DDX_Control(pDX, IDC_LIST_FILE, m_List);
}

int CRemoteClientDlg::SendCommandPacket(int nCmd, bool bAutoClose, BYTE* pData, size_t nLength)
{
    UpdateData();
    CClientSocket* pClient = CClientSocket::getInstance();
    bool ret = pClient->InitSocket(m_server_address, atoi((LPCTSTR)m_nPort));
    if (!ret) {
        AfxMessageBox(_T("网络初始化失败"));
        return -1;
    }
    CPacket pack(nCmd, pData, nLength);
    ret = pClient->Send(pack);
    TRACE("Send ret : %d\r\n", ret);
    int cmd = pClient->DealCommand();
    //TRACE("ack: %d\r\n", cmd);
	if (bAutoClose) {
		pClient->CloseSocket();
	}
	return cmd;
}

BEGIN_MESSAGE_MAP(CRemoteClientDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_TEST, &CRemoteClientDlg::OnBnClickedBtnTest)
	ON_BN_CLICKED(IDC_BTN_FILEINFO, &CRemoteClientDlg::OnBnClickedBtnFileinfo)
	ON_NOTIFY(NM_DBLCLK, IDC_TREE_DIR, &CRemoteClientDlg::OnNMDblclkTreeDir)
	ON_NOTIFY(NM_CLICK, IDC_TREE_DIR, &CRemoteClientDlg::OnNMClickTreeDir)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_FILE, &CRemoteClientDlg::OnNMRClickListFile)
	ON_COMMAND(ID_DOWNLOAD_FILE, &CRemoteClientDlg::OnDownloadFile)
	ON_COMMAND(ID_DELETE_FILE, &CRemoteClientDlg::OnDeleteFile)
	ON_COMMAND(ID_RUN_FILE, &CRemoteClientDlg::OnRunFile)
	ON_MESSAGE(WM_SEND_PACKET, &CRemoteClientDlg::onSendPacket)//消息映射表添加项==注册消息③
	ON_BN_CLICKED(IDC_BTN_START_WATCH, &CRemoteClientDlg::OnBnClickedBtnStartWatch)
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CRemoteClientDlg 消息处理程序

BOOL CRemoteClientDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	UpdateData();
	m_server_address = 0x7F000001;
	m_nPort = _T("9327");
	UpdateData(FALSE);
	m_dlgStatus.Create(IDD_DLG_STATUS, this);
	m_dlgStatus.ShowWindow(SW_HIDE);
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CRemoteClientDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CRemoteClientDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CRemoteClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CRemoteClientDlg::OnBnClickedBtnTest()
{
	SendCommandPacket(1981);
}
void CRemoteClientDlg::OnBnClickedBtnFileinfo()
{
	int ret = SendCommandPacket(1);
	if (ret == -1) {
		AfxMessageBox(_T("命令处理失败!!!"));
		return;
	}
    CClientSocket* pClient = CClientSocket::getInstance();
	std::string drivers = pClient->GetPacket().strData;
	std::string dr;
	m_Tree.DeleteAllItems();
	for (size_t i = 0;i<drivers.size();i++)
	{
		if (drivers[i] == ',') {
			dr += ":";
			HTREEITEM hTemp = m_Tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);
			m_Tree.InsertItem(_T(""), hTemp, TVI_LAST);
            dr.clear();
			continue;
        }
        dr += drivers[i];
	}
}

void CRemoteClientDlg::threadEntryForWatchData(void* arg)
{
}

void CRemoteClientDlg::threadWatchData()
{
	CClientSocket* pClient = NULL;
	do {
		pClient = CClientSocket::getInstance();
	} while (pClient == NULL);
	for (;;) {
		CPacket pack(6, NULL, 0);
		bool ret = pClient->Send(pack);// 申请监控画面
		if (ret) {
			int cmd = pClient->DealCommand();// 拿到数据
            if (cmd == 6) {
                if (m_isFull == false) { // 更新数据到缓存
                    BYTE* pData = (BYTE*)pClient->GetPacket().strData.c_str();
					HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);
					if (hMem == NULL) {
						TRACE("内存不足了!");
						Sleep(1);// 防止过度消耗cpu资源和带宽
						continue;
					}
					IStream* pStream = NULL;
					HRESULT hRet = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
					if (hRet == S_OK) {
						ULONG length = 0;
						pStream->Write(pData, pClient->GetPacket().strData.size(),&length);
						LARGE_INTEGER bg{};
						pStream->Seek(bg, STREAM_SEEK_SET, NULL);
						m_image.Load(pStream);
                        m_isFull = true;
					}
				}
            }
		}
		else {
			Sleep(1);// 防止send时网络断开导致吃满cpu
		}
	}
}

void CRemoteClientDlg::threadEntryForDownFile(void* arg)
{
	CRemoteClientDlg* thiz = (CRemoteClientDlg*)arg;
	thiz->threadDownFile();
	_endthread();
}

void CRemoteClientDlg::threadDownFile()
{
	int nListSelected = m_List.GetSelectionMark();
	CString strFile = m_List.GetItemText(nListSelected, 0);

	CFileDialog dlg(false, NULL,
		strFile, OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
		NULL, this);
	if (dlg.DoModal() == IDOK) {
		FILE* pFile = fopen(dlg.GetPathName(), "wb+");
		if (pFile == nullptr) {
            AfxMessageBox(_T("没有权限保存该文件/文件无法创建"));
            EndWaitCursor();
            m_dlgStatus.ShowWindow(SW_HIDE);
			return;
		}
		HTREEITEM hSelected = m_Tree.GetSelectedItem();
		strFile = GetPath(hSelected) + strFile;
		TRACE("%s\r\n", LPCSTR(strFile));
		CClientSocket* pClient = CClientSocket::getInstance();
		do 
        {
			int ret = SendMessage(WM_SEND_PACKET, 4 << 1 | 0, (LPARAM)(LPCSTR)strFile);
            if (ret < 0) {
                AfxMessageBox("执行下载命令失败!!");
                TRACE("执行下载命令失败:ret = %d \r\n", ret);
                break;
            }
            long long nLength = *(long long*)CClientSocket::getInstance()->GetPacket().strData.c_str();
            if (nLength == 0) {
                AfxMessageBox("文件长度为0或者无法读取文件!!");
				break;
            }
            long long nCount = 0;
            while (nCount < nLength) {
                ret = pClient->DealCommand();
                if (ret < 0) {
                    AfxMessageBox("传输失败!!");
                    TRACE("传输失败:ret = %d \r\n", ret);
                    break;
                }
                fwrite(pClient->GetPacket().strData.c_str(), 1, pClient->GetPacket().strData.size(), pFile);
                nCount += pClient->GetPacket().strData.size();
            }
		} while (false);
		fclose(pFile);
		pClient->CloseSocket();
	}
	m_dlgStatus.ShowWindow(SW_HIDE);
	EndWaitCursor();
	MessageBox(_T("操作成功!"));
}

// 刷新文件
void CRemoteClientDlg::LoadFileCurrent()
{
	HTREEITEM hTree = m_Tree.GetSelectedItem();
	CString strPath = GetPath(hTree);
    m_List.DeleteAllItems();
    int cmd = SendCommandPacket(2, false, (BYTE*)(LPCSTR)strPath, strPath.GetLength());
    PFILEINFO pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
    CClientSocket* pClient = CClientSocket::getInstance();
    while (pInfo->HasNext == TRUE)
    {
        TRACE("[%s] isdir %d\r\n", pInfo->szFileName, pInfo->IsDiretory);
        if (!pInfo->IsDiretory) {
            m_List.InsertItem(0, pInfo->szFileName);
        }
        int cmd = pClient->DealCommand();
        TRACE("ack:%d \r\n", cmd);
        if (cmd < 0) {
            break;
        }
        pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
    }
    pClient->CloseSocket();
}

void CRemoteClientDlg::LoadFileInfo()
{
	CPoint ptMouse;
	GetCursorPos(&ptMouse);
	m_Tree.ScreenToClient(&ptMouse);
	HTREEITEM hTreeSelected = m_Tree.HitTest(ptMouse, 0);
	if (hTreeSelected == NULL) return;
	if (m_Tree.GetChildItem(hTreeSelected) == NULL) return;
	DeleteTreeChildrenItem(hTreeSelected);

	m_List.DeleteAllItems();
	CString strPath = GetPath(hTreeSelected);
	int cmd = SendCommandPacket(2, false, (BYTE*)(LPCSTR)strPath, strPath.GetLength());
	PFILEINFO pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
	CClientSocket* pClient = CClientSocket::getInstance();
	int Count{};
	while (pInfo->HasNext == TRUE)
	{
		TRACE("[%s] isdir %d\r\n", pInfo->szFileName, pInfo->IsDiretory);
		if (pInfo->IsDiretory) {
			if (CString(pInfo->szFileName) == "." || CString(pInfo->szFileName) == "..") {
				int cmd = pClient->DealCommand();
				TRACE("ack:%d \r\n", cmd);
				if (cmd < 0) {
					break;
				}
				pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
				continue;
			}
			HTREEITEM hTemp = m_Tree.InsertItem(pInfo->szFileName, hTreeSelected, TVI_LAST);
			m_Tree.InsertItem(_T(""), hTemp, TVI_LAST);
		}
		else {
			m_List.InsertItem(0, pInfo->szFileName);
		}
		int cmd = pClient->DealCommand();
		//TRACE("ack:%d \r\n", cmd);
		if (cmd < 0) {
			break;
		}
		pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
		Count++;
	}
	TRACE("recv        %d\r\n", Count);
	pClient->CloseSocket();
}

// 获得路径
CString CRemoteClientDlg::GetPath(HTREEITEM hTree)
{
	CString strRet, strTmp;
	do
	{
		strTmp = m_Tree.GetItemText(hTree);
		strRet = strTmp + '\\' + strRet;
		hTree = m_Tree.GetParentItem(hTree);
	} while (hTree != NULL);
	return strRet;
}

// 删除树的子文件
void CRemoteClientDlg::DeleteTreeChildrenItem(HTREEITEM hTree)
{
	HTREEITEM hSub = NULL;
	do
	{
		hSub = m_Tree.GetChildItem(hTree);
		if (hSub != NULL) m_Tree.DeleteItem(hSub);
	} while (hSub != NULL);
}

// 双击树的文件夹
void CRemoteClientDlg::OnNMDblclkTreeDir(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	LoadFileInfo();
}

// 单击树的文件夹
void CRemoteClientDlg::OnNMClickTreeDir(NMHDR* pNMHDR, LRESULT* pResult)
{
	LoadFileInfo();
	*pResult = 0;
}

// 右键单击list文件
void CRemoteClientDlg::OnNMRClickListFile(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	*pResult = 0;
	CPoint ptMouse, ptList;
	GetCursorPos(&ptMouse);
	ptList = ptMouse;
	m_List.ScreenToClient(&ptList);
	int ListSelected = m_List.HitTest(ptList);
	if (ListSelected < 0) return;
	CMenu menu;
	menu.LoadMenu(IDR_MENU_RCLICK);
	CMenu* pPupup = menu.GetSubMenu(0);
	if (pPupup != NULL) {
		pPupup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, ptMouse.x, ptMouse.y, this);
	}
}

// 下载文件
void CRemoteClientDlg::OnDownloadFile()
{
	// 添加线程函数
    _beginthread(CRemoteClientDlg::threadEntryForDownFile, 0, this);
	BeginWaitCursor();
	m_dlgStatus.m_info.SetWindowText(_T("命令正在执行中!"));
	m_dlgStatus.ShowWindow(SW_SHOW);
	m_dlgStatus.CenterWindow(this);
	m_dlgStatus.SetActiveWindow();
}

// 删除文件
void CRemoteClientDlg::OnDeleteFile()
{
    HTREEITEM hSelected = m_Tree.GetSelectedItem();
    CString strPath = GetPath(hSelected);
    int nSelected = m_List.GetSelectionMark();
    CString strFile = m_List.GetItemText(nSelected, 0);
    strFile = strPath + strFile;
    int ret = SendCommandPacket(9, true, (BYTE*)(LPCSTR)strFile, strFile.GetLength());
    if (ret < 0) {
        AfxMessageBox("删除文件命令执行失败!!");
    }
	LoadFileCurrent();
}

// 打开文件
void CRemoteClientDlg::OnRunFile()
{
	HTREEITEM hSelected = m_Tree.GetSelectedItem();
	CString strPath = GetPath(hSelected);
	int nSelected = m_List.GetSelectionMark();
	CString strFile = m_List.GetItemText(nSelected, 0);
	strFile = strPath + strFile;
	int ret = SendCommandPacket(3, true, (BYTE*)(LPCSTR)strFile, strFile.GetLength());
	if (ret < 0) {
		AfxMessageBox("打开文件命令执行失败!!");
	}
}

// 实现消息响应函数④
LRESULT CRemoteClientDlg::onSendPacket(WPARAM wParam, LPARAM lParam)
{
	CString strFile = (LPCSTR)lParam;
    int ret = SendCommandPacket(wParam >> 1, wParam&1, (BYTE*)(LPCSTR)strFile, strFile.GetLength());
	return ret;
}

// 开启监视线程
void CRemoteClientDlg::OnBnClickedBtnStartWatch()
{
    _beginthread(CRemoteClientDlg::threadEntryForWatchData, 0, this);
	CWatchDialog dlg(this);// 方便控制父对象
	dlg.DoModal();
}

void CRemoteClientDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	CDialogEx::OnTimer(nIDEvent);
}
