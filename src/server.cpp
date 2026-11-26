#include "common.h"

namespace
{

const int IDM_CREATE = 1001;
const int IDM_READ   = 1002;
const int IDM_CLOSE  = 1003;
const int IDM_AUTO   = 1004;
const int IDM_CLEAR  = 1005;
const int IDM_EXIT   = 1006;
const int IDM_ABOUT  = 1007;

const int IDC_LOG    = 2001;
const int IDC_STATUS = 2002;
const int IDC_CREATE = 2003;
const int IDC_READ   = 2004;
const int IDC_CLOSE  = 2005;
const int IDC_AUTO   = 2006;

const UINT WM_APP_CHANGED = WM_APP + 1;

const wchar_t kClassName[] = L"Lab6FileMappingServer";
const wchar_t kTitle[]     = L"ЛР6 — Сервер FileMapping — Гарбєр М.О.";

HINSTANCE g_inst;
HWND      g_main, g_log, g_status, g_btnCreate, g_btnRead, g_btnClose, g_chkAuto;
HMENU     g_menuMap;
HFONT     g_uiFont, g_monoFont;
HANDLE    g_map = nullptr;
HANDLE    g_mutex = nullptr;
HANDLE    g_event = nullptr;
HANDLE    g_stop = nullptr;
HANDLE    g_watcher = nullptr;
bool      g_auto = false;
LONG      g_lastSeq = 0;
unsigned  g_received = 0;
unsigned  g_reads = 0;

void Log(const std::wstring& text)
{
    fm::AppendLog(g_log, L"[" + fm::Now() + L"] " + text);
}

void UpdateUi()
{
    bool open = g_map != nullptr;
    std::wstring status;
    if (open)
        status = L"Проекція " + std::wstring(fm::kMapName) + L" (" + std::to_wstring(fm::kMapSize) +
                 L" байт) · автовідстеження: " + (g_auto ? L"увімкнено" : L"вимкнено") +
                 L" · прочитано повідомлень: " + std::to_wstring(g_received);
    else
        status = L"FileMapping не створено";
    SetWindowTextW(g_status, status.c_str());

    EnableMenuItem(g_menuMap, IDM_CREATE, open ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(g_menuMap, IDM_READ, open ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(g_menuMap, IDM_CLOSE, open ? MF_ENABLED : MF_GRAYED);
    CheckMenuItem(g_menuMap, IDM_AUTO, g_auto ? MF_CHECKED : MF_UNCHECKED);
    EnableWindow(g_btnCreate, !open);
    EnableWindow(g_btnRead, open);
    EnableWindow(g_btnClose, open);
    SendMessageW(g_chkAuto, BM_SETCHECK, g_auto ? BST_CHECKED : BST_UNCHECKED, 0);
    DrawMenuBar(g_main);
}

DWORD WINAPI WatcherProc(LPVOID)
{
    HANDLE handles[2] = { g_stop, g_event };
    for (;;)
    {
        DWORD r = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (r == WAIT_OBJECT_0 + 1)
            PostMessageW(g_main, WM_APP_CHANGED, 0, 0);
        else
            break;
    }
    return 0;
}

void StartWatcher()
{
    if (g_watcher || !g_map) return;
    ResetEvent(g_stop);
    g_watcher = CreateThread(nullptr, 0, WatcherProc, nullptr, 0, nullptr);
    if (!g_watcher)
    {
        Log(L"Помилка CreateThread(): " + fm::ErrorText(GetLastError()));
        return;
    }
    Log(L"Автовідстеження увімкнено: потік-спостерігач чекає на подію " + std::wstring(fm::kEventName) +
        L" (WaitForMultipleObjects)");
}

void StopWatcher(bool quiet)
{
    if (!g_watcher) return;
    SetEvent(g_stop);
    WaitForSingleObject(g_watcher, INFINITE);
    CloseHandle(g_watcher);
    g_watcher = nullptr;
    if (!quiet) Log(L"Автовідстеження вимкнено: потік-спостерігач завершено");
}

void CloseObjects()
{
    if (g_event) CloseHandle(g_event);
    if (g_mutex) CloseHandle(g_mutex);
    if (g_map) CloseHandle(g_map);
    g_event = g_mutex = g_map = nullptr;
}

void CreateMapping()
{
    if (g_map)
    {
        Log(L"FileMapping уже створено");
        return;
    }

    HANDLE map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, fm::kMapSize, fm::kMapName);
    if (!map)
    {
        Log(L"Помилка CreateFileMapping(" + std::wstring(fm::kMapName) + L"): " + fm::ErrorText(GetLastError()));
        return;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(map);
        Log(L"Об'єкт " + std::wstring(fm::kMapName) + L" уже існує — ймовірно, запущено інший екземпляр сервера");
        return;
    }

    g_map   = map;
    g_mutex = CreateMutexW(nullptr, FALSE, fm::kMutexName);
    g_event = CreateEventW(nullptr, FALSE, FALSE, fm::kEventName);
    if (!g_mutex || !g_event)
    {
        Log(L"Помилка створення об'єктів синхронізації: " + fm::ErrorText(GetLastError()));
        CloseObjects();
        return;
    }

    void* view = MapViewOfFile(g_map, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!view)
    {
        Log(L"Помилка MapViewOfFile(): " + fm::ErrorText(GetLastError()));
        CloseObjects();
        return;
    }
    auto* block = static_cast<fm::SharedBlock*>(view);
    ZeroMemory(view, fm::kHeaderSize);
    block->magic = fm::kMagic;
    UnmapViewOfFile(view);

    g_lastSeq = 0;
    g_received = 0;
    g_reads = 0;
    Log(L"Створено проекцію (CreateFileMapping, INVALID_HANDLE_VALUE, PAGE_READWRITE): " + std::wstring(fm::kMapName) +
        L", " + std::to_wstring(fm::kMapSize) + L" байт");
    Log(L"Створено м'ютекс " + std::wstring(fm::kMutexName) + L" і подію " + std::wstring(fm::kEventName) +
        L" (автоскидання)");
    Log(L"Заголовок спільної пам'яті ініціалізовано через MapViewOfFile / UnmapViewOfFile");
    Log(L"Сервер FileMapping запущено");
    if (g_auto) StartWatcher();
    UpdateUi();
}

void CloseMapping()
{
    if (!g_map) return;
    StopWatcher(true);
    CloseObjects();
    Log(L"FileMapping закрито (CloseHandle): проекцію, м'ютекс і подію звільнено. Прочитано повідомлень: " +
        std::to_wstring(g_received));
    Log(L"Сервер FileMapping зупинено");
    UpdateUi();
}

void ReadMapping(bool automatic)
{
    if (!g_map)
    {
        Log(L"FileMapping не створено — читати нічого");
        return;
    }
    ++g_reads;
    std::wstring how = automatic ? L"автоматично (подія)" : L"за командою меню";

    void* view = MapViewOfFile(g_map, FILE_MAP_READ, 0, 0, 0);
    if (!view)
    {
        Log(L"Помилка MapViewOfFile(): " + fm::ErrorText(GetLastError()));
        return;
    }

    DWORD w = WaitForSingleObject(g_mutex, fm::kLockWait);
    if (w == WAIT_TIMEOUT)
    {
        UnmapViewOfFile(view);
        Log(L"Читання №" + std::to_wstring(g_reads) + L": м'ютекс зайнятий понад " + std::to_wstring(fm::kLockWait) +
            L" мс, читання скасовано");
        return;
    }
    if (w == WAIT_ABANDONED) Log(L"Попередження: м'ютекс покинуто процесом-клієнтом (WAIT_ABANDONED)");

    const auto* block = static_cast<const fm::SharedBlock*>(view);
    LONG seq = block->seq;
    DWORD pid = block->senderPid;
    DWORD len = block->length < fm::kDataSize ? block->length : fm::kDataSize;
    std::wstring sender(block->sender, wcsnlen(block->sender, 32));
    std::string data(block->data, len);
    ReleaseMutex(g_mutex);
    UnmapViewOfFile(view);
    if (!automatic) ResetEvent(g_event);

    if (seq == 0)
    {
        Log(L"Читання №" + std::to_wstring(g_reads) + L" (" + how + L"): проекція ще порожня");
        return;
    }
    if (seq == g_lastSeq)
    {
        Log(L"Читання №" + std::to_wstring(g_reads) + L" (" + how + L"): нових даних немає, останнє повідомлення №" +
            std::to_wstring(seq) + L" вже прочитано");
        return;
    }

    if (seq > g_lastSeq + 1)
        Log(L"Увага: пропущено повідомлень — " + std::to_wstring(seq - g_lastSeq - 1) +
            L" (їх перезаписано до читання)");
    g_lastSeq = seq;
    ++g_received;
    Log(L"Читання №" + std::to_wstring(g_reads) + L" (" + how + L"): повідомлення №" + std::to_wstring(seq) +
        L" від «" + sender + L"» (PID " + std::to_wstring(pid) + L"), " + std::to_wstring(len) + L" байт");
    for (const auto& line : fm::SplitLines(fm::FromUtf8(data))) fm::AppendLog(g_log, L"        " + line);
    UpdateUi();
}

void ToggleAuto()
{
    g_auto = !g_auto;
    if (g_auto)
    {
        if (g_map)
            StartWatcher();
        else
            Log(L"Автовідстеження буде увімкнено після створення FileMapping");
    }
    else
    {
        if (g_watcher)
            StopWatcher(false);
        else
            Log(L"Автовідстеження вимкнено");
    }
    UpdateUi();
}

HMENU BuildMenu()
{
    HMENU bar = CreateMenu();
    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, IDM_CLEAR, L"Очистити журнал");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, IDM_EXIT, L"Вихід");

