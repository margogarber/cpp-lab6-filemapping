@echo off
rem MinGW-w64 (g++ must be in PATH)
if not exist build mkdir build
set FLAGS=-std=c++17 -O2 -Wall -Wextra -municode -mwindows -static -DUNICODE -D_UNICODE
g++ %FLAGS% src\server.cpp -o build\Server.exe || goto :fail
g++ %FLAGS% src\client1.cpp src\client_core.cpp -o build\Client1.exe || goto :fail
g++ %FLAGS% src\client2.cpp src\client_core.cpp -o build\Client2.exe || goto :fail
echo Build OK
exit /b 0
:fail
echo Build FAILED
exit /b 1
