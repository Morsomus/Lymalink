#########################################################
# File: build.ps1
# Date: 2026-06-20
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Automated Windows build and deployment script for Lymalink.
# Usage:
#   .\build.ps1 debug          - Debug build
#   .\build.ps1 release        - Release build
#   .\build.ps1 clean          - Clean Windows build directory
#   .\build.ps1 deploy         - Clean + Release build + per-user install
#   .\build.ps1 deploy --debug - Clean + Debug build + per-user install
#   .\build.ps1 uninstall      - Remove per-user install, preserve user data
#   .\build.ps1 dev            - Clean + Debug build + Execute
#   .\build.ps1 test           - Clean + Debug build + Test
#   .\build.ps1 test --silent  - Clean + Debug build + Test with failures only
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
$MIN_QT_VERSION = [Version]"6.8.0"
$CMAKE_GENERATOR = "Ninja"
$BACKEND_OWNED_INSTALL_ITEMS = @("lymalinkd.exe", "sqlite3.dll", "64x64-lymalink-test-icon.png", "sounds", "overlay")
$REQUIRED_QML_MODULES = @("Qt5Compat\GraphicalEffects")

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

function Test-QtVersion {
    param([string]$QmakePath)

    $qtVersionText = (& $QmakePath -query QT_VERSION)
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($qtVersionText)) {
        throw "Could not query Qt version with: $QmakePath"
    }

    try {
        $qtVersion = [Version]$qtVersionText.Trim()
    }
    catch {
        throw "Invalid Qt version returned by ${QmakePath}: $qtVersionText"
    }

    if ($qtVersion -lt $MIN_QT_VERSION) {
        throw "Qt $qtVersionText is too old for the frontend QML runtime. Build with Qt >= $MIN_QT_VERSION."
    }
}

##############################################################################

function Test-RequiredQtQmlModules {
    param([string]$QmakePath)

    $qtQmlDir = (& $QmakePath -query QT_INSTALL_QML)
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($qtQmlDir)) {
        throw "Could not query Qt QML module directory with: $QmakePath"
    }

    $qtQmlDir = $qtQmlDir.Trim()
    foreach ($module in $REQUIRED_QML_MODULES) {
        $moduleDir = Join-Path $qtQmlDir $module
        $moduleManifest = Join-Path $moduleDir "qmldir"
        if (-not (Test-Path -LiteralPath $moduleManifest -PathType Leaf)) {
            throw "Required Qt QML module is not installed: $module. Install the Qt 5 Compatibility Module for this Qt kit. Expected: $moduleManifest"
        }
    }
}

##############################################################################

function Test-DeployedQtQmlModules {
    param([string]$BundleDirectory)

    foreach ($module in $REQUIRED_QML_MODULES) {
        $moduleManifest = Join-Path (Join-Path $BundleDirectory "qml") (Join-Path $module "qmldir")
        if (-not (Test-Path -LiteralPath $moduleManifest -PathType Leaf)) {
            throw "windeployqt did not bundle required Qt QML module: $module. Missing: $moduleManifest"
        }
    }
}

##############################################################################

