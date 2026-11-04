@echo off
setlocal
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
) else (
    echo Visual Studio 32-bit compiler environment not found. Run this from a Developer Command Prompt.
)

cd /d "%~dp0"
if not exist "..\bin" mkdir "..\bin"
cl.exe /nologo /LD /O2 /GS- /Fe:..\bin\sacred-language-loader.dll language_loader.c /link /SUBSYSTEM:WINDOWS,5.01
copy /y ..\bin\sacred-language-loader.dll ..\sacred-language-loader.dll
echo Done. Output at bin\sacred-language-loader.dll and ..\sacred-language-loader.dll
