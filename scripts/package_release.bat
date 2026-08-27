@echo off
setlocal EnableExtensions

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
if not defined BUILD_DIR set "BUILD_DIR=%ROOT_DIR%\build\release"
if not defined DIST_DIR set "DIST_DIR=%ROOT_DIR%\dist"
if not defined LVGL_GLFW_RELEASE_VERSION (
    for /f "usebackq delims=" %%V in (`powershell -NoProfile -Command "$line=Get-Content -LiteralPath '%ROOT_DIR%\config\project_config.h' | Where-Object { $_ -match '^\s*#define\s+LVGL_GLFW_PROJECT_VERSION\s+\"([^\"]+)\"' } | Select-Object -First 1; if($line -match '^\s*#define\s+LVGL_GLFW_PROJECT_VERSION\s+\"([^\"]+)\"'){ $matches[1] }"`) do set "LVGL_GLFW_RELEASE_VERSION=%%V"
)
if not defined LVGL_GLFW_RELEASE_VERSION set "LVGL_GLFW_RELEASE_VERSION=0.0.0"
if not defined LVGL_GLFW_RELEASE_PLATFORM set "LVGL_GLFW_RELEASE_PLATFORM=windows"
if not defined LVGL_GLFW_RELEASE_ARCH (
    if /I "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
        set "LVGL_GLFW_RELEASE_ARCH=arm64"
    ) else if /I "%PROCESSOR_ARCHITEW6432%"=="ARM64" (
        set "LVGL_GLFW_RELEASE_ARCH=arm64"
    ) else (
        set "LVGL_GLFW_RELEASE_ARCH=x86_64"
    )
)
set "PACKAGE_NAME=lvgl-desktop-simulator-%LVGL_GLFW_RELEASE_VERSION%-%LVGL_GLFW_RELEASE_PLATFORM%-%LVGL_GLFW_RELEASE_ARCH%"
set "STAGING_DIR=%TEMP%\%PACKAGE_NAME%-%RANDOM%"
set "PACKAGE_DIR=%STAGING_DIR%\%PACKAGE_NAME%"
set "ARCHIVE_PATH=%DIST_DIR%\%PACKAGE_NAME%.zip"

where cmake >nul 2>&1
if errorlevel 1 (
    echo Error: cmake was not found in PATH.
    exit /b 127
)
where powershell >nul 2>&1
if errorlevel 1 (
    echo Error: PowerShell was not found in PATH.
    exit /b 127
)

if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"
if not exist "%PACKAGE_DIR%" mkdir "%PACKAGE_DIR%"

echo [release] configuring %BUILD_DIR%
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto failure

echo [release] building lvgl-glfw-app
cmake --build "%BUILD_DIR%" --config Release --target lvgl_glfw_app --parallel
if errorlevel 1 goto failure

set "APP_BINARY=%BUILD_DIR%\lvgl-glfw-app.exe"
if not exist "%APP_BINARY%" if exist "%BUILD_DIR%\Release\lvgl-glfw-app.exe" set "APP_BINARY=%BUILD_DIR%\Release\lvgl-glfw-app.exe"
if not exist "%APP_BINARY%" (
    echo Error: release executable was not produced at %APP_BINARY%.
    goto failure
)

copy /y "%APP_BINARY%" "%PACKAGE_DIR%\lvgl-glfw-app.exe" >nul
copy /y "%ROOT_DIR%\README.md" "%PACKAGE_DIR%\README.md" >nul
copy /y "%ROOT_DIR%\VALIDATION.md" "%PACKAGE_DIR%\VALIDATION.md" >nul
if exist "%ROOT_DIR%\LICENSE" copy /y "%ROOT_DIR%\LICENSE" "%PACKAGE_DIR%\LICENSE" >nul

set "ICON_PATH="
set "WINDOW_TITLE="
for /f "usebackq delims=" %%I in (`powershell -NoProfile -Command "$line=Get-Content -LiteralPath '%ROOT_DIR%\config\display_config.h' | Where-Object { $_ -match '^\s*#define\s+LVGL_GLFW_ICON_PATH\s+\"([^\"]*)\"' } | Select-Object -First 1; if($line -match '^\s*#define\s+LVGL_GLFW_ICON_PATH\s+\"([^\"]*)\"'){ $matches[1] }"`) do set "ICON_PATH=%%I"
for /f "usebackq delims=" %%T in (`powershell -NoProfile -Command "$line=Get-Content -LiteralPath '%ROOT_DIR%\config\display_config.h' | Where-Object { $_ -match '^\s*#define\s+LVGL_GLFW_WINDOW_TITLE\s+\"([^\"]+)\"' } | Select-Object -First 1; if($line -match '^\s*#define\s+LVGL_GLFW_WINDOW_TITLE\s+\"([^\"]+)\"'){ $matches[1] }"`) do set "WINDOW_TITLE=%%T"
if defined ICON_PATH for %%I in ("%ICON_PATH%") do set "ICON_PATH=%%~fI"
if defined ICON_PATH if exist "%ICON_PATH%" (
    for %%I in ("%ICON_PATH%") do copy /y "%ICON_PATH%" "%PACKAGE_DIR%\lvgl-glfw-app-icon%%~xI" >nul
)

> "%PACKAGE_DIR%\RELEASE.txt" echo LVGL Desktop Simulator release %LVGL_GLFW_RELEASE_VERSION%
>> "%PACKAGE_DIR%\RELEASE.txt" echo Platform: %LVGL_GLFW_RELEASE_PLATFORM%
>> "%PACKAGE_DIR%\RELEASE.txt" echo Architecture: %LVGL_GLFW_RELEASE_ARCH%
>> "%PACKAGE_DIR%\RELEASE.txt" echo LVGL and GLFW are linked into the executable from Git submodules.
>> "%PACKAGE_DIR%\RELEASE.txt" echo The host still needs a compatible OpenGL 3.3 driver and native Windows runtime components.
>> "%PACKAGE_DIR%\RELEASE.txt" echo The configured window title and optional ICO icon are included in the executable.
> "%PACKAGE_DIR%\runtime-dependencies.txt" echo Windows host dependencies are provided by the operating system and graphics driver.

if exist "%ARCHIVE_PATH%" del /f /q "%ARCHIVE_PATH%"
powershell -NoProfile -Command "Compress-Archive -LiteralPath '%PACKAGE_DIR%' -DestinationPath '%ARCHIVE_PATH%' -Force"
if errorlevel 1 goto failure
powershell -NoProfile -Command "$hash=(Get-FileHash -LiteralPath '%ARCHIVE_PATH%' -Algorithm SHA256).Hash.ToLowerInvariant(); $hash + '  ' + [IO.Path]::GetFileName('%ARCHIVE_PATH%')" > "%ARCHIVE_PATH%.sha256"
if errorlevel 1 goto failure

echo [release] archive: %ARCHIVE_PATH%
echo [release] checksum: %ARCHIVE_PATH%.sha256
rmdir /s /q "%STAGING_DIR%" >nul 2>&1
exit /b 0

:failure
echo Error: Windows release packaging failed.
rmdir /s /q "%STAGING_DIR%" >nul 2>&1
exit /b 1
