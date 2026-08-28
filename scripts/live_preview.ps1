[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RootDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$BuildDir = if ([string]::IsNullOrWhiteSpace($env:BUILD_DIR)) {
    Join-Path $RootDir 'build'
}
else {
    [System.IO.Path]::GetFullPath($env:BUILD_DIR)
}
$BuildType = if ([string]::IsNullOrWhiteSpace($env:CMAKE_BUILD_TYPE)) {
    'Debug'
}
else {
    $env:CMAKE_BUILD_TYPE
}
$ProjectConfig = Join-Path $RootDir 'config\project_config.h'
$IntervalText = '0.35'
$ChildProcess = $null
$LastSignature = ''

function Read-Define {
    param(
        [string]$Path,
        [string]$Name,
        [string]$Fallback
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $Fallback
    }

    $match = Select-String -LiteralPath $Path -Pattern "^\s*#define\s+$Name\s+(.+)$" |
        Select-Object -First 1
    if ($null -ne $match) {
        return $match.Matches.Groups[1].Value.Trim()
    }
    return $Fallback
}

function Get-WatchedSignature {
    $directories = @(
        (Join-Path $RootDir 'src\app'),
        (Join-Path $RootDir 'src\integration'),
        (Join-Path $RootDir 'config'),
        (Join-Path $RootDir 'cmake')
    )
    $files = @(
        (Join-Path $RootDir 'CMakeLists.txt'),
        (Join-Path $RootDir 'CMakePresets.json'),
        (Join-Path $RootDir 'src\app\CMakeLists.txt'),
        (Join-Path $RootDir 'src\integration\CMakeLists.txt'),
        (Join-Path $RootDir 'scripts\live_preview.bat'),
        (Join-Path $RootDir 'scripts\live_preview.ps1')
    )
    $records = [System.Collections.Generic.List[string]]::new()

    foreach ($directory in $directories) {
        if (Test-Path -LiteralPath $directory -PathType Container) {
            Get-ChildItem -LiteralPath $directory -File -Recurse |
                Sort-Object FullName |
                ForEach-Object {
                    [void]$records.Add(('{0}|{1}|{2}' -f $_.FullName, $_.Length, $_.LastWriteTimeUtc.Ticks))
                }
        }
    }

    foreach ($file in $files) {
        if (Test-Path -LiteralPath $file -PathType Leaf) {
            $item = Get-Item -LiteralPath $file
            [void]$records.Add(('{0}|{1}|{2}' -f $item.FullName, $item.Length, $item.LastWriteTimeUtc.Ticks))
        }
    }

    $bytes = [Text.Encoding]::UTF8.GetBytes(($records.ToArray() -join [Environment]::NewLine))
    $hasher = [Security.Cryptography.SHA256]::Create()
    try {
        return (-join ($hasher.ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') })).ToLowerInvariant()
    }
    finally {
        $hasher.Dispose()
    }
}

function Test-ChildProcess {
    if ($null -eq $script:ChildProcess) {
        return $false
    }
    try {
        $script:ChildProcess.Refresh()
        return -not $script:ChildProcess.HasExited
    }
    catch {
        return $false
    }
}

function Stop-ChildProcess {
    if ($null -eq $script:ChildProcess) {
        return
    }

    try {
        $script:ChildProcess.Refresh()
        if ($script:ChildProcess.HasExited) {
            $script:ChildProcess = $null
            return
        }

        Write-Host "[live-preview] stopping application $($script:ChildProcess.Id)"
        if ($script:ChildProcess.MainWindowHandle -ne [IntPtr]::Zero) {
            [void]$script:ChildProcess.CloseMainWindow()
        }

        $deadline = [DateTime]::UtcNow.AddSeconds(2)
        do {
            Start-Sleep -Milliseconds 50
            $script:ChildProcess.Refresh()
        } while (-not $script:ChildProcess.HasExited -and [DateTime]::UtcNow -lt $deadline)

        if (-not $script:ChildProcess.HasExited) {
            Write-Host '[live-preview] graceful close timed out; forcing application termination.'
            Stop-Process -Id $script:ChildProcess.Id -Force -ErrorAction SilentlyContinue
        }
    }
    catch {
        Stop-Process -Id $script:ChildProcess.Id -Force -ErrorAction SilentlyContinue
    }
    finally {
        $script:ChildProcess = $null
    }
}

function Start-ChildProcess {
    $candidate = Join-Path $BuildDir 'lvgl-glfw-app.exe'
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        $candidate = Join-Path $BuildDir "$BuildType\lvgl-glfw-app.exe"
    }
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        Write-Host "[live-preview] executable not found: $candidate"
        return $false
    }

    Write-Host "[live-preview] launching $candidate"
    $hadPreviewVariable = Test-Path Env:LVGL_GLFW_PREVIEW
    $previousPreview = if ($hadPreviewVariable) {
        (Get-Item Env:LVGL_GLFW_PREVIEW).Value
    }
    else {
        $null
    }
    $env:LVGL_GLFW_PREVIEW = '1'
    try {
        $script:ChildProcess = Start-Process -FilePath $candidate -WorkingDirectory $BuildDir -PassThru
    }
    finally {
        if (-not $hadPreviewVariable) {
            Remove-Item Env:LVGL_GLFW_PREVIEW -ErrorAction SilentlyContinue
        }
        else {
            $env:LVGL_GLFW_PREVIEW = $previousPreview
        }
    }
    return $true
}

function Build-Project {
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    if (-not (Test-Path -LiteralPath (Join-Path $BuildDir 'CMakeCache.txt') -PathType Leaf)) {
        $configureArgs = @('-S', $RootDir, '-B', $BuildDir, "-DCMAKE_BUILD_TYPE=$BuildType")
        if ($null -ne (Get-Command ninja.exe -ErrorAction SilentlyContinue) -or
            $null -ne (Get-Command ninja -ErrorAction SilentlyContinue)) {
            $configureArgs += @('-G', 'Ninja')
        }
        & cmake @configureArgs
    }
    else {
        & cmake -S $RootDir -B $BuildDir "-DCMAKE_BUILD_TYPE=$BuildType"
    }
    if ($LASTEXITCODE -ne 0) {
        return $false
    }

    & cmake --build $BuildDir --config $BuildType --parallel
    return $LASTEXITCODE -eq 0
}

function Wait-PreviewInterval {
    $seconds = [double]::Parse($script:IntervalText, [Globalization.CultureInfo]::InvariantCulture)
    $milliseconds = [Math]::Max(1, [int]($seconds * 1000))
    Start-Sleep -Milliseconds $milliseconds
}

try {
    $script:IntervalText = Read-Define -Path $ProjectConfig -Name 'LVGL_GLFW_PREVIEW_INTERVAL_SECONDS' -Fallback '0.35'
    if ($null -eq (Get-Command cmake.exe -ErrorAction SilentlyContinue) -and
        $null -eq (Get-Command cmake -ErrorAction SilentlyContinue)) {
        throw 'cmake was not found in PATH.'
    }

    Write-Host "[live-preview] watching $RootDir"
    while ($true) {
        $currentSignature = Get-WatchedSignature
        if ($currentSignature -ne $LastSignature) {
            Write-Host '[live-preview] change detected'
            if (Build-Project) {
                Stop-ChildProcess
                if (Start-ChildProcess) {
                    $LastSignature = $currentSignature
                }
                else {
                    Write-Host '[live-preview] build succeeded, but the application failed to start.'
                    $LastSignature = ''
                }
            }
            else {
                Write-Host '[live-preview] build failed; keeping the current application running.'
                $LastSignature = $currentSignature
            }
        }

        if ($null -ne $script:ChildProcess -and -not (Test-ChildProcess)) {
            $script:ChildProcess = $null
        }
        Wait-PreviewInterval
    }
}
finally {
    Stop-ChildProcess
}
