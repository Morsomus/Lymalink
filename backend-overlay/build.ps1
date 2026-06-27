#########################################################
# File: build.ps1
# Date: 2026-06-21
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Windows build, deployment, and removal for Lymalink overlay.
# Usage:
#   .\build.ps1 clean          - Clean Windows overlay build directory
#   .\build.ps1 debug          - Debug build
#   .\build.ps1 release        - Release build
#   .\build.ps1 deploy         - Release build + per-user install
#   .\build.ps1 deploy --debug - Debug build + per-user install
#   .\build.ps1 uninstall      - Remove per-user overlay install
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
$BUILD_DIR = Join-Path $SCRIPT_DIR "build"
$BUILD_ROOT = Join-Path $BUILD_DIR "windows"
$CMAKE_GENERATOR = "Ninja"
$IMGUI_VERSION = "1.92.8"
$IMGUI_DIR = Join-Path $SCRIPT_DIR "src\imgui"
$INSTALL_DIR_NAME = "Programs\Lymalink\overlay"
$ARCHITECTURES = @("x64", "x86")

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

    return Join-Path $localAppData $INSTALL_DIR_NAME
}

##############################################################################

function Get-BuildDirectory {
    param(
        [ValidateSet("Debug", "Release")]
        [string]$Mode,
        [ValidateSet("x64", "x86")]
        [string]$Architecture
    )

    return Join-Path $BUILD_ROOT "$Mode\$Architecture"
}

##############################################################################

function Initialize-MsvcEnvironment {
    param([ValidateSet("x64", "x86")][string]$Architecture)

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw "Could not find Visual Studio Installer's vswhere.exe. Install Visual Studio Build Tools with MSVC x64/x86 build tools."
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

    Write-Host "==> Loading MSVC $Architecture build environment..."
    Import-Module -Name $devShellModule -ErrorAction Stop
    Enter-VsDevShell -VsInstallPath $vsInstallPath -SkipAutomaticLocation -DevCmdArguments "-arch=$Architecture -host_arch=x64" | Out-Null

    if ($env:VSCMD_ARG_TGT_ARCH -ne $Architecture -or $null -eq (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw "Could not load MSVC $Architecture compiler environment."
    }
}

##############################################################################

function Test-Toolchain {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        throw "build.ps1 must run on Windows."
    }

    if ($null -eq (Get-Command cmake -ErrorAction SilentlyContinue)) {
        throw "Could not find cmake. Install CMake and add it to PATH."
    }
    if ($CMAKE_GENERATOR -eq "Ninja" -and $null -eq (Get-Command ninja -ErrorAction SilentlyContinue)) {
        throw "Ninja generator selected but ninja is not on PATH."
    }
    if ([string]::IsNullOrWhiteSpace($env:VULKAN_SDK) -or -not (Test-Path -LiteralPath (Join-Path $env:VULKAN_SDK "Include\vulkan\vulkan.h") -PathType Leaf)) {
        throw "VULKAN_SDK with Vulkan headers is required."
    }
}

##############################################################################

function Get-SourceArchive {
    param(
        [string]$Directory,
        [string]$ExpectedFile,
        [string]$Uri,
        [string]$Name
    )

    $expectedPath = Join-Path $Directory $ExpectedFile
    if (Test-Path -LiteralPath $expectedPath -PathType Leaf) {
        Write-Host "==> $Name found: $expectedPath"
        return
    }

    Write-Host "==> Downloading $Name..."
    Remove-Item -LiteralPath $Directory -Recurse -Force -ErrorAction SilentlyContinue
    $zipPath = Join-Path $env:TEMP "$Name.zip"
    $extractDirectory = Join-Path $env:TEMP "$Name-extract"
    Remove-Item -LiteralPath $extractDirectory -Recurse -Force -ErrorAction SilentlyContinue
    Invoke-WebRequest -Uri $Uri -OutFile $zipPath
    Expand-Archive -LiteralPath $zipPath -DestinationPath $extractDirectory -Force

    $source = Get-ChildItem -LiteralPath $extractDirectory -Directory | Select-Object -First 1
    if ($null -eq $source) {
        throw "Downloaded $Name archive has no source directory."
    }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Directory) | Out-Null
    Move-Item -LiteralPath $source.FullName -Destination $Directory
    Remove-Item -LiteralPath $zipPath -Force
    Remove-Item -LiteralPath $extractDirectory -Recurse -Force
}

##############################################################################

function Get-Dependencies {
    Get-SourceArchive $IMGUI_DIR "imgui.h" "https://github.com/ocornut/imgui/archive/refs/tags/v$IMGUI_VERSION.zip" "imgui-$IMGUI_VERSION"
}

##############################################################################

function Clean {
    Write-Host "==> Cleaning Windows overlay build directory..."
    if (Test-Path -LiteralPath $BUILD_DIR) {
        Remove-Item -LiteralPath $BUILD_DIR -Recurse -Force
    }
    Write-Host "==> Clean done."
}

##############################################################################

