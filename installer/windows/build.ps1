#########################################################
# File: build.ps1
# Date: 2026-07-06
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Builds the per-user Windows NSIS installer for Lymalink
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
$ROOT_DIR = [System.IO.Path]::GetFullPath((Join-Path $SCRIPT_DIR "..\.."))
$BUILD_DIR = Join-Path $SCRIPT_DIR "build"
$RELEASE_DIR = Join-Path $BUILD_DIR "lymalink-release"
$ARCH = "x64"
$MIN_QT_VERSION = [Version]"6.8.0"
$REQUIRED_QML_MODULES = @("Qt5Compat\GraphicalEffects")
$OVERLAY_ARCHITECTURES = @("x64", "x86")
$OVERLAY_ARTIFACTS = @(
    "lymalink-overlay-vulkan-{0}.dll",
    "lymalink-overlay-opengl-{0}.dll",
    "lymalink-overlay-dx9-{0}.dll",
    "lymalink-overlay-dx10-{0}.dll",
    "lymalink-overlay-dx11-{0}.dll",
    "lymalink-overlay-dx12-{0}.dll",
    "lymalink-overlay-injector-{0}.exe"
)

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

function Resolve-CommandPath {
    param(
        [string[]]$Candidates,
        [string]$ErrorMessage
    )

    foreach ($candidate in $Candidates) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -ne $command) {
            return $command.Source
        }
    }

    throw $ErrorMessage
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

    $candidate = Join-Path $qtBinDir.Trim() "windeployqt.exe"
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        return $candidate
    }

    return Resolve-CommandPath @("windeployqt.exe") "Could not find windeployqt.exe in this Qt installation."
}

##############################################################################

function Resolve-Vcpkg {
    if ([string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
        throw "VCPKG_ROOT is not set. Install required Windows dependencies with vcpkg first."
    }

    $root = [System.IO.Path]::GetFullPath($env:VCPKG_ROOT)
    $installedDir = Join-Path $root "installed\x64-windows"
    $required = @(
        (Join-Path $root "vcpkg.exe"),
        (Join-Path $root "scripts\buildsystems\vcpkg.cmake"),
        (Join-Path $installedDir "bin\sqlite3.dll")
    )

    foreach ($path in $required) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required vcpkg file not found: $path"
        }
    }

    return [PSCustomObject]@{
        Root         = $root
        InstalledDir = $installedDir
        RuntimeDir   = Join-Path $installedDir "bin"
    }
}

##############################################################################

function Test-Toolchain {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        throw "Windows installer builds must run on Windows."
    }

    Resolve-CommandPath @("cmake.exe", "cmake") "Could not find cmake. Install CMake and add it to PATH." | Out-Null
    Resolve-CommandPath @("ninja.exe", "ninja") "Could not find ninja. Install Ninja and add it to PATH." | Out-Null
    Resolve-CommandPath @("makensis.exe", "makensis") "Could not find makensis.exe. Install NSIS and add it to PATH." | Out-Null

    $qmakePath = Resolve-Qmake
    Test-QtVersion $qmakePath
    Test-RequiredQtQmlModules $qmakePath
    Resolve-Windeployqt $qmakePath | Out-Null
    Resolve-Vcpkg | Out-Null

    if ([string]::IsNullOrWhiteSpace($env:VULKAN_SDK) -or -not (Test-Path -LiteralPath (Join-Path $env:VULKAN_SDK "Include\vulkan\vulkan.h") -PathType Leaf)) {
        throw "VULKAN_SDK with Vulkan headers is required."
    }
}

##############################################################################

function Get-Version {
    $versionPath = Join-Path $ROOT_DIR "VERSION"
    if (-not (Test-Path -LiteralPath $versionPath -PathType Leaf)) {
        throw "VERSION file not found: $versionPath"
    }

    $version = (Get-Content -LiteralPath $versionPath -Raw).Trim()
    if ([string]::IsNullOrWhiteSpace($version)) {
        throw "VERSION file is empty."
    }

    return $version
}

##############################################################################

function Invoke-ComponentBuild {
    param(
        [string]$RelativeScript,
        [string]$Name
    )

    $scriptPath = Join-Path $ROOT_DIR $RelativeScript
    if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) {
        throw "$Name build script not found: $scriptPath"
    }

    Write-Host "==> Building $Name release..."
    & powershell -ExecutionPolicy Bypass -File $scriptPath release
    if ($LASTEXITCODE -ne 0) {
        throw "$Name release build failed."
    }
}