    g_menuMap = CreatePopupMenu();
    AppendMenuW(g_menuMap, MF_STRING, IDM_CREATE, L"Створити FileMapping (Start)");
    AppendMenuW(g_menuMap, MF_STRING, IDM_READ, L"Прочитати FileMapping (Read)");
    AppendMenuW(g_menuMap, MF_STRING, IDM_CLOSE, L"Закрити FileMapping (Stop)");
    AppendMenuW(g_menuMap, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_menuMap, MF_STRING, IDM_AUTO, L"Автоматичне відстеження змін");

    HMENU help = CreatePopupMenu();
    AppendMenuW(help, MF_STRING, IDM_ABOUT, L"Про програму");

    AppendMenuW(bar, MF_POPUP, (UINT_PTR)file, L"Файл");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)g_menuMap, L"FileMapping");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)help, L"Довідка");
    return bar;
}

void Layout(int w, int h)
{
    const int pad = fm::S(8), row = fm::S(28), btn = fm::S(150), line = fm::S(20);
    MoveWindow(g_btnCreate, pad, pad, btn, row, TRUE);
    MoveWindow(g_btnRead, pad * 2 + btn, pad, btn, row, TRUE);
    MoveWindow(g_btnClose, pad * 3 + btn * 2, pad, btn, row, TRUE);
    MoveWindow(g_chkAuto, pad * 4 + btn * 3, pad, w - (pad * 5 + btn * 3), row, TRUE);
    int y = pad * 2 + row;
    MoveWindow(g_status, pad, y, w - pad * 2, line, TRUE);
    y += line + pad;
    MoveWindow(g_log, pad, y, w - pad * 2, h - y - pad, TRUE);
}

