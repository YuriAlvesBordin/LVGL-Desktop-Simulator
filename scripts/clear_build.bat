@echo off
setlocal EnableExtensions

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
if "%~1"=="" (
    if defined BUILD_DIR (
        set "BUILD_DIR=%BUILD_DIR%"
    ) else (
        set "BUILD_DIR=%ROOT_DIR%\build"
    )
) else (
    set "BUILD_DIR=%~1"
)
for %%I in ("%BUILD_DIR%") do set "BUILD_DIR=%%~fI"
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

if /I "%BUILD_DIR%"=="%ROOT_DIR%" (
    echo Error: refusing to clear the project root.
    exit /b 2
)
if "%BUILD_DIR:~1,2%"==":\" if "%BUILD_DIR:~3%"=="" (
    echo Error: refusing to clear a drive root.
    exit /b 2
)

if not exist "%BUILD_DIR%" (
    echo [clear] build directory does not exist: %BUILD_DIR%
    exit /b 0
)

rmdir /s /q "%BUILD_DIR%"
if errorlevel 1 (
    echo Error: could not remove %BUILD_DIR%.
    exit /b 1
)
echo [clear] removed %BUILD_DIR%
exit /b 0
