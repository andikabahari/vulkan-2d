@echo off
setlocal

if not exist .\bin mkdir .\bin
cd .\bin

set cl_include=/I"..\src" /I"%VULKAN_SDK%\Include" /I"..\thirdparty\glfw\include"
set cl_libpath=/LIBPATH:"%VULKAN_SDK%\Lib" /LIBPATH:"..\thirdparty\glfw\lib"
set cl_libfile=vulkan-1.lib glfw3.lib user32.lib gdi32.lib shell32.lib

set cl_compile=call cl ..\src\main.cpp /Zi /MD %cl_include%
set cl_link=/link /INCREMENTAL:NO /OUT:main.exe %cl_libpath% %cl_libfile%

%cl_compile% %cl_link%

if %ERRORLEVEL% equ 0 (
    cd   ..
    call .\bin\main.exe
)

endlocal
