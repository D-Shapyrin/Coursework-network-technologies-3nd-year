@echo off
echo Building File Transfer Server and Client...

REM Проверка наличия компилятора
where cl >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: Visual Studio compiler (cl.exe) not found in PATH
    echo Please run this script from Visual Studio Developer Command Prompt
    echo or add Visual Studio to your PATH
    pause
    exit /b 1
)

REM Компиляция протокола
echo Compiling protocol.cpp...
cl /c /EHsc protocol.cpp /Fo:protocol.obj
if %ERRORLEVEL% NEQ 0 (
    echo Error compiling protocol.cpp
    pause
    exit /b 1
)

REM Компиляция сервера
echo Compiling server.cpp...
cl /c /EHsc server.cpp /Fo:server.obj
if %ERRORLEVEL% NEQ 0 (
    echo Error compiling server.cpp
    pause
    exit /b 1
)

REM Компиляция клиента
echo Compiling client.cpp...
cl /c /EHsc client.cpp /Fo:client.obj
if %ERRORLEVEL% NEQ 0 (
    echo Error compiling client.cpp
    pause
    exit /b 1
)

REM Линковка сервера
echo Linking server.exe...
link server.obj protocol.obj ws2_32.lib /OUT:server.exe
if %ERRORLEVEL% NEQ 0 (
    echo Error linking server.exe
    pause
    exit /b 1
)

REM Линковка клиента
echo Linking client.exe...
link client.obj protocol.obj ws2_32.lib /OUT:client.exe
if %ERRORLEVEL% NEQ 0 (
    echo Error linking client.exe
    pause
    exit /b 1
)

REM Удаление временных файлов
del *.obj

echo.
echo Build completed successfully!
echo.
echo Server: server.exe
echo Client: client.exe
echo.
pause
