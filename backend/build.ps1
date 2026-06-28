#########################################################
# File: build.ps1
# Date: 2026-06-20
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Windows build, deployment, and daemon control for lymalinkd.
# Usage:
#   .\build.ps1 clean          - Clean build directory
#   .\build.ps1 debug          - Debug build
#   .\build.ps1 release        - Release build
#   .\build.ps1 deploy         - Clean + Release build + per-user install
#   .\build.ps1 deploy --debug - Clean + Debug build + per-user install
#   .\build.ps1 start          - Start lymalinkd
#   .\build.ps1 stop           - Stop lymalinkd
#   .\build.ps1 restart        - Restart lymalinkd
#   .\build.ps1 status         - Show lymalinkd status
#   .\build.ps1 logs           - Follow lymalinkd log
#   .\build.ps1 uninstall      - Remove per-user install, preserve user data
#########################################################

param(
    [Parameter(Position = 0)]
    [string]$Command,

    [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$CommandArguments
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$SCRIPT_DIR = $PSScriptRoot
$BUILD_ROOT = Join-Path $SCRIPT_DIR "build\windows"
$CMAKE_GENERATOR = "Ninja"
$NLOHMANN_VERSION = "3.12.0"
$MINIAUDIO_VERSION = "0.11.25"
$NLOHMANN_PATH = Join-Path $SCRIPT_DIR "src\nlohmann\json.hpp"
$MINIAUDIO_PATH = Join-Path $SCRIPT_DIR "src\miniaudio\miniaudio.h"
$STB_VORBIS_PATH = Join-Path $SCRIPT_DIR "src\miniaudio\stb_vorbis.c"
$NLOHMANN_URL = "https://github.com/nlohmann/json/releases/download/v$NLOHMANN_VERSION/json.hpp"
$MINIAUDIO_URL = "https://raw.githubusercontent.com/mackron/miniaudio/$MINIAUDIO_VERSION/miniaudio.h"
$STB_VORBIS_URL = "https://raw.githubusercontent.com/mackron/miniaudio/$MINIAUDIO_VERSION/extras/stb_vorbis.c"
$LOG_PATH = Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)) "Lymalink\logs\lymalink-backend.log"
$AUTOSTART_REGISTRY_KEY = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
$AUTOSTART_VALUE_NAME = "Lymalinkd"

##############################################################################

function Test-NoExtraArgs {
    param(
        [string]$CommandName,
        [string[]]$Options
    )

    if ($null -ne $Options) {
        throw "Too many options for $CommandName. Usage: .\build.ps1 $CommandName"
    }
}

##############################################################################

function Get-WindowsInstallDirectory {
    $localAppData = [Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)
    if ([string]::IsNullOrWhiteSpace($localAppData)) {
        throw "Could not resolve LOCALAPPDATA."
    }

    return Join-Path $localAppData "Programs\Lymalink"
}

##############################################################################

function Resolve-Qmake {
    foreach ($candidate in @("qmake6.exe", "qmake.exe", "qmake6", "qmake")) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -eq $command) {
            continue
        }

        & $command.Source -query QT_INSTALL_PREFIX *> $null
        if ($LASTEXITCODE -eq 0) {
            return $command.Source
        }
    }

    throw "Could not find Qt qmake. Load Qt's MSVC environment first."
}

##############################################################################

