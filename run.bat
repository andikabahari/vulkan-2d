@echo off
setlocal

mkdir .\bin 2>nul
cd    .\bin

set cl_include=/I"..\src" /I"%VULKAN_SDK%\Include" /I"..\thirdparty\glfw\include"
set cl_libpath=/LIBPATH:"%VULKAN_SDK%\Lib" /LIBPATH:"..\thirdparty\glfw\lib"
set cl_libfile=vulkan-1.lib glfw3.lib user32.lib gdi32.lib shell32.lib

call cl   ..\src\main.cpp /c /Zi /MD %cl_include%
call link .\main.obj /DEBUG /INCREMENTAL:NO /OUT:temp.exe %cl_libpath% %cl_libfile% /MACHINE:X64

if exist ".\temp.exe" (
    del  .\main.exe 2>nul
    move .\temp.exe .\main.exe 1>nul
    call .\main.exe
)

endlocal
