param(
    [string]$SdkRoot = "D:\\Tools\\AndroidSdk"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$SourceSdk = Join-Path $env:LOCALAPPDATA "Android\\Sdk"
$SdkManager = Join-Path $SourceSdk "cmdline-tools\\latest\\bin\\sdkmanager.bat"
$SourceLicenses = Join-Path $SourceSdk "licenses"

if (-not (Test-Path $SdkManager)) {
    throw "Android SDK command-line tools were not found at $SdkManager."
}

New-Item -ItemType Directory -Force -Path $SdkRoot | Out-Null

$TargetLicenses = Join-Path $SdkRoot "licenses"
if (Test-Path $SourceLicenses) {
    New-Item -ItemType Directory -Force -Path $TargetLicenses | Out-Null
    Copy-Item (Join-Path $SourceLicenses "*") $TargetLicenses -Force
}

Write-Host "Installing UE 5.8 Android toolchain to $SdkRoot" -ForegroundColor Cyan

& $SdkManager "--sdk_root=$SdkRoot" "platform-tools" "platforms;android-35" "build-tools;35.0.1" "ndk;27.2.12479018"

if ($LASTEXITCODE -ne 0) {
    throw "sdkmanager failed with exit code ${LASTEXITCODE}."
}

$Required = @(
    "platform-tools\\adb.exe",
    "platforms\\android-35\\android.jar",
    "build-tools\\35.0.1\\aapt2.exe",
    "ndk\\27.2.12479018\\ndk-build.cmd"
)

foreach ($Relative in $Required) {
    $Path = Join-Path $SdkRoot $Relative
    if (-not (Test-Path $Path)) {
        throw "Required Android component is missing after setup: $Path"
    }
}

Write-Host "UE 5.8 Android toolchain ready." -ForegroundColor Green
Write-Host "Set OG_ANDROID_SDK_ROOT=$SdkRoot when using a non-default location."