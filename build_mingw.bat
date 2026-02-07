@echo off
echo Building File Transfer Server and Client with MinGW...

REM Проверка наличия компилятора
where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: MinGW g++ compiler not found in PATH
    echo Please install MinGW and add it to your PATH
    pause
    exit /b 1
)

REM Компиляция сервера
echo Compiling server...
g++ -std=c++11 -o server.exe server.cpp protocol.cpp -lws2_32 -static-libgcc -static-libstdc++
if %ERRORLEVEL% NEQ 0 (
    echo Error compiling server
    pause
    exit /b 1
)

REM Компиляция клиента
echo Compiling client...
g++ -std=c++11 -o client.exe client.cpp protocol.cpp -lws2_32 -static-libgcc -static-libstdc++
if %ERRORLEVEL% NEQ 0 (
    echo Error compiling client
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo.
echo Server: server.exe
echo Client: client.exe
echo.
pause
