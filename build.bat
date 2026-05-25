@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

where gcc >nul 2>&1 || ( echo [ERROR] gcc not in PATH - install MinGW and add it to PATH & goto :fail )
where g++ >nul 2>&1 || ( echo [ERROR] g++ not in PATH - install MinGW and add it to PATH & goto :fail )
where windres >nul 2>&1 || ( echo [ERROR] windres not in PATH - install MinGW and add it to PATH & goto :fail )
where dlltool >nul 2>&1 || ( echo [ERROR] dlltool not in PATH - install MinGW and add it to PATH & goto :fail )

if not exist build mkdir build

rem MinGW.org ships no import libs for d3d11 / d3dcompiler, nor the Vista+
rem user32 export SetProcessDPIAware.  Generate them from the .def files in
rem libs\ (--kill-at keeps the _name@N linker symbol but imports the
rem undecorated name the system DLL actually exports).  dxgi/windowscodecs
rem are NOT needed: those APIs are reached through COM / CoCreateInstance.
echo [implib] d3d11 d3dcompiler user32ext
dlltool --kill-at -d libs\d3d11.def       -l libs\libd3d11.a       || goto :fail
dlltool --kill-at -d libs\d3dcompiler.def -l libs\libd3dcompiler.a || goto :fail
dlltool --kill-at -d libs\user32ext.def   -l libs\libuser32ext.a   || goto :fail

set CFLAGS=-O2 -Wall -Wno-unknown-pragmas -Wno-strict-overflow -DWINVER=0x0600 -D_WIN32_WINNT=0x0600 -D_WIN32_IE=0x0600 -Isrc/third_party/stub_includes
set CXXFLAGS=%CFLAGS% -std=c++14 -fno-exceptions -fno-rtti -include objbase.h -include win_compat.h -Wno-misleading-indentation
set IMGUI_INC=-Isrc/third_party/imgui -DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD
set LDFLAGS=-mwindows -s
set LIBS=-Llibs -ld3d11 -ld3dcompiler -lwininet -lcomdlg32 -lshell32 -lshlwapi -lole32 -luser32 -luser32ext -lgdi32 -ladvapi32 -static-libgcc -static-libstdc++

set OBJS=

echo [resources] resource.rc
windres resource.rc -O coff -o build/resource.res || goto :fail
set OBJS=!OBJS! build/resource.res

for %%F in (src\core\process.c src\core\uri.c src\core\json.c src\core\favorites.c src\core\settings.c src\core\anti_afk.c src\core\crash_detect.c src\core\singleton.c src\core\thumbnail.c) do (
    echo [c]   %%F
    gcc %CFLAGS% -c %%F -o build/%%~nF.o || goto :fail
    set OBJS=!OBJS! build/%%~nF.o
)

echo [cpp] src\third_party\imgui\imgui.cpp
g++ %CXXFLAGS% %IMGUI_INC% -c src/third_party/imgui/imgui.cpp -o build/imgui.o || goto :fail
set OBJS=!OBJS! build/imgui.o

echo [cpp] src\third_party\imgui\imgui_draw.cpp
g++ %CXXFLAGS% %IMGUI_INC% -c src/third_party/imgui/imgui_draw.cpp -o build/imgui_draw.o || goto :fail
set OBJS=!OBJS! build/imgui_draw.o

echo [cpp] src\third_party\imgui\imgui_widgets.cpp
g++ %CXXFLAGS% %IMGUI_INC% -c src/third_party/imgui/imgui_widgets.cpp -o build/imgui_widgets.o || goto :fail
set OBJS=!OBJS! build/imgui_widgets.o

echo [cpp] src\third_party\imgui\imgui_tables.cpp
g++ %CXXFLAGS% %IMGUI_INC% -c src/third_party/imgui/imgui_tables.cpp -o build/imgui_tables.o || goto :fail
set OBJS=!OBJS! build/imgui_tables.o

echo [cpp] src\third_party\imgui\backends\imgui_impl_win32.cpp
g++ %CXXFLAGS% %IMGUI_INC% -c src/third_party/imgui/backends/imgui_impl_win32.cpp -o build/imgui_impl_win32.o || goto :fail
set OBJS=!OBJS! build/imgui_impl_win32.o

echo [cpp] src\third_party\imgui\backends\imgui_impl_dx11.cpp
g++ %CXXFLAGS% %IMGUI_INC% -c src/third_party/imgui/backends/imgui_impl_dx11.cpp -o build/imgui_impl_dx11.o || goto :fail
set OBJS=!OBJS! build/imgui_impl_dx11.o

echo [cpp] src\gui\Texture.cpp
g++ %CXXFLAGS% %IMGUI_INC% -c src/gui/Texture.cpp -o build/Texture.o || goto :fail
set OBJS=!OBJS! build/Texture.o

echo [cpp] src\gui\Tabs.cpp
g++ %CXXFLAGS% %IMGUI_INC% -c src/gui/Tabs.cpp -o build/Tabs.o || goto :fail
set OBJS=!OBJS! build/Tabs.o

echo [cpp] src\gui\App.cpp
g++ %CXXFLAGS% %IMGUI_INC% -c src/gui/App.cpp -o build/App.o || goto :fail
set OBJS=!OBJS! build/App.o

echo [link] RobloxHandler.exe
g++ %LDFLAGS% !OBJS! %LIBS% -o RobloxHandler.exe || goto :fail

echo.
echo Build done: RobloxHandler.exe
endlocal
exit /b 0

:fail
echo.
echo [BUILD FAILED] See the error above.
pause
exit /b 1
