@echo off
rem Run from Developer PowerShell / Developer Command Prompt for Visual Studio
if not exist build mkdir build
set FLAGS=/nologo /std:c++17 /W4 /O2 /utf-8 /EHsc /DUNICODE /D_UNICODE
set LIBS=user32.lib gdi32.lib kernel32.lib /SUBSYSTEM:WINDOWS
cl %FLAGS% src\server.cpp /Fe:build\Server.exe /Fo:build\server.obj /link %LIBS% || goto :fail
cl %FLAGS% src\client1.cpp src\client_core.cpp /Fe:build\Client1.exe /Fo:build\ /link %LIBS% || goto :fail
cl %FLAGS% src\client2.cpp src\client_core.cpp /Fe:build\Client2.exe /Fo:build\ /link %LIBS% || goto :fail
echo Build OK: build\Server.exe build\Client1.exe build\Client2.exe
exit /b 0
:fail
echo Build FAILED
exit /b 1