function Build {
    param([ValidateSet("Debug", "Release")][string]$Mode)

    Test-Toolchain
    Get-Dependencies

    foreach ($architecture in $ARCHITECTURES) {
        Initialize-MsvcEnvironment $architecture
        $buildDirectory = Get-BuildDirectory $Mode $architecture

        Write-Host "==> Configuring Windows $Mode $architecture overlay..."
        & cmake -S $SCRIPT_DIR -B $buildDirectory -G $CMAKE_GENERATOR "-DCMAKE_BUILD_TYPE=$Mode"
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configure failed for $architecture."
        }

        Write-Host "==> Building Windows $Mode $architecture overlay..."
        & cmake --build $buildDirectory --parallel
        if ($LASTEXITCODE -ne 0) {
            throw "CMake build failed for $architecture."
        }

        $binDirectory = Join-Path $buildDirectory "bin"
        foreach ($artifact in @("lymalink-overlay-vulkan-$architecture.dll")) {
            $path = Join-Path $binDirectory $artifact
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
                throw "Expected build artifact not found: $path"
            }
        }

        Write-Host "==> Done: $binDirectory"
    }
}

##############################################################################

function Get-RegistryView {
    param([ValidateSet("x64", "x86")][string]$Architecture)

    if ($Architecture -eq "x86") {
        return [Microsoft.Win32.RegistryView]::Registry32
    }
    return [Microsoft.Win32.RegistryView]::Registry64
}

##############################################################################

function Set-VulkanRegistryValue {
    param(
        [string]$ManifestPath,
        [ValidateSet("x64", "x86")][string]$Architecture,
        [bool]$Enabled
    )

    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser, (Get-RegistryView $Architecture))
    try {
        if ($Enabled) {
            $key = $base.CreateSubKey("Software\Khronos\Vulkan\ImplicitLayers")
        }
        else {
            $key = $base.OpenSubKey("Software\Khronos\Vulkan\ImplicitLayers", $true)
        }
        if ($null -eq $key) {
            return
        }
        try {
            if ($Enabled) {
                # Vulkan loader opens Windows layer manifests whose registry DWORD data is 0
                $key.SetValue($ManifestPath, 1, [Microsoft.Win32.RegistryValueKind]::DWord)
            }
            else {
                $key.DeleteValue($ManifestPath, $false)
            }
        }
        finally {
            $key.Dispose()
        }
    }
    finally {
        $base.Dispose()
    }
}

##############################################################################

function Write-VulkanManifest {
    param(
        [string]$InstallDirectory,
        [ValidateSet("x64", "x86")][string]$Architecture
    )

    $dll = Join-Path $InstallDirectory "lymalink-overlay-vulkan-$Architecture.dll"
    $manifest = Join-Path $InstallDirectory "lymalink-overlay-vulkan-$Architecture.json"
    $escapedDll = $dll.Replace("\", "\\")
    @"
{
  "file_format_version": "1.0.0",
  "layer": {
    "name": "VK_LAYER_LYMALINK_overlay",
    "type": "GLOBAL",
    "library_path": "$escapedDll",
    "api_version": "1.4.312",
    "implementation_version": "1",
    "description": "Lymalink achievement overlay",
    "disable_environment": {
      "DISABLE_VK_LAYER_LYMALINK_overlay": "1"
    },
    "functions": {
      "vkNegotiateLoaderLayerInterfaceVersion": "LymalinkLayer_vkNegotiateLoaderLayerInterfaceVersion"
    }
  }
}
"@ | Set-Content -LiteralPath $manifest -Encoding utf8
    Set-VulkanRegistryValue $manifest $Architecture $true
}

##############################################################################

function Remove-VulkanManifest {
    param(
        [string]$InstallDirectory,
        [ValidateSet("x64", "x86")][string]$Architecture
    )

    $manifest = Join-Path $InstallDirectory "lymalink-overlay-vulkan-$Architecture.json"
    Set-VulkanRegistryValue $manifest $Architecture $false
    if (Test-Path -LiteralPath $manifest -PathType Leaf) {
        Remove-Item -LiteralPath $manifest -Force
    }
}

##############################################################################

function Deploy {
    param([string[]]$Options)

    if ($null -ne $Options -and ($Options.Count -gt 1 -or $Options[0] -ne "--debug")) {
        throw "Usage: .\build.ps1 deploy [--debug]"
    }

    $mode = if ($null -ne $Options) { "Debug" } else { "Release" }
    Build $mode

    $installDirectory = Get-WindowsInstallDirectory
    New-Item -ItemType Directory -Force -Path $installDirectory | Out-Null
    foreach ($architecture in $ARCHITECTURES) {
        $binDirectory = Join-Path (Get-BuildDirectory $mode $architecture) "bin"
        $vulkanArtifact = "lymalink-overlay-vulkan-$architecture.dll"
        $vulkanSource = Join-Path $binDirectory $vulkanArtifact
        if (Test-Path -LiteralPath $vulkanSource -PathType Leaf) {
            Copy-Item -LiteralPath $vulkanSource -Destination (Join-Path $installDirectory $vulkanArtifact) -Force
            Write-VulkanManifest $installDirectory $architecture
        }
        else {
            Remove-VulkanManifest $installDirectory $architecture
        }
    }

    Write-Host "==> Deployed overlay: $installDirectory"
}

##############################################################################

function Uninstall {
    $installDirectory = Get-WindowsInstallDirectory
    foreach ($architecture in $ARCHITECTURES) {
        Remove-VulkanManifest $installDirectory $architecture
    }

    if (Test-Path -LiteralPath $installDirectory) {
        Remove-Item -LiteralPath $installDirectory -Recurse -Force
    }

    Write-Host "==> Overlay uninstall done."
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
    default {
        Write-Host "Usage: .\build.ps1 [clean|debug|release|deploy [--debug]|uninstall]"
        exit 1
    }
}
