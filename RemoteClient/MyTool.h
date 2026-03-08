#pragma once
#include <Windows.h>
#include <string>
#include <atlimage.h>

class CMyTool
{
public:
    static void Dump(BYTE* pData, size_t nSize)
    {
        std::string strOut;
        for (size_t i = 0; i < nSize; i++) {
            char buf[8] = "0";
            if (i > 0 && (i % 16 == 0)) strOut += "\n";
            snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
            strOut += buf;
        }
        strOut += "\n";
        OutputDebugStringA(strOut.c_str());
    }

    // 包内容转换为CImage并load 
    static int Bytes2Image(CImage& image, const std::string& strBuffer) {
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);
        if (hMem == NULL) {
            TRACE("内存不足了!");
            Sleep(1);// 防止过度消耗cpu资源和带宽
            return -1;
        }
        IStream* pStream = NULL;
        HRESULT hRet = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
        if (hRet == S_OK) {
            ULONG length = 0;
            pStream->Write((BYTE*)strBuffer.c_str(), strBuffer.size(), &length);
            LARGE_INTEGER bg{};
            pStream->Seek(bg, STREAM_SEEK_SET, NULL);
            if ((HBITMAP)image != NULL)
            {
                image.Destroy();
            }
            image.Load(pStream);
        }
        return hRet;
    }
};