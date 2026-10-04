param(
    [string]$EngineRoot = "D:\\Epic Games\\UE_5.8",
    [string]$AndroidSdkRoot = $(if ($env:OG_ANDROID_SDK_ROOT) { $env:OG_ANDROID_SDK_ROOT } else { "D:\\Tools\\AndroidSdk" }),
    [string]$JavaHome = $(if ($env:OG_JAVA_HOME) { $env:OG_JAVA_HOME } elseif ($env:JAVA_HOME) { $env:JAVA_HOME } else { "" }),
    [switch]$ProbeExecutables
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$checks = [System.Collections.Generic.List[object]]::new()

function Add-Check {
    param([string]$Name, [bool]$Ok, [string]$Detail, [bool]$WarningOnly = $false)
    $checks.Add([pscustomobject]@{
        Name = $Name
        Status = $(if ($Ok) { "PASS" } elseif ($WarningOnly) { "WARN" } else { "FAIL" })
        Detail = $Detail
    })
}

function Require-Path {
    param([string]$Name, [string]$Path)
    $ok = Test-Path $Path
    Add-Check $Name $ok $Path
    return $ok
}

Write-Host "Offline-Game UE 5.8 environment readiness check" -ForegroundColor Cyan
Write-Host "This script DOES NOT compile OfflineGame, run UHT/UBT for the project, run automation tests, cook, or package." -ForegroundColor Yellow

$Editor = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
$EditorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$BuildBat = Join-Path $EngineRoot "Engine\Build\BatchFiles\Build.bat"
$UBT = Join-Path $EngineRoot "Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll"
$BuildVersion = Join-Path $EngineRoot "Engine\Build\Build.version"

Require-Path "UnrealEditor.exe" $Editor | Out-Null
Require-Path "UnrealEditor-Cmd.exe" $EditorCmd | Out-Null
Require-Path "Build.bat" $BuildBat | Out-Null
Require-Path "UnrealBuildTool.dll" $UBT | Out-Null
Require-Path "Build.version" $BuildVersion | Out-Null

if (Test-Path $BuildVersion) {
    try {
        $version = Get-Content $BuildVersion -Raw | ConvertFrom-Json
        Add-Check "UE version metadata" $true ("{0}.{1}.{2} changelist {3}" -f $version.MajorVersion,$version.MinorVersion,$version.PatchVersion,$version.Changelist)
    } catch {
        Add-Check "UE version metadata" $false $_.Exception.Message
    }
}

$launcherDb = "C:\ProgramData\Epic\UnrealEngineLauncher\LauncherInstalled.dat"
if (Test-Path $launcherDb) {
    try {
        $installed = Get-Content $launcherDb -Raw | ConvertFrom-Json
        $entry = @($installed.InstallationList | Where-Object {
            $_.InstallLocation -eq $EngineRoot -or $_.AppName -eq "UE_5.8"
        }) | Select-Object -First 1
        Add-Check "Epic registration" ($null -ne $entry) $(if ($entry) { "$($entry.AppName) -> $($entry.InstallLocation)" } else { "UE_5.8 not present in LauncherInstalled.dat; explicit EngineRoot remains usable." }) $true
    } catch {
        Add-Check "Epic registration" $false $_.Exception.Message
    }
} else {
    Add-Check "Epic registration" $false "$launcherDb missing; explicit EngineRoot remains usable." $true
}

$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    Add-Check "Visual Studio C++ workload" (-not [string]::IsNullOrWhiteSpace($vs)) $(if ($vs) { $vs } else { "No VS installation with VC x64 tools found" })
    if ($vs) {
        $cl = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find "VC\Tools\MSVC\**\bin\Hostx64\x64\cl.exe" 2>$null | Select-Object -First 1
        Add-Check "MSVC cl.exe" (-not [string]::IsNullOrWhiteSpace($cl)) $(if ($cl) { $cl } else { "cl.exe not found" })
    }
} else {
    Add-Check "vswhere.exe" $false $vswhere
}

