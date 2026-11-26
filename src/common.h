#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace fm
{

const wchar_t kMapName[]   = L"Local\\lab6_garber_map";
const wchar_t kMutexName[] = L"Local\\lab6_garber_mutex";
const wchar_t kEventName[] = L"Local\\lab6_garber_changed";

const DWORD kMapSize   = 8192;
const DWORD kMagic     = 0x4C364D46;
const DWORD kLockWait  = 2000;

struct SharedBlock
{
    DWORD   magic;
    LONG    seq;
    DWORD   senderPid;
    DWORD   length;
    wchar_t sender[32];
    char    data[1];
};

const DWORD kHeaderSize = (DWORD)offsetof(SharedBlock, data);
const DWORD kDataSize   = kMapSize - kHeaderSize;

inline std::string ToUtf8(const std::wstring& w)
{
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

inline std::wstring FromUtf8(const std::string& s)
{
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

inline std::vector<std::wstring> SplitLines(const std::wstring& text)
{
    std::vector<std::wstring> lines;
    std::wstring cur;
    for (wchar_t c : text)
    {
        if (c == L'\r') continue;
        if (c == L'\n')
        {
            lines.push_back(cur);
            cur.clear();
        }
        else
        {
            cur += c;
        }
    }
    if (!cur.empty() || lines.empty()) lines.push_back(cur);
    return lines;
}

inline std::wstring ToCrLf(const std::wstring& text)
{
    std::wstring out;
    for (const auto& line : SplitLines(text))
    {
        if (!out.empty()) out += L"\r\n";
        out += line;
    }
    return out;
}

inline std::wstring Now()
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[32];
    swprintf(buf, 32, L"%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    return buf;
}

inline std::wstring ErrorText(DWORD code)
{
    wchar_t* msg = nullptr;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, code, 0, (LPWSTR)&msg, 0, nullptr);
    std::wstring text;
    if (n && msg)
    {
        text.assign(msg, n);
        while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' ' || text.back() == L'.'))
            text.pop_back();
    }
    if (msg) LocalFree(msg);
    return L"код " + std::to_wstring(code) + (text.empty() ? L"" : L" — " + text);
}

inline void AppendLog(HWND edit, const std::wstring& line)
{
    int len = GetWindowTextLengthW(edit);
    SendMessageW(edit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    std::wstring text = line + L"\r\n";
    SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

inline int Dpi()
{
    static int dpi = 0;
    if (!dpi)
    {
        HDC hdc = GetDC(nullptr);
        dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(nullptr, hdc);
    }
    return dpi;
}

inline int S(int v)
{
    return MulDiv(v, Dpi(), 96);
}

inline HFONT MakeFont(const wchar_t* face, int pt)
{
    HDC hdc = GetDC(nullptr);
    int h = -MulDiv(pt, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(nullptr, hdc);
    return CreateFontW(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
}

inline std::wstring GetText(HWND h)
{
    int n = GetWindowTextLengthW(h);
    std::wstring s((size_t)n + 1, L'\0');
    GetWindowTextW(h, &s[0], n + 1);
    s.resize((size_t)n);
    return s;
}

inline HWND Child(HWND parent, HINSTANCE inst, const wchar_t* cls, const wchar_t* text, DWORD style, int id, HFONT font)
{
    HWND h = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, parent,
                             (HMENU)(INT_PTR)id, inst, nullptr);
    SendMessageW(h, WM_SETFONT, (WPARAM)font, TRUE);
    return h;
}

}