function Initialize-MsvcX64Environment {
    if ($env:VSCMD_ARG_TGT_ARCH -eq "x64" -and $null -ne (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        return
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw "Could not find Visual Studio Installer's vswhere.exe. Install Visual Studio Build Tools with the MSVC x64/x86 build tools component."
    }

    $vsInstallPath = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($vsInstallPath)) {
        throw "Could not find Visual Studio Build Tools with MSVC x64/x86 build tools installed."
    }

    $vsInstallPath = $vsInstallPath.Trim()
    $devShellModule = Join-Path $vsInstallPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    if (-not (Test-Path -LiteralPath $devShellModule -PathType Leaf)) {
        throw "Could not find Visual Studio PowerShell environment module: $devShellModule"
    }

    Write-Host "==> Loading MSVC x64 build environment..."
    Import-Module -Name $devShellModule -ErrorAction Stop
    Enter-VsDevShell -VsInstallPath $vsInstallPath -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null

    if ($env:VSCMD_ARG_TGT_ARCH -ne "x64" -or $null -eq (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw "Could not load MSVC x64 compiler environment."
    }
}

##############################################################################

function Resolve-Vcpkg {
    if ([string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
        throw "VCPKG_ROOT is not set. Install sqlite with: vcpkg install sqlite3:x64-windows"
    }

    $root = [System.IO.Path]::GetFullPath($env:VCPKG_ROOT)
    $installed = Join-Path $root "installed\x64-windows"
    $toolchain = Join-Path $root "scripts\buildsystems\vcpkg.cmake"
    $required = @(
        (Join-Path $root "vcpkg.exe"),
        $toolchain,
        (Join-Path $installed "include\sqlite3.h"),
        (Join-Path $installed "lib\sqlite3.lib"),
        (Join-Path $installed "bin\sqlite3.dll"),
        (Join-Path $installed "debug\bin\sqlite3.dll")
    )

    if (@($required | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }).Count -gt 0) {
        throw "sqlite3:x64-windows is missing. Run: & `"$root\vcpkg.exe`" install sqlite3:x64-windows"
    }

    return [PSCustomObject]@{
        Root          = $root
        InstalledDir  = $installed
        ToolchainFile = $toolchain
    }
}

##############################################################################

function Test-Toolchain {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        throw "build.ps1 must run on Windows."
    }

    $qmakePath = Resolve-Qmake
    $qtSpec = (& $qmakePath -query QMAKE_SPEC)
    if ($LASTEXITCODE -ne 0) {
        throw "Could not query Qt build specification with: $qmakePath"
    }
    if ($qtSpec -notmatch 'msvc') {
        throw "Qt kit must use MSVC for vcpkg triplet x64-windows. Found QMAKE_SPEC: $qtSpec"
    }
    Initialize-MsvcX64Environment

    if ($null -eq (Get-Command cmake -ErrorAction SilentlyContinue)) {
        throw "Could not find cmake. Install CMake and add it to PATH."
    }

    if ($CMAKE_GENERATOR -eq "Ninja" -and $null -eq (Get-Command ninja -ErrorAction SilentlyContinue)) {
        throw "Ninja generator selected but ninja is not on PATH."
    }

    # Backend-specific: expose Qt to CMake package discovery.
    $qtPrefix = (& $qmakePath -query QT_INSTALL_PREFIX).Trim()
    if ($env:CMAKE_PREFIX_PATH -notlike "*$qtPrefix*") {
        $env:CMAKE_PREFIX_PATH = "$qtPrefix;$env:CMAKE_PREFIX_PATH"
    }
}

##############################################################################

function Get-Dependency {
    param(
        [string]$Path,
        [string]$Uri,
        [string]$Name
    )

    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        Write-Host "==> $Name found: $Path"
        return
    }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    Write-Host "==> Downloading $Name..."
    Invoke-WebRequest -Uri $Uri -OutFile $Path
}

##############################################################################

function Get-BuildDirectory {
    param([string]$Mode)

    return Join-Path $BUILD_ROOT $Mode.ToLowerInvariant()
}

##############################################################################

function Get-BinaryPath {
    param([string]$Mode)

    return Join-Path (Get-BuildDirectory $Mode) "bin\lymalinkd.exe"
}

##############################################################################

function Clean {
    $buildDirectory = Join-Path $SCRIPT_DIR "build"
    Write-Host "==> Cleaning build directory..."

    if (Test-Path -LiteralPath $buildDirectory) {
        Remove-Item -LiteralPath $buildDirectory -Recurse -Force
    }

    Write-Host "==> Clean done."
}

##############################################################################

function Build {
    param([ValidateSet("Debug", "Release")][string]$Mode)

    Test-Toolchain
    Get-Dependency $NLOHMANN_PATH $NLOHMANN_URL "nlohmann/json v$NLOHMANN_VERSION"
    Get-Dependency $MINIAUDIO_PATH $MINIAUDIO_URL "Miniaudio v$MINIAUDIO_VERSION"
    Get-Dependency $STB_VORBIS_PATH $STB_VORBIS_URL "stb_vorbis for Miniaudio"

    $vcpkg = Resolve-Vcpkg
    $buildDir = Get-BuildDirectory $Mode
    $cache = Join-Path $buildDir "CMakeCache.txt"
    if (Test-Path -LiteralPath $cache -PathType Leaf) {
        $toolchainLine = Select-String -LiteralPath $cache -Pattern '^CMAKE_TOOLCHAIN_FILE(:[^=]+)?=(.+)$' | Select-Object -First 1
        if ($null -eq $toolchainLine -or $toolchainLine.Matches[0].Groups[2].Value.Replace('/', '\') -ine $vcpkg.ToolchainFile) {
            Remove-Item -LiteralPath $buildDir -Recurse -Force
        }
    }

    Write-Host "==> Configuring Windows $Mode build..."
    & cmake -S $SCRIPT_DIR -B $buildDir -G $CMAKE_GENERATOR "-DCMAKE_BUILD_TYPE=$Mode" "-DVCPKG_TARGET_TRIPLET=x64-windows" "-DCMAKE_TOOLCHAIN_FILE=$($vcpkg.ToolchainFile)"
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed."
    }

    Write-Host "==> Building Windows $Mode..."
    & cmake --build $buildDir --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed."
    }

    $binary = Get-BinaryPath $Mode
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
        throw "Expected binary not found: $binary"
    }

    Write-Host "==> Done: $binary"
}

##############################################################################

function Get-InstalledDaemonProcesses {
    $binary = Join-Path (Get-WindowsInstallDirectory) "lymalinkd.exe"
    $wanted = [System.IO.Path]::GetFullPath($binary)
    return @(Get-CimInstance Win32_Process -Filter "Name = 'lymalinkd.exe'" -ErrorAction SilentlyContinue | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_.ExecutablePath) -and [System.IO.Path]::GetFullPath($_.ExecutablePath) -ieq $wanted
    })
}

##############################################################################

function Invoke-BackendRequest {
    param(
        [string]$Method,
        [int]$TimeoutMilliseconds = 1000
    )

    $pipe = [System.IO.Pipes.NamedPipeClientStream]::new(".", "lymalinkd-ipc", [System.IO.Pipes.PipeDirection]::InOut, [System.IO.Pipes.PipeOptions]::Asynchronous)
    try {
        $pipe.Connect($TimeoutMilliseconds)
        $writer = [System.IO.StreamWriter]::new($pipe, [System.Text.UTF8Encoding]::new($false), 1024, $true)
        $writer.AutoFlush = $true
        $request = @{
            type   = "request"
            id     = 1
            method = $Method
        }
        $writer.WriteLine(($request | ConvertTo-Json -Compress))
        $reader = [System.IO.StreamReader]::new($pipe, [System.Text.UTF8Encoding]::new($false), $false, 1024, $true)
        $task = $reader.ReadLineAsync()
        if (-not $task.Wait($TimeoutMilliseconds)) {
            return $null
        }

        return $task.Result | ConvertFrom-Json
    }
    catch {
        return $null
    }
    finally {
        $pipe.Dispose()
    }
}

##############################################################################

function Wait-ForBackend {
    param([int]$TimeoutSeconds = 10)

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        $reply = Invoke-BackendRequest "Ping" 500
        if ($null -ne $reply -and $reply.ok -and $reply.result -eq "pong") {
            return $true
        }

        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)

    return $false
}

##############################################################################

function Start-Backend {
    $binary = Join-Path (Get-WindowsInstallDirectory) "lymalinkd.exe"
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
        throw "Installed backend not found: $binary. Run deploy first."
    }
    if (Wait-ForBackend 1) {
        Write-Host "==> lymalinkd is already running."
        return
    }

    Write-Host "==> Starting lymalinkd..."
    Start-Process -FilePath $binary -WorkingDirectory (Split-Path -Parent $binary) -WindowStyle Hidden
    if (-not (Wait-ForBackend)) {
        throw "lymalinkd started but did not respond on lymalinkd-ipc."
    }

    Write-Host "==> lymalinkd is running."
}

##############################################################################

function Stop-Backend {
    $processes = @(Get-InstalledDaemonProcesses)
    if ($processes.Count -eq 0) {
        Write-Host "==> lymalinkd is not running."
        return
    }

    Write-Host "==> Stopping lymalinkd..."
    $reply = Invoke-BackendRequest "Shutdown" 1000
    if ($null -eq $reply -or -not $reply.ok) {
        throw "lymalinkd did not accept Shutdown request."
    }

    $deadline = (Get-Date).AddSeconds(5)
    do {
        Start-Sleep -Milliseconds 200
        $processes = @(Get-InstalledDaemonProcesses)
    } while ($processes.Count -gt 0 -and (Get-Date) -lt $deadline)

    if ($processes.Count -gt 0) {
        throw "lymalinkd did not stop after Shutdown request."
    }

    Write-Host "==> lymalinkd stopped."
}

##############################################################################

function Deploy {
    param([string[]]$Options)

    if ($null -ne $Options -and ($Options.Count -gt 1 -or $Options[0] -ne "--debug")) {
        throw "Usage: .\build.ps1 deploy [--debug]"
    }

    if ($null -ne $Options) {
        $mode = "Debug"
    }
    else {
        $mode = "Release"
    }
    $installDir = Get-WindowsInstallDirectory
    Stop-Backend
    Clean
    Build $mode
    $vcpkg = Resolve-Vcpkg
    if ($mode -eq "Debug") {
        $runtimeRelativePath = "debug\bin\sqlite3.dll"
    }
    else {
        $runtimeRelativePath = "bin\sqlite3.dll"
    }
    $runtime = Join-Path $vcpkg.InstalledDir $runtimeRelativePath
    New-Item -ItemType Directory -Force -Path $installDir | Out-Null
    Copy-Item -LiteralPath (Get-BinaryPath $mode) -Destination (Join-Path $installDir "lymalinkd.exe") -Force
    Copy-Item -LiteralPath $runtime -Destination (Join-Path $installDir "sqlite3.dll") -Force

    $soundDir = Join-Path $installDir "sounds"
    if (Test-Path -LiteralPath $soundDir) {
        Remove-Item -LiteralPath $soundDir -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $soundDir | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $SCRIPT_DIR "res") -Filter "*.ogg" -File | Copy-Item -Destination $soundDir
    Copy-Item -LiteralPath (Join-Path $SCRIPT_DIR "..\frontend\res\img\64x64-lymalink-test-icon.png") -Destination (Join-Path $installDir "64x64-lymalink-test-icon.png") -Force

    foreach ($path in @((Join-Path $installDir "lymalinkd.exe"), (Join-Path $installDir "sqlite3.dll"), (Join-Path $installDir "64x64-lymalink-test-icon.png"))) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Deployment verification failed: $path"
        }
    }
    Write-Host "==> Deployed backend: $installDir"
}

##############################################################################

function Show-Status {
    $processes = @(Get-InstalledDaemonProcesses)
    $reply = Invoke-BackendRequest "Ping" 500

    if ($processes.Count -gt 0) {
        $processStatus = "running"
    }
    else {
        $processStatus = "stopped"
    }

    if ($null -ne $reply -and $reply.ok -and $reply.result -eq "pong") {
        $ipcStatus = "ready"
    }
    else {
        $ipcStatus = "unavailable"
    }

    Write-Host "Process: $processStatus"
    Write-Host "IPC:     $ipcStatus"
}

##############################################################################

function Show-Logs {
    if (-not (Test-Path -LiteralPath $LOG_PATH -PathType Leaf)) {
        throw "Backend log not found: $LOG_PATH"
    }

    Get-Content -LiteralPath $LOG_PATH -Tail 100 -Wait
}

##############################################################################

function Uninstall {
    Stop-Backend

    if (Get-ItemProperty -Path $AUTOSTART_REGISTRY_KEY -Name $AUTOSTART_VALUE_NAME -ErrorAction SilentlyContinue) {
        Remove-ItemProperty -Path $AUTOSTART_REGISTRY_KEY -Name $AUTOSTART_VALUE_NAME
    }

    $installDir = Get-WindowsInstallDirectory
    foreach ($path in @("lymalinkd.exe", "sqlite3.dll", "64x64-lymalink-test-icon.png")) {
        $target = Join-Path $installDir $path
        if (Test-Path -LiteralPath $target) {
            Remove-Item -LiteralPath $target -Force
        }
    }

    $soundDir = Join-Path $installDir "sounds"
    if (Test-Path -LiteralPath $soundDir) {
        Remove-Item -LiteralPath $soundDir -Recurse -Force
    }
    if ((Test-Path -LiteralPath $installDir -PathType Container) -and @(Get-ChildItem -LiteralPath $installDir -Force).Count -eq 0) {
        Remove-Item -LiteralPath $installDir -Force
    }

    Write-Host "==> Backend uninstall done. User data and logs preserved."
}

##############################################################################

switch ($Command) {
    "clean" {
        Test-NoExtraArgs "clean" $CommandArguments
        Clean
    }
    "debug" {
        Test-NoExtraArgs "debug" $CommandArguments
        Build Debug
    }
    "release" {
        Test-NoExtraArgs "release" $CommandArguments
        Build Release
    }
    "deploy" {
        Deploy $CommandArguments
    }
    "start" {
        Test-NoExtraArgs "start" $CommandArguments
        Start-Backend
    }
    "stop" {
        Test-NoExtraArgs "stop" $CommandArguments
        Stop-Backend
    }
    "restart" {
        Test-NoExtraArgs "restart" $CommandArguments
        Stop-Backend
        Start-Backend
    }
    "status" {
        Test-NoExtraArgs "status" $CommandArguments
        Show-Status
    }
    "logs" {
        Test-NoExtraArgs "logs" $CommandArguments
        Show-Logs
    }
    "uninstall" {
        Test-NoExtraArgs "uninstall" $CommandArguments
        Uninstall
    }
    default {
        Write-Host "Usage: .\build.ps1 [clean|debug|release|deploy|start|stop|restart|status|logs|uninstall]"
        exit 1
    }
}