function Resolve-Windeployqt {
    param([string]$QmakePath)

    $qtBinDir = (& $QmakePath -query QT_INSTALL_BINS)
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($qtBinDir)) {
        throw "Could not query Qt binary directory with: $QmakePath"
    }
    $qtBinDir = $qtBinDir.Trim()
    $candidate = Join-Path $qtBinDir "windeployqt.exe"
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        return $candidate
    }

    $command = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    throw "Could not find windeployqt.exe in this Qt installation."
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
        throw "VCPKG_ROOT is not set. Set it to your vcpkg checkout, then install OpenSSL: & `"`$env:VCPKG_ROOT\vcpkg.exe`" install openssl:x64-windows"
    }

    $root = [System.IO.Path]::GetFullPath($env:VCPKG_ROOT)
    $vcpkgExe = Join-Path $root "vcpkg.exe"
    $toolchainFile = Join-Path $root "scripts\buildsystems\vcpkg.cmake"
    $installedDir = Join-Path $root "installed\x64-windows"
    $headerPath = Join-Path $installedDir "include\openssl\ssl.h"
    $cryptoLibrary = Join-Path $installedDir "lib\libcrypto.lib"

    if (-not (Test-Path -LiteralPath $vcpkgExe -PathType Leaf)) {
        throw "VCPKG_ROOT does not contain vcpkg.exe: $vcpkgExe"
    }
    if (-not (Test-Path -LiteralPath $toolchainFile -PathType Leaf)) {
        throw "VCPKG_ROOT does not contain vcpkg's CMake toolchain file: $toolchainFile"
    }
    if (-not (Test-Path -LiteralPath $headerPath -PathType Leaf) -or -not (Test-Path -LiteralPath $cryptoLibrary -PathType Leaf)) {
        throw "OpenSSL x64-windows is not installed in vcpkg. Run: & `"$vcpkgExe`" install openssl:x64-windows"
    }

    return [PSCustomObject]@{
        Root          = $root
        InstalledDir  = $installedDir
        ToolchainFile = $toolchainFile
    }
}

##############################################################################

function Get-VcpkgRuntimeDirectory {
    param(
        [PSCustomObject]$Vcpkg,
        [ValidateSet("Debug", "Release")][string]$Mode
    )

    $relativePath = if ($Mode -eq "Debug") { "debug\bin" } else { "bin" }
    $runtimeDir = Join-Path $Vcpkg.InstalledDir $relativePath
    if (-not (Test-Path -LiteralPath $runtimeDir -PathType Container)) {
        throw "vcpkg OpenSSL runtime directory not found: $runtimeDir"
    }

    return $runtimeDir
}

##############################################################################

function Add-VcpkgRuntimePath {
    param([string]$RuntimeDirectory)

    $pathEntries = @($env:PATH -split ';')
    if ($pathEntries -notcontains $RuntimeDirectory) {
        $env:PATH = "$RuntimeDirectory;$env:PATH"
    }
}

##############################################################################

function Test-Toolchain {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        throw "build.ps1 must run on Windows."
    }

    $qmakePath = Resolve-Qmake
    Test-QtVersion $qmakePath
    Test-RequiredQtQmlModules $qmakePath

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
}

##############################################################################

function Get-BuildDirectory {
    param([string]$Mode)

    return Join-Path $BUILD_ROOT $Mode.ToLowerInvariant()
}

##############################################################################

function Test-MultiConfigBuild {
    param([string]$BuildDirectory)

    $cachePath = Join-Path $BuildDirectory "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
        return $false
    }

    return [bool](Select-String -LiteralPath $cachePath -Pattern '^CMAKE_CONFIGURATION_TYPES:STRING=' -Quiet)
}

##############################################################################

function Get-BinaryPath {
    param([string]$Mode)

    $buildDir = Get-BuildDirectory $Mode
    if (Test-MultiConfigBuild $buildDir) {
        return Join-Path $buildDir "bin\$Mode\Lymalink.exe"
    }

    return Join-Path $buildDir "bin\Lymalink.exe"
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

    # Check that compiler and tools exist
    Test-Toolchain

    # Setup build paths and update environment PATH
    $buildDir = Get-BuildDirectory $Mode
    $vcpkg = Resolve-Vcpkg
    Add-VcpkgRuntimePath (Get-VcpkgRuntimeDirectory $vcpkg $Mode)

    # Clean stale or incompatible CMake cache
    $cachePath = Join-Path $buildDir "CMakeCache.txt"
    if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
        $isX86Cache = Select-String -LiteralPath $cachePath -Pattern '^CMAKE_(SIZEOF_VOID_P:INTERNAL=4|CXX_COMPILER:FILEPATH=.*[/\\]x86[/\\]cl\.exe)$' -Quiet
        $toolchainEntry = Select-String -LiteralPath $cachePath -Pattern '^CMAKE_TOOLCHAIN_FILE(?::[^=]+)?=(.+)$' | Select-Object -First 1
        $usesVcpkgToolchain = $null -ne $toolchainEntry -and $toolchainEntry.Matches[0].Groups[1].Value.Replace('/', '\') -ieq $vcpkg.ToolchainFile
        if ($isX86Cache -or -not $usesVcpkgToolchain) {
            $reason = if ($isX86Cache) { "stale x86 compiler" } else { "missing or changed vcpkg toolchain" }
            Write-Host "==> Removing CMake cache ($reason)..."
            Remove-Item -LiteralPath $buildDir -Recurse -Force
        }
    }
    $binaryPath = Get-BinaryPath $Mode

    # Run CMake configuration
    Write-Host "==> Configuring Windows $Mode build with $CMAKE_GENERATOR..."
    $cmakeConfigureArgs = @(
        "-S", $SCRIPT_DIR,
        "-B", $buildDir,
        "-G", $CMAKE_GENERATOR,
        "-DCMAKE_BUILD_TYPE=$Mode",
        "-DVCPKG_TARGET_TRIPLET=x64-windows"
    )
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
        $cmakeConfigureArgs += "-DCMAKE_TOOLCHAIN_FILE=$($vcpkg.ToolchainFile)"
    }
    if ($CMAKE_GENERATOR -like "Visual Studio*") {
        $cmakeConfigureArgs += @("-A", "x64")
    }
    & cmake @cmakeConfigureArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed."
    }

    # Refresh binary path based on finalized generator
    $binaryPath = Get-BinaryPath $Mode

    # Build project in parallel
    Write-Host "==> Building Windows $Mode..."
    if (Test-MultiConfigBuild $buildDir) {
        & cmake --build $buildDir --config $Mode --parallel
    }
    else {
        & cmake --build $buildDir --parallel
    }
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed."
    }

    # Verify that the executable was created
    if (-not (Test-Path -LiteralPath $binaryPath -PathType Leaf)) {
        throw "Expected binary not found: $binaryPath"
    }

    Write-Host "==> Done: $binaryPath"
}

##############################################################################

function Stop-LymalinkProcess {
    $processes = @(Get-Process -Name "Lymalink" -ErrorAction SilentlyContinue)
    if ($processes.Count -eq 0) {
        return
    }

    Write-Host "==> Stopping running Lymalink process..."
    $processes | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1

    if (@(Get-Process -Name "Lymalink" -ErrorAction SilentlyContinue).Count -ne 0) {
        throw "Could not stop Lymalink. Close it, then run this command again."
    }
}

##############################################################################

function Get-WindowsInstallPaths {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        throw "Windows installation is only supported on Windows."
    }

    $localAppData = [Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)
    $startMenuRoot = [Environment]::GetFolderPath([Environment+SpecialFolder]::StartMenu)
    if ([string]::IsNullOrWhiteSpace($localAppData) -or [string]::IsNullOrWhiteSpace($startMenuRoot)) {
        throw "Could not resolve current user's Windows application directories."
    }

    $appName = "Lymalink"
    $installDirectory = Join-Path $localAppData "Programs\$appName"
    $startMenuDirectory = Join-Path $startMenuRoot "Programs\$appName"

    return [PSCustomObject]@{
        InstallDirectory = $installDirectory
        ShortcutPath     = Join-Path $startMenuDirectory "$appName.lnk"
        StartMenuDirectory = $startMenuDirectory
    }
}

##############################################################################

function New-LymalinkStartMenuShortcut {
    param(
        [string]$InstallDirectory,
        [string]$ShortcutPath
    )

    $shortcutDirectory = Split-Path -Parent $ShortcutPath
    New-Item -ItemType Directory -Path $shortcutDirectory -Force | Out-Null

    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($ShortcutPath)
    $shortcut.TargetPath = Join-Path $InstallDirectory "Lymalink.exe"
    $shortcut.WorkingDirectory = $InstallDirectory
    $shortcut.IconLocation = "$($shortcut.TargetPath),0"
    $shortcut.Description = "Lymalink"
    $shortcut.Save()
}

##############################################################################

function Deploy {
    param([string[]]$Options)

    $mode = "Release"
    if ($null -ne $Options -and $Options.Count -gt 1) {
        throw "Too many deploy options. Usage: .\build.ps1 deploy [--debug]"
    }

    if ($null -ne $Options) {
        if ($Options[0] -eq "--debug") {
            $mode = "Debug"
        }
        else {
            throw "Unknown deploy option: $($Options[0]). Usage: .\build.ps1 deploy [--debug]"
        }
    }

    $installPaths = Get-WindowsInstallPaths
    Clean
    Build $mode

    $modeLower = $mode.ToLowerInvariant()
    $binaryPath = Get-BinaryPath $mode
    $bundleDir = Join-Path $BUILD_ROOT "bundle-$modeLower"
    $vcpkg = Resolve-Vcpkg
    $runtimeDir = Get-VcpkgRuntimeDirectory $vcpkg $mode
    $qmakePath = Resolve-Qmake
    $windeployqt = Resolve-Windeployqt $qmakePath

    if (Test-Path -LiteralPath $bundleDir) {
        Remove-Item -LiteralPath $bundleDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $bundleDir -Force | Out-Null
    Copy-Item -LiteralPath $binaryPath -Destination (Join-Path $bundleDir "Lymalink.exe")
    Get-ChildItem -LiteralPath $runtimeDir -Filter "*.dll" -File | Copy-Item -Destination $bundleDir

    Write-Host "==> Running windeployqt..."
    & $windeployqt --qmldir $SCRIPT_DIR "--$modeLower" (Join-Path $bundleDir "Lymalink.exe")
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed."
    }
    Test-DeployedQtQmlModules $bundleDir

    Stop-LymalinkProcess
    if (Test-Path -LiteralPath $installPaths.InstallDirectory -PathType Container) {
        Get-ChildItem -LiteralPath $installPaths.InstallDirectory -Force |
            Where-Object { $_.Name -notin $BACKEND_OWNED_INSTALL_ITEMS } |
            Remove-Item -Recurse -Force
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $installPaths.InstallDirectory) -Force | Out-Null
    New-Item -ItemType Directory -Path $installPaths.InstallDirectory -Force | Out-Null
    Get-ChildItem -LiteralPath $bundleDir -Force | Where-Object { $_.Name -notin $BACKEND_OWNED_INSTALL_ITEMS } | Copy-Item -Destination $installPaths.InstallDirectory -Recurse -Force
    if (-not (Test-Path -LiteralPath (Join-Path $installPaths.InstallDirectory "Lymalink.exe") -PathType Leaf)) {
        throw "Installed binary not found: $($installPaths.InstallDirectory)\Lymalink.exe"
    }

    New-LymalinkStartMenuShortcut $installPaths.InstallDirectory $installPaths.ShortcutPath

    Write-Host "==> Installed: $($installPaths.InstallDirectory)"
    Write-Host "==> Start Menu: $($installPaths.ShortcutPath)"
}

##############################################################################

function Uninstall {
    $installPaths = Get-WindowsInstallPaths
    Stop-LymalinkProcess

    Write-Host "==> Removing Lymalink installation..."

    if (Test-Path -LiteralPath $installPaths.InstallDirectory -PathType Container) {
        Get-ChildItem -LiteralPath $installPaths.InstallDirectory -Force |
            Where-Object { $_.Name -notin $BACKEND_OWNED_INSTALL_ITEMS } |
            Remove-Item -Recurse -Force
        if (@(Get-ChildItem -LiteralPath $installPaths.InstallDirectory -Force).Count -eq 0) {
            Remove-Item -LiteralPath $installPaths.InstallDirectory -Force
        }
    }
    if (Test-Path -LiteralPath $installPaths.ShortcutPath -PathType Leaf) {
        Remove-Item -LiteralPath $installPaths.ShortcutPath -Force
    }
    if ((Test-Path -LiteralPath $installPaths.StartMenuDirectory -PathType Container) -and @(Get-ChildItem -LiteralPath $installPaths.StartMenuDirectory -Force).Count -eq 0) {
        Remove-Item -LiteralPath $installPaths.StartMenuDirectory -Force
    }

    Write-Host "==> Uninstall done. User settings and data were preserved."
}

##############################################################################

function Dev {
    Clean
    Build Debug

    Write-Host "==> Launching..."
    Start-Process -FilePath (Get-BinaryPath Debug)
}

##############################################################################

function Invoke-Tests {
    param([string[]]$Options)

    if ($null -ne $Options -and ($Options.Count -gt 1 -or $Options[0] -ne "--silent")) {
        throw "Usage: .\build.ps1 test [--silent]"
    }

    Clean
    Build Debug

    $buildDir = Get-BuildDirectory Debug
    Write-Host "==> Running tests..."
    if ($null -ne $Options) {
        & ctest --test-dir $buildDir --output-on-failure
    }
    else {
        & ctest --test-dir $buildDir --verbose
    }

    if ($LASTEXITCODE -ne 0) {
        throw "Tests failed."
    }
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
    "uninstall" {
        Test-NoExtraArgs "uninstall" $CommandArguments
        Uninstall
    }
    "dev" {
        Test-NoExtraArgs "dev" $CommandArguments
        Dev
    }
    "test" {
        Invoke-Tests $CommandArguments
    }
    default {
        Write-Host "Usage: .\build.ps1 [clean|debug|release|deploy|uninstall|dev|test]"
        Write-Host ""
        Write-Host "  clean          - Remove build\"
        Write-Host "  debug          - Debug build   -> build\windows\debug\"
        Write-Host "  release        - Release build -> build\windows\release\"
        Write-Host "  deploy         - clean + release build + per-user install"
        Write-Host "  deploy --debug - clean + debug build + per-user install"
        Write-Host "  uninstall      - remove per-user install, preserve user data"
        Write-Host "  dev            - clean + debug build + launch"
        Write-Host "  test           - clean + debug build + test (full verbosity)"
        Write-Host "  test --silent  - clean + debug build + test (failures only)"
        exit 1
    }
}
