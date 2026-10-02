param(
    [string]$EngineRoot = $env:UE_ROOT
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Project = Join-Path $Root "OfflineGame.uproject"
$ReportDir = Join-Path $Root "Saved\Automation\Reports"
$BuildConfigDir = Join-Path $Root "Saved\UnrealBuildTool"
$BuildConfig = Join-Path $BuildConfigDir "BuildConfiguration.xml"
$GateStamp = Join-Path $Root "Saved\Automation\last_unreal_gate.json"

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

    if ($Preferred) {
        $candidates.Add($Preferred)
    }

    $candidates.Add("C:\Program Files\Epic Games\UE_5.8")
    $candidates.Add("C:\Epic Games\UE_5.8")

    foreach ($drive in Get-PSDrive -PSProvider FileSystem) {
        $root = $drive.Root
        if (-not $root) { continue }

        $candidates.Add((Join-Path $root "Epic Games\UE_5.8"))
        $candidates.Add((Join-Path $root "UE_5.8"))
        $candidates.Add((Join-Path $root "Unreal\UE_5.8"))
    }

    foreach ($candidate in $candidates | Select-Object -Unique) {
        $buildBat = Join-Path $candidate "Engine\Build\BatchFiles\Build.bat"
        $editorCmd = Join-Path $candidate "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

        if ((Test-Path $buildBat) -and (Test-Path $editorCmd)) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "Unreal Engine 5.8 was not found. Install the prebuilt UE 5.8 engine with Epic Games Launcher, preferably on the 512 GB storage device, then rerun. You may also set UE_ROOT."
}

Write-Host "=== Mandatory Unreal C++ preflight ===" -ForegroundColor Cyan

$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    $python = Get-Command py -ErrorAction SilentlyContinue
}

if (-not $python) {
    throw "Python is required for Scripts\verify_unreal_cpp.py."
}

$preflight = Join-Path $Root "Scripts\verify_unreal_cpp.py"

if ($python.Name -eq "py.exe") {
    Invoke-Checked -FilePath $python.Source -Arguments @("-3", $preflight)
} else {
    Invoke-Checked -FilePath $python.Source -Arguments @($preflight)
}

$EngineRoot = Find-Unreal58 $EngineRoot
$BuildBat = Join-Path $EngineRoot "Engine\Build\BatchFiles\Build.bat"
$EditorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

Write-Host "Using Unreal Engine: $EngineRoot" -ForegroundColor Green

New-Item -ItemType Directory -Force -Path $BuildConfigDir | Out-Null

$xml = @'
<?xml version="1.0" encoding="utf-8"?>
<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">
  <BuildConfiguration>
    <MaxParallelActions>1</MaxParallelActions>
    <bUseUnityBuild>true</bUseUnityBuild>
    <bAllowUBAExecutor>false</bAllowUBAExecutor>
    <bAllowUBALocalExecutor>false</bAllowUBALocalExecutor>
    <bCompactOutput>true</bCompactOutput>
  </BuildConfiguration>
</Configuration>
'@

$xml | Set-Content -Path $BuildConfig -Encoding UTF8

New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
Get-ChildItem -Path $ReportDir -Force -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force
Remove-Item -Path $GateStamp -Force -ErrorAction SilentlyContinue

Write-Host "=== Unreal project compile (Win64 Development Editor) ===" -ForegroundColor Cyan

$buildArgs = @(
    "OfflineGameEditor",
    "Win64",
    "Development",
    $Project,
    "-WaitMutex",
    "-NoHotReloadFromIDE"
)

Invoke-Checked -FilePath $BuildBat -Arguments $buildArgs

Write-Host "=== OfflineGame automation tests ===" -ForegroundColor Cyan

$testArgs = @(
    $Project,
    "-unattended",
    "-nop4",
    "-nosplash",
    "-nosound",
    "-NullRHI",
    "-stdout",
    "-FullStdOutLogOutput",
    "-ExecCmds=Automation RunTest OfflineGame;Quit",
    "-TestExit=Automation Test Queue Empty",
    "-ReportExportPath=$ReportDir"
)

Invoke-Checked -FilePath $EditorCmd -Arguments $testArgs

$Head = (& git -C $Root rev-parse HEAD).Trim()
$GateRecord = [ordered]@{
    commit = $Head
    engine_root = $EngineRoot
    completed_utc = [DateTime]::UtcNow.ToString("o")
    compile = "passed"
    automation = "passed"
}
$GateRecord | ConvertTo-Json | Set-Content -Path $GateStamp -Encoding UTF8

Write-Host "Unreal compile + OfflineGame automation tests completed." -ForegroundColor Green
Write-Host "Reports: $ReportDir"
Write-Host "Gate stamp: $GateStamp"