##############################################################################

function Require-File {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required build artifact not found: $Path"
    }
}

##############################################################################

function New-CleanDirectory {
    param([string]$Path)

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

##############################################################################

function Get-FrontendBinaryPath {
    $binary = Join-Path $ROOT_DIR "frontend\build\windows\release\bin\Lymalink.exe"
    if (Test-Path -LiteralPath $binary -PathType Leaf) {
        return $binary
    }

    $multiConfigBinary = Join-Path $ROOT_DIR "frontend\build\windows\release\bin\Release\Lymalink.exe"
    if (Test-Path -LiteralPath $multiConfigBinary -PathType Leaf) {
        return $multiConfigBinary
    }

    return $binary
}

##############################################################################

function Stage-Payload {
    $version = Get-Version
    $qmakePath = Resolve-Qmake
    $windeployqt = Resolve-Windeployqt $qmakePath
    $vcpkg = Resolve-Vcpkg
    $frontendBinary = Get-FrontendBinaryPath
    $backendBinary = Join-Path $ROOT_DIR "backend\build\windows\release\bin\lymalinkd.exe"
    $sqliteRuntime = Join-Path $vcpkg.RuntimeDir "sqlite3.dll"
    $overlayBuildRoot = Join-Path $ROOT_DIR "backend-overlay\build\windows\Release"
    $licensePath = Join-Path $ROOT_DIR "LICENSE"
    $testIconPath = Join-Path $ROOT_DIR "frontend\res\img\64x64-lymalink-test-icon.png"

    Require-File $frontendBinary
    Require-File $backendBinary
    Require-File $sqliteRuntime
    Require-File $licensePath
    Require-File $testIconPath

    New-CleanDirectory $RELEASE_DIR
    New-Item -ItemType Directory -Path (Join-Path $RELEASE_DIR "sounds") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $RELEASE_DIR "overlay") -Force | Out-Null

    Copy-Item -LiteralPath $frontendBinary -Destination (Join-Path $RELEASE_DIR "Lymalink.exe") -Force
    Copy-Item -LiteralPath $backendBinary -Destination (Join-Path $RELEASE_DIR "lymalinkd.exe") -Force
    Copy-Item -LiteralPath $sqliteRuntime -Destination (Join-Path $RELEASE_DIR "sqlite3.dll") -Force
    Copy-Item -LiteralPath $licensePath -Destination (Join-Path $RELEASE_DIR "LICENSE") -Force
    Copy-Item -LiteralPath $testIconPath -Destination (Join-Path $RELEASE_DIR "64x64-lymalink-test-icon.png") -Force

    Get-ChildItem -LiteralPath (Join-Path $vcpkg.InstalledDir "bin") -Filter "*.dll" -File |
        Copy-Item -Destination $RELEASE_DIR -Force
    Get-ChildItem -LiteralPath (Join-Path $ROOT_DIR "backend\res") -Filter "*.ogg" -File |
        Copy-Item -Destination (Join-Path $RELEASE_DIR "sounds") -Force

    foreach ($architecture in $OVERLAY_ARCHITECTURES) {
        $binDirectory = Join-Path $overlayBuildRoot "$architecture\bin"
        foreach ($artifactPattern in $OVERLAY_ARTIFACTS) {
            $artifact = $artifactPattern -f $architecture
            $source = Join-Path $binDirectory $artifact
            Require-File $source
            Copy-Item -LiteralPath $source -Destination (Join-Path $RELEASE_DIR "overlay\$artifact") -Force
        }
    }

    Write-Host "==> Running windeployqt..."
    & $windeployqt --qmldir (Join-Path $ROOT_DIR "frontend") --release (Join-Path $RELEASE_DIR "Lymalink.exe") |
        ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed."
    }
    Test-DeployedQtQmlModules $RELEASE_DIR

    foreach ($path in @(
        (Join-Path $RELEASE_DIR "Lymalink.exe"),
        (Join-Path $RELEASE_DIR "lymalinkd.exe"),
        (Join-Path $RELEASE_DIR "sqlite3.dll"),
        (Join-Path $RELEASE_DIR "LICENSE"),
        (Join-Path $RELEASE_DIR "overlay\lymalink-overlay-vulkan-x64.dll"),
        (Join-Path $RELEASE_DIR "overlay\lymalink-overlay-vulkan-x86.dll")
    )) {
        Require-File $path
    }

    return $version
}