void OnCreate(HWND hWnd)
{
    g_main     = hWnd;
    g_uiFont   = fm::MakeFont(L"Segoe UI", 10);
    g_monoFont = fm::MakeFont(L"Consolas", 10);
    g_stop     = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    g_btnCreate = fm::Child(hWnd, g_inst, L"BUTTON", L"Створити", BS_PUSHBUTTON, IDC_CREATE, g_uiFont);
    g_btnRead   = fm::Child(hWnd, g_inst, L"BUTTON", L"Прочитати", BS_PUSHBUTTON, IDC_READ, g_uiFont);
    g_btnClose  = fm::Child(hWnd, g_inst, L"BUTTON", L"Закрити", BS_PUSHBUTTON, IDC_CLOSE, g_uiFont);
    g_chkAuto   = fm::Child(hWnd, g_inst, L"BUTTON", L"Автовідстеження змін", BS_CHECKBOX, IDC_AUTO, g_uiFont);
    g_status    = fm::Child(hWnd, g_inst, L"STATIC", L"", SS_LEFT | SS_ENDELLIPSIS, IDC_STATUS, g_uiFont);
    g_log       = fm::Child(hWnd, g_inst, L"EDIT", L"",
                            WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, IDC_LOG, g_monoFont);
    SendMessageW(g_log, EM_SETLIMITTEXT, 0, 0);

    Log(L"Лабораторна робота — відображення файлу в пам'ять (FileMapping)");
    Log(L"Для запуску сервера оберіть «FileMapping → Створити FileMapping (Start)»");
    UpdateUi();
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
        reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = POINT{ fm::S(700), fm::S(300) };
        return 0;

    case WM_APP_CHANGED:
        if (g_auto && g_map) ReadMapping(true);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_CREATE:
        case IDC_CREATE:
            CreateMapping();
            break;
        case IDM_READ:
        case IDC_READ:
            ReadMapping(false);
            break;
        case IDM_CLOSE:
        case IDC_CLOSE:
            CloseMapping();
            break;
        case IDM_AUTO:
        case IDC_AUTO:
            ToggleAuto();
            break;
        case IDM_CLEAR:
            SetWindowTextW(g_log, L"");
            break;
        case IDM_ABOUT:
            MessageBoxW(hWnd,
                        L"Лабораторна робота №6, варіант 1\nСервер FileMapping (відображення файлу в пам'ять)\n"
                        L"Гарбєр М.О., група ІА-з41",
                        kTitle, MB_OK | MB_ICONINFORMATION);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        }
        return 0;

    case WM_DESTROY:
        CloseMapping();
        if (g_stop) CloseHandle(g_stop);
        DeleteObject(g_uiFont);
        DeleteObject(g_monoFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    SetProcessDPIAware();
    g_inst = hInstance;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = kClassName;
    wc.hIconSm       = LoadIconW(nullptr, IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) return 1;

    HWND hWnd = CreateWindowExW(0, kClassName, kTitle, WS_OVERLAPPEDWINDOW, fm::S(10), fm::S(10), fm::S(720),
                                fm::S(460), nullptr, BuildMenu(), hInstance, nullptr);
    if (!hWnd) return 1;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
