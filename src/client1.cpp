#include "client_core.h"

namespace
{

std::wstring Collect(HWND hWnd)
{
    int cyMenu = GetSystemMetrics(SM_CYMENU);
    int mouse = GetSystemMetrics(SM_MOUSEPRESENT);
    int buttons = GetSystemMetrics(SM_CMOUSEBUTTONS);
    int wheel = GetSystemMetrics(SM_MOUSEWHEELPRESENT);

    MENUBARINFO mbi{};
    mbi.cbSize = sizeof(mbi);
    int barWidth = 0, barHeight = 0;
    if (GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi))
    {
        barWidth = mbi.rcBar.right - mbi.rcBar.left;
        barHeight = mbi.rcBar.bottom - mbi.rcBar.top;
    }

    HDC hdc = GetDC(nullptr);
    int horzRes = GetDeviceCaps(hdc, HORZRES);
    ReleaseDC(nullptr, hdc);

    std::wstring out;
    out += L"Дані Клієнта #1 (варіант 1):\n";
    out += L" - смуга меню: SM_CYMENU = " + std::to_wstring(cyMenu) + L" px (GetSystemMetrics(SM_CYMENU))\n";
    out += L"   фактичний розмір смуги меню вікна = " + std::to_wstring(barWidth) + L" x " +
           std::to_wstring(barHeight) + L" px (GetMenuBarInfo)\n";
    out += L" - наявність миші: " + std::wstring(mouse ? L"так" : L"ні") + L" (GetSystemMetrics(SM_MOUSEPRESENT) = " +
           std::to_wstring(mouse) + L"), кнопок: " + std::to_wstring(buttons) + L", коліщатко: " +
           (wheel ? L"є" : L"немає") + L"\n";
    out += L" - ширина екрану = " + std::to_wstring(horzRes) + L" px (GetDeviceCaps(hdc, HORZRES))";
    return out;
}

}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    ClientConfig cfg{ L"Lab6FileMappingClient1", L"ЛР6 — Клієнт 1 — Гарбєр М.О.", L"Клієнт 1",
                      740, 10, 620, 380, Collect };
    return RunClient(hInstance, nCmdShow, cfg);
}