##############################################################################

function Format-NsisString {
    param([string]$Value)

    return $Value.Replace('$', '$$').Replace('"', '$\"')
}

##############################################################################

function New-NsisWrapperScript {
    param(
        [string]$Version,
        [string]$PayloadDir,
        [string]$LicenseFile,
        [string]$IconFile,
        [string]$OutputFile,
        [string]$InstallerScript
    )

    New-Item -ItemType Directory -Path $BUILD_DIR -Force | Out-Null

    $definesPath = Join-Path $BUILD_DIR "lymalink-installer-defines.nsh"
    $wrapperPath = Join-Path $BUILD_DIR "lymalink-installer-wrapper.nsi"

    $defines = @(
        ("!define VERSION ""{0}""" -f (Format-NsisString $Version)),
        ("!define PAYLOAD_DIR ""{0}""" -f (Format-NsisString $PayloadDir)),
        ("!define LICENSE_FILE ""{0}""" -f (Format-NsisString $LicenseFile)),
        ("!define ICON_FILE ""{0}""" -f (Format-NsisString $IconFile)),
        ("!define OUTPUT_FILE ""{0}""" -f (Format-NsisString $OutputFile))
    )
    Set-Content -LiteralPath $definesPath -Value $defines -Encoding ASCII

    $wrapper = @(
        ("!include ""{0}""" -f (Format-NsisString $definesPath)),
        ("!include ""{0}""" -f (Format-NsisString $InstallerScript))
    )
    Set-Content -LiteralPath $wrapperPath -Value $wrapper -Encoding ASCII

    return $wrapperPath
}

##############################################################################

function Build-Installer {
    Test-Toolchain

    & (Join-Path $ROOT_DIR "backend-overlay\build.ps1") clean
    & (Join-Path $ROOT_DIR "backend\build.ps1") clean
    & (Join-Path $ROOT_DIR "frontend\build.ps1") clean

    Invoke-ComponentBuild "frontend\build.ps1" "frontend"
    Invoke-ComponentBuild "backend\build.ps1" "backend"
    Invoke-ComponentBuild "backend-overlay\build.ps1" "backend-overlay"

    $version = Stage-Payload
    $makensis = Resolve-CommandPath @("makensis.exe", "makensis") "Could not find makensis.exe."
    $nsisScript = Join-Path $SCRIPT_DIR "lymalink-installer.nsi"
    $outputPath = Join-Path $BUILD_DIR "lymalink-installer-$version-win-x64.exe"
    $licensePath = Join-Path $RELEASE_DIR "LICENSE"
    $iconPath = Join-Path $ROOT_DIR "frontend\res\windows\Lymalink.ico"

    Require-File $nsisScript
    Require-File $licensePath
    Require-File $iconPath

    if (Test-Path -LiteralPath $outputPath -PathType Leaf) {
        Remove-Item -LiteralPath $outputPath -Force
    }

    $nsisWrapper = New-NsisWrapperScript `
        -Version $version `
        -PayloadDir $RELEASE_DIR `
        -LicenseFile $licensePath `
        -IconFile $iconPath `
        -OutputFile $outputPath `
        -InstallerScript $nsisScript

    Write-Host "==> Creating NSIS installer..."
    & $makensis $nsisWrapper
    if ($LASTEXITCODE -ne 0) {
        throw "makensis failed."
    }

    Require-File $outputPath
    Write-Host "==> Windows installer build done."
    Write-Host "    Staging:   $RELEASE_DIR"
    Write-Host "    Installer: $outputPath"
}

##############################################################################

function Clean {
    Write-Host "==> Cleaning Windows installer build directory..."
    if (Test-Path -LiteralPath $BUILD_DIR) {
        Remove-Item -LiteralPath $BUILD_DIR -Recurse -Force
    }
    Write-Host "==> Clean done."
}

##############################################################################

switch ($Command) {
    "clean" {
        Test-NoExtraArgs "clean" $CommandArguments
        Clean
    }
    $null {
        Build-Installer
    }
    "" {
        Build-Installer
    }
    default {
        Write-Host "Usage: .\build.ps1 [clean]"
        Write-Host ""
        Write-Host "  clean - Remove installer build directory"
        Write-Host "  <none> - Build release components and create Windows NSIS installer"
        exit 1
    }
}
