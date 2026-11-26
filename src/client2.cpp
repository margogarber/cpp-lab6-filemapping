#include "client_core.h"

namespace
{

std::wstring Collect(HWND)
{
    int cxIcon = GetSystemMetrics(SM_CXICON);
    int cyIcon = GetSystemMetrics(SM_CYICON);
    int cxVScroll = GetSystemMetrics(SM_CXVSCROLL);

    HDC hdc = GetDC(nullptr);
    int bits = GetDeviceCaps(hdc, BITSPIXEL);
    int planes = GetDeviceCaps(hdc, PLANES);
    ReleaseDC(nullptr, hdc);

    std::wstring out;
    out += L"Дані Клієнта #2 (варіант 1):\n";
    out += L" - ширина та висота іконки додатку = " + std::to_wstring(cxIcon) + L" x " + std::to_wstring(cyIcon) +
           L" px (SM_CXICON, SM_CYICON)\n";
    out += L" - ширина вертикальної смуги прокрутки = " + std::to_wstring(cxVScroll) + L" px (SM_CXVSCROLL)\n";
    out += L" - глибина кольору = " + std::to_wstring(bits * planes) + L" біт на піксель (GetDeviceCaps(hdc, BITSPIXEL))";
    return out;
}

}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    ClientConfig cfg{ L"Lab6FileMappingClient2", L"ЛР6 — Клієнт 2 — Гарбєр М.О.", L"Клієнт 2",
                      740, 400, 620, 360, Collect };
    return RunClient(hInstance, nCmdShow, cfg);
}
