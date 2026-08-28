@echo off
setlocal EnableExtensions

where powershell.exe >nul 2>&1
if errorlevel 1 (
    echo Error: Windows PowerShell was not found in PATH.
    exit /b 127
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0live_preview.ps1"
exit /b %errorlevel%