try {
    $kitsRoot = (Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots" -Name KitsRoot10 -ErrorAction Stop).KitsRoot10
    $sdkVersions = Get-ChildItem (Join-Path $kitsRoot "Include") -Directory -ErrorAction Stop | Sort-Object Name -Descending
    $sdk = $sdkVersions | Select-Object -First 1
    $ok = $null -ne $sdk -and (Test-Path (Join-Path $kitsRoot ("Lib\" + $sdk.Name)))
    Add-Check "Windows SDK" $ok $(if ($sdk) { "$kitsRoot :: $($sdk.Name)" } else { "No Windows 10/11 SDK include version found" })
} catch {
    Add-Check "Windows SDK" $false $_.Exception.Message
}

$androidRequired = @(
    @{ Name = "adb"; Path = "platform-tools\adb.exe" },
    @{ Name = "Android API 35"; Path = "platforms\android-35\android.jar" },
    @{ Name = "Build Tools 35.0.1"; Path = "build-tools\35.0.1\aapt2.exe" },
    @{ Name = "NDK r27c"; Path = "ndk\27.2.12479018\ndk-build.cmd" }
)
foreach ($item in $androidRequired) {
    Require-Path $item.Name (Join-Path $AndroidSdkRoot $item.Path) | Out-Null
}

if ([string]::IsNullOrWhiteSpace($JavaHome)) {
    $candidates = @(
        "C:\Program Files\Android\Android Studio\jbr",
        "D:\Tools\AndroidStudio\jbr",
        "D:\Tools\Java\jbr"
    )
    $JavaHome = $candidates | Where-Object { Test-Path (Join-Path $_ "bin\java.exe") } | Select-Object -First 1
}
if (-not [string]::IsNullOrWhiteSpace($JavaHome)) {
    Require-Path "Java 21 runtime" (Join-Path $JavaHome "bin\java.exe") | Out-Null
} else {
    Add-Check "Java 21 runtime" $false "No OG_JAVA_HOME/JAVA_HOME or known JBR location"
}

if ($ProbeExecutables) {
    $RunningEditor = Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.ExecutablePath -eq $Editor } |
        Select-Object -First 1
    Add-Check "UnrealEditor executable probe" ($null -ne $RunningEditor) $(if ($RunningEditor) { "Running successfully from $Editor (PID $($RunningEditor.ProcessId))" } else { "Editor is not currently running; file presence was verified without launching a project." }) $true

    $DotNet = Get-ChildItem (Join-Path $EngineRoot "Engine\Binaries\ThirdParty\DotNet") -Recurse -Filter dotnet.exe -File -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
    if ($DotNet -and (Test-Path $UBT)) {
        $UbtText = (& $DotNet $UBT -Help 2>&1 | Out-String)
        Add-Check "UnrealBuildTool executable probe" ($UbtText -match "Global Options") $(if ($UbtText -match "Global Options") { "UBT loaded and returned its help surface." } else { $UbtText.Trim() })
    }

    $adb = Join-Path $AndroidSdkRoot "platform-tools\adb.exe"
    if (Test-Path $adb) {
        $AdbText = (& $adb version 2>&1 | Out-String)
        Add-Check "adb executable probe" ($AdbText -match "Android Debug Bridge") $AdbText.Trim()
    }

    $Aapt2 = Join-Path $AndroidSdkRoot "build-tools\35.0.1\aapt2.exe"
    if (Test-Path $Aapt2) {
        $AaptCommand = '"' + $Aapt2 + '" version 2>&1'
        $AaptText = (& cmd.exe /d /c $AaptCommand | Out-String)
        Add-Check "aapt2 executable probe" ($AaptText -match "Android Asset Packaging Tool") $AaptText.Trim()
    }

    $Clang = Join-Path $AndroidSdkRoot "ndk\27.2.12479018\toolchains\llvm\prebuilt\windows-x86_64\bin\clang.exe"
    if (Test-Path $Clang) {
        $ClangText = (& $Clang --version 2>&1 | Out-String)
        Add-Check "NDK clang executable probe" ($ClangText -match "clang version") ($ClangText -split "[\r\n]" | Select-Object -First 1)
    }

    if (-not [string]::IsNullOrWhiteSpace($JavaHome)) {
        $java = Join-Path $JavaHome "bin\java.exe"
        if (Test-Path $java) {
            $JavaCommand = '"' + $java + '" -version 2>&1'
            $JavaText = (& cmd.exe /d /c $JavaCommand | Out-String)
            Add-Check "Java executable probe" ($JavaText -match "version") ($JavaText -split "[\r\n]" | Select-Object -First 1)
        }
    }
}

$drive = Get-Item $EngineRoot -ErrorAction SilentlyContinue
if ($drive) {
    $psDrive = Get-PSDrive -Name ([System.IO.Path]::GetPathRoot($EngineRoot).Substring(0,1)) -ErrorAction SilentlyContinue
    if ($psDrive) {
        Add-Check "Engine drive free space" ($psDrive.Free -gt 20GB) ("{0:N1} GB free" -f ($psDrive.Free / 1GB))
    }
}

$checks | Format-Table -AutoSize

$failed = @($checks | Where-Object Status -eq "FAIL")
if ($failed.Count -gt 0) {
    Write-Host ("Environment readiness FAILED: {0} check(s)." -f $failed.Count) -ForegroundColor Red
    exit 1
}

Write-Host "Environment readiness PASS. OfflineGame itself has NOT been compiled or tested." -ForegroundColor Green
exit 0
