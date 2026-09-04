@echo off
setlocal

REM Discover Python executable
where python >nul 2>nul
if %ERRORLEVEL% equ 0 (
    python "%~dp0zweirun.py" %*
    exit /b %ERRORLEVEL%
)

where py >nul 2>nul
if %ERRORLEVEL% equ 0 (
    py -3 "%~dp0zweirun.py" %*
    exit /b %ERRORLEVEL%
)

if exist "C:\msys64\ucrt64\bin\python.exe" (
    "C:\msys64\ucrt64\bin\python.exe" "%~dp0zweirun.py" %*
    exit /b %ERRORLEVEL%
)

echo [ERROR] Python was not found in PATH or standard MSYS2 directories.
exit /b 1
