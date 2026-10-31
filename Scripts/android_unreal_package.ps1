param(
    [string]$EngineRoot = $env:UE_ROOT,
    [string]$ArchiveDirectory = "",
    [switch]$AndroidFirst,
    [switch]$InstallOnDevice
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Project = Join-Path $Root "OfflineGame.uproject"
$GateStamp = Join-Path $Root "Saved\Automation\last_unreal_gate.json"
$PreferredSdkRoot = if ($env:OG_ANDROID_SDK_ROOT) {
    $env:OG_ANDROID_SDK_ROOT
} else {
    "D:\Tools\AndroidSdk"
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [string[]]$Arguments = @()
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath"
    }
}

function Find-Unreal58 {
    param([string]$Preferred)

    $candidates = New-Object System.Collections.Generic.List[string]
    if ($Preferred) { $candidates.Add($Preferred) }

    foreach ($drive in Get-PSDrive -PSProvider FileSystem) {
        if (-not $drive.Root) { continue }
        $candidates.Add((Join-Path $drive.Root "Epic Games\UE_5.8"))
        $candidates.Add((Join-Path $drive.Root "UE_5.8"))
        $candidates.Add((Join-Path $drive.Root "Unreal\UE_5.8"))
    }

    foreach ($candidate in $candidates | Select-Object -Unique) {
        $runUat = Join-Path $candidate "Engine\Build\BatchFiles\RunUAT.bat"
        if (Test-Path $runUat) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "Unreal Engine 5.8 with RunUAT.bat was not found."
}

# Android-first is an explicit developer build path, not a declaration that
# Win64 Editor automation passed. BuildCookRun still compiles, cooks, stages
# and packages real Unreal game code and source data before any installation.
if (-not $AndroidFirst) {
    if (-not (Test-Path $GateStamp)) {
        throw "Android packaging requires the Win64 compile/automation gate, or explicit -AndroidFirst for a native device-validation build."
    }
    $Gate = Get-Content $GateStamp -Raw | ConvertFrom-Json
    $Head = (& git -C $Root rev-parse HEAD).Trim()
    if ($Gate.commit -ne $Head -or
        $Gate.compile -ne "passed" -or
        $Gate.automation -ne "passed") {
        throw "Android packaging gate stamp is stale or does not match HEAD $Head."
    }
} else {
    Write-Host "Android-first build: Win64 automation is NOT marked passed." -ForegroundColor Yellow
}

$EngineRoot = Find-Unreal58 $EngineRoot
$RunUAT = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"

if (Test-Path $PreferredSdkRoot) {
    $env:ANDROID_HOME = $PreferredSdkRoot
    $env:ANDROID_SDK_ROOT = $PreferredSdkRoot

    $NdkRoot = Join-Path $PreferredSdkRoot "ndk\27.2.12479018"
    if (Test-Path $NdkRoot) {
        $env:NDKROOT = $NdkRoot
    }
}

$AndroidStudioJdk = "C:\Program Files\Android\Android Studio\jbr"
if (Test-Path $AndroidStudioJdk) {
    $env:JAVA_HOME = $AndroidStudioJdk
}

if (-not $ArchiveDirectory) {
    $ArchiveDirectory = Join-Path $Root "Saved\AndroidBuild"
}

New-Item -ItemType Directory -Force -Path $ArchiveDirectory | Out-Null
# Mandatory low-memory compilation and caches remain on the D: worktree.
$HostScratch = Join-Path $Root "Saved\HostBuildScratch"
New-Item -ItemType Directory -Force -Path $HostScratch | Out-Null
$env:TEMP = $HostScratch
$env:TMP = $HostScratch
$env:UE_LocalDataCachePath = Join-Path $Root "Saved\DerivedData\Local"
$env:UE_SharedDataCachePath = Join-Path $Root "Saved\DerivedData\Shared"
$env:OG_LOW_MEMORY_BUILD = "1"

Write-Host "=== Android SDK verification ===" -ForegroundColor Cyan
Invoke-Checked -FilePath $RunUAT -Arguments @(
    "Turnkey",
    "-command=VerifySdk",
    "-platform=Android",
    "-utf8output"
)

Write-Host "=== Android Development package ===" -ForegroundColor Cyan
Invoke-Checked -FilePath $RunUAT -Arguments @(
    "BuildCookRun",
    "-project=$Project",
    "-noP4",
    "-platform=Android",
    "-clientconfig=Development",
    "-build",
    "-cook",
    "-stage",
    "-package",
    "-archive",
    "-archivedirectory=$ArchiveDirectory",
    "-utf8output"
)

# A successful RunUAT exit is necessary but never sufficient: verify a real
# current generated APK before offering or attempting to install it.
$Artifact = Get-ChildItem -LiteralPath $ArchiveDirectory -Recurse -Filter "*.apk" -File |
    Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if (-not $Artifact -or $Artifact.Length -lt 1000000) {
    throw "RunUAT exited but no valid packaged Android APK was found."
}
Write-Host "Android package completed." -ForegroundColor Green
Write-Host "APK: $($Artifact.FullName)" -ForegroundColor Green
Write-Host "APK size (bytes): $($Artifact.Length)"

if ($InstallOnDevice) {
    $Adb = Join-Path $PreferredSdkRoot "platform-tools\adb.exe"
    if (-not (Test-Path $Adb)) { throw "ADB missing from configured SDK." }
    $Connected = @(& $Adb devices | Where-Object { $_ -match '^\S+\s+device\s*$' })
    if ($Connected.Count -ne 1) {
        throw "Expected exactly one authorized Android device; got $($Connected.Count)."
    }
    Invoke-Checked -FilePath $Adb -Arguments @("install", "-r", $Artifact.FullName)
    Write-Host "Native APK installed on the connected Android device." -ForegroundColor Green
    Write-Host "Owner gameplay acceptance remains outstanding." -ForegroundColor Yellow
}
