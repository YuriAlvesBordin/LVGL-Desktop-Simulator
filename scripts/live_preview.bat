@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
if not defined BUILD_DIR set "BUILD_DIR=%ROOT_DIR%\build"
if not defined CMAKE_BUILD_TYPE set "CMAKE_BUILD_TYPE=Debug"
for /f "usebackq delims=" %%I in (`powershell -NoProfile -Command "$line=Get-Content -LiteralPath '%ROOT_DIR%\config\project_config.h' | Where-Object { $_ -match '^\s*#define\s+LVGL_GLFW_PREVIEW_INTERVAL_SECONDS\s+(.+)$' } | Select-Object -First 1; if($line -match '^\s*#define\s+LVGL_GLFW_PREVIEW_INTERVAL_SECONDS\s+(.+)$'){ $matches[1] }"`) do set "PREVIEW_INTERVAL=%%I"
if not defined PREVIEW_INTERVAL set "PREVIEW_INTERVAL=0.35"
set "APP_BINARY=%BUILD_DIR%\lvgl-glfw-app.exe"
set "CHILD_PID="
set "LAST_SIGNATURE="

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

echo [live-preview] watching %ROOT_DIR%

:main_loop
call :watch_signature CURRENT_SIGNATURE
if not "!CURRENT_SIGNATURE!"=="!LAST_SIGNATURE!" (
    echo [live-preview] change detected
    call :configure_and_build
    if not errorlevel 1 (
        call :stop_app
        call :start_app
        if errorlevel 1 echo [live-preview] build succeeded, but the application failed to start.
        set "LAST_SIGNATURE=!CURRENT_SIGNATURE!"
    ) else (
        echo [live-preview] build failed; keeping the current application running.
        set "LAST_SIGNATURE=!CURRENT_SIGNATURE!"
    )
)

if defined CHILD_PID (
    call :process_exists !CHILD_PID!
    if errorlevel 1 set "CHILD_PID="
)
call :sleep_interval
goto main_loop

:watch_signature
set "SIGNATURE_VALUE="
for /f "usebackq delims=" %%S in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "$root='%ROOT_DIR%'; $directories=@($root+'\src\app',$root+'\src\integration',$root+'\config',$root+'\cmake'); $files=@($root+'\CMakeLists.txt',$root+'\CMakePresets.json',$root+'\src\app\CMakeLists.txt',$root+'\src\integration\CMakeLists.txt'); $records=foreach($directory in $directories){if(Test-Path -LiteralPath $directory){Get-ChildItem -LiteralPath $directory -File -Recurse | Sort-Object FullName | ForEach-Object { '{0}|{1}|{2}' -f $_.FullName,$_.Length,$_.LastWriteTimeUtc.Ticks }}}; $records+=foreach($file in $files){if(Test-Path -LiteralPath $file){$item=Get-Item -LiteralPath $file; '{0}|{1}|{2}' -f $item.FullName,$item.Length,$item.LastWriteTimeUtc.Ticks}}; $bytes=[Text.Encoding]::UTF8.GetBytes(($records -join [Environment]::NewLine)); (-join ([Security.Cryptography.SHA256]::Create().ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') })).ToLowerInvariant()"`) do set "SIGNATURE_VALUE=%%S"
set "%~1=%SIGNATURE_VALUE%"
exit /b 0

:configure_and_build
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    where ninja >nul 2>&1
    if errorlevel 1 (
        cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE="%CMAKE_BUILD_TYPE%"
    ) else (
        cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE="%CMAKE_BUILD_TYPE%"
    )
) else (
    cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE="%CMAKE_BUILD_TYPE%"
)
if errorlevel 1 exit /b 1
cmake --build "%BUILD_DIR%" --config "%CMAKE_BUILD_TYPE%" --parallel
exit /b %errorlevel%

:start_app
if not exist "%APP_BINARY%" (
    if exist "%BUILD_DIR%\%CMAKE_BUILD_TYPE%\lvgl-glfw-app.exe" set "APP_BINARY=%BUILD_DIR%\%CMAKE_BUILD_TYPE%\lvgl-glfw-app.exe"
)
if not exist "%APP_BINARY%" (
    echo [live-preview] executable not found: %APP_BINARY%.
    exit /b 1
)
echo [live-preview] launching %APP_BINARY%
set "CHILD_PID="
for /f "usebackq delims=" %%P in (`powershell -NoProfile -Command "$env:LVGL_GLFW_PREVIEW='1'; (Start-Process -FilePath '%APP_BINARY%' -WorkingDirectory '%BUILD_DIR%' -PassThru).Id"`) do set "CHILD_PID=%%P"
if not defined CHILD_PID exit /b 1
exit /b 0

:stop_app
if not defined CHILD_PID exit /b 0
call :process_exists !CHILD_PID!
if errorlevel 1 (
    set "CHILD_PID="
    exit /b 0
)
echo [live-preview] stopping application !CHILD_PID!
powershell -NoProfile -Command "Add-Type 'using System; using System.Runtime.InteropServices; public static class NativeMethods { [DllImport(''user32.dll'')] public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam); }'; $p=Get-Process -Id !CHILD_PID! -ErrorAction SilentlyContinue; if($p -and $p.MainWindowHandle -ne 0){[NativeMethods]::PostMessage($p.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)}" >nul 2>&1
for /L %%N in (1,1,20) do (
    call :process_exists !CHILD_PID!
    if errorlevel 1 goto stop_app_done
    powershell -NoProfile -Command "Start-Sleep -Milliseconds 50" >nul 2>&1
)
echo [live-preview] graceful close timed out; forcing application termination.
taskkill /PID !CHILD_PID! /T /F >nul 2>&1

:stop_app_done
set "CHILD_PID="
exit /b 0

:process_exists
powershell -NoProfile -Command "$p=Get-Process -Id %~1 -ErrorAction SilentlyContinue; if($p){exit 0}else{exit 1}" >nul 2>&1
exit /b %errorlevel%

:sleep_interval
powershell -NoProfile -Command "$seconds=[double]::Parse('%PREVIEW_INTERVAL%',[Globalization.CultureInfo]::InvariantCulture); Start-Sleep -Milliseconds ([Math]::Max(1,[int]($seconds*1000)))" >nul 2>&1
exit /b 0
