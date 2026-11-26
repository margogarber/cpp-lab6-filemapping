#include "client_core.h"

namespace
{

const int IDM_PREPARE = 1001;
const int IDM_SEND    = 1002;
const int IDM_CLEAR   = 1003;
const int IDM_EXIT    = 1004;
const int IDM_ABOUT   = 1005;

const int IDC_PREPARE = 2001;
const int IDC_SEND    = 2002;
const int IDC_STATUS  = 2003;
const int IDC_MESSAGE = 2004;
const int IDC_LOG     = 2005;
const int IDC_CAPTION = 2006;

ClientConfig g_cfg;
HINSTANCE    g_inst;
HWND         g_main, g_btnPrepare, g_btnSend, g_status, g_caption, g_message, g_log;
HFONT        g_uiFont, g_monoFont;
unsigned     g_sent = 0;

void Log(const std::wstring& text)
{
    fm::AppendLog(g_log, L"[" + fm::Now() + L"] " + text);
}

void Prepare(bool quiet)
{
    SetWindowTextW(g_message, fm::ToCrLf(g_cfg.collect(g_main)).c_str());
    if (!quiet) Log(L"Сформовано повідомлення за замовчуванням (поточні значення)");
}

void Send()
{
    std::wstring body = fm::GetText(g_message);
    if (body.empty())
    {
        Log(L"Поле повідомлення порожнє — введіть текст або натисніть «Сформувати дані»");
        SetFocus(g_message);
        return;
    }
    std::string data = fm::ToUtf8(fm::ToCrLf(body));
    if (data.size() > fm::kDataSize)
    {
        Log(L"Повідомлення завелике: " + std::to_wstring(data.size()) + L" байт (максимум " +
            std::to_wstring(fm::kDataSize) + L")");
        return;
    }

    HANDLE map = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, fm::kMapName);
    if (!map)
    {
        DWORD err = GetLastError();
        Log(L"Помилка OpenFileMapping(" + std::wstring(fm::kMapName) + L"): " + fm::ErrorText(err));
        if (err == ERROR_FILE_NOT_FOUND) Log(L"Сервер не запущено або FileMapping закрито");
        return;
    }

    HANDLE mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, fm::kMutexName);
    HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, fm::kEventName);
    void* view = MapViewOfFile(map, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, fm::kMapSize);
    if (!view || !mutex || !event)
    {
        Log(L"Помилка підготовки проекції: " + fm::ErrorText(GetLastError()));
        if (view) UnmapViewOfFile(view);
        if (event) CloseHandle(event);
        if (mutex) CloseHandle(mutex);
        CloseHandle(map);
        return;
    }

    DWORD w = WaitForSingleObject(mutex, fm::kLockWait);
    if (w == WAIT_TIMEOUT)
    {
        Log(L"М'ютекс зайнятий понад " + std::to_wstring(fm::kLockWait) + L" мс — повідомлення не записано");
    }
    else
    {
        auto* block = static_cast<fm::SharedBlock*>(view);
        block->magic = fm::kMagic;
        block->senderPid = GetCurrentProcessId();
        block->length = (DWORD)data.size();
        ZeroMemory(block->sender, sizeof(block->sender));
        wcsncpy_s(block->sender, g_cfg.name, _TRUNCATE);
        CopyMemory(block->data, data.data(), data.size());
        LONG seq = ++block->seq;
        ReleaseMutex(mutex);
        SetEvent(event);
        ++g_sent;
        Log(L"OpenFileMapping → MapViewOfFile → записано " + std::to_wstring(data.size()) +
            L" байт (повідомлення №" + std::to_wstring(seq) + L" у проекції), подію змін встановлено (SetEvent)");
    }

    UnmapViewOfFile(view);
    CloseHandle(event);
    CloseHandle(mutex);
    CloseHandle(map);
}

HMENU BuildMenu()
{
    HMENU bar = CreateMenu();
    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, IDM_PREPARE, L"Сформувати дані");
    AppendMenuW(file, MF_STRING, IDM_SEND, L"Надіслати у FileMapping");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, IDM_CLEAR, L"Очистити журнал");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, IDM_EXIT, L"Вихід");

    HMENU help = CreatePopupMenu();
    AppendMenuW(help, MF_STRING, IDM_ABOUT, L"Про програму");

    AppendMenuW(bar, MF_POPUP, (UINT_PTR)file, L"Файл");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)help, L"Довідка");
    return bar;
}

