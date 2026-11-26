#pragma once

#include "common.h"

struct ClientConfig
{
    const wchar_t* className;
    const wchar_t* title;
    const wchar_t* name;
    int            x;
    int            y;
    int            w;
    int            h;
    std::wstring (*collect)(HWND hWnd);
};

int RunClient(HINSTANCE hInstance, int nCmdShow, const ClientConfig& config);