void Layout(int w, int h)
{
    const int pad = fm::S(8), row = fm::S(28), btn = fm::S(180), line = fm::S(20);
    MoveWindow(g_btnPrepare, pad, pad, btn, row, TRUE);
    MoveWindow(g_btnSend, pad * 2 + btn, pad, btn, row, TRUE);
    int y = pad * 2 + row;
    MoveWindow(g_status, pad, y, w - pad * 2, line, TRUE);
    y += line + pad;
    MoveWindow(g_caption, pad, y, w - pad * 2, line, TRUE);
    y += line;
    int rest = h - y - pad * 2;
    int msgH = rest * 52 / 100;
    MoveWindow(g_message, pad, y, w - pad * 2, msgH, TRUE);
    y += msgH + pad;
    MoveWindow(g_log, pad, y, w - pad * 2, h - y - pad, TRUE);
}

void OnCreate(HWND hWnd)
{
    g_main     = hWnd;
    g_uiFont   = fm::MakeFont(L"Segoe UI", 10);
    g_monoFont = fm::MakeFont(L"Consolas", 10);

    g_btnPrepare = fm::Child(hWnd, g_inst, L"BUTTON", L"Сформувати дані", BS_PUSHBUTTON, IDC_PREPARE, g_uiFont);
    g_btnSend    = fm::Child(hWnd, g_inst, L"BUTTON", L"Надіслати у FileMapping", BS_PUSHBUTTON, IDC_SEND, g_uiFont);
    g_status     = fm::Child(hWnd, g_inst, L"STATIC", (L"Проекція сервера: " + std::wstring(fm::kMapName)).c_str(),
                             SS_LEFT | SS_ENDELLIPSIS, IDC_STATUS, g_uiFont);
    g_caption = fm::Child(hWnd, g_inst, L"STATIC", L"Повідомлення для сервера (можна редагувати):", SS_LEFT,
                          IDC_CAPTION, g_uiFont);
    g_message = fm::Child(hWnd, g_inst, L"EDIT", L"",
                          WS_BORDER | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                          IDC_MESSAGE, g_monoFont);
    g_log = fm::Child(hWnd, g_inst, L"EDIT", L"", WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                      IDC_LOG, g_uiFont);
    SendMessageW(g_message, EM_SETLIMITTEXT, 2000, 0);
    SendMessageW(g_log, EM_SETLIMITTEXT, 0, 0);

    Log(std::wstring(g_cfg.name) + L" готовий до роботи");
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        OnCreate(hWnd);
        return 0;

    case WM_SIZE:
        Layout(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = POINT{ fm::S(460), fm::S(300) };
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_PREPARE:
        case IDC_PREPARE:
            Prepare(false);
            break;
        case IDM_SEND:
        case IDC_SEND:
            Send();
            break;
        case IDM_CLEAR:
            SetWindowTextW(g_log, L"");
            break;
        case IDM_ABOUT:
        {
            std::wstring text = L"Лабораторна робота №6, варіант 1\n" + std::wstring(g_cfg.name) +
                                L" FileMapping (відображення файлу в пам'ять)\nГарбєр М.О., група ІА-з41";
            MessageBoxW(hWnd, text.c_str(), g_cfg.title, MB_OK | MB_ICONINFORMATION);
            break;
        }
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        }
        return 0;

    case WM_DESTROY:
        DeleteObject(g_uiFont);
        DeleteObject(g_monoFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

}

int RunClient(HINSTANCE hInstance, int nCmdShow, const ClientConfig& config)
{
    SetProcessDPIAware();
    g_inst = hInstance;
    g_cfg  = config;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = g_cfg.className;
    wc.hIconSm       = LoadIconW(nullptr, IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) return 1;

    HWND hWnd = CreateWindowExW(0, g_cfg.className, g_cfg.title, WS_OVERLAPPEDWINDOW, fm::S(g_cfg.x), fm::S(g_cfg.y),
                                fm::S(g_cfg.w), fm::S(g_cfg.h), nullptr, BuildMenu(), hInstance, nullptr);
    if (!hWnd) return 1;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    Prepare(true);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000) &&
            msg.hwnd == g_message)
        {
            Send();
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
