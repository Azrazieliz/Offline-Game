param(
    [string]$EngineRoot = $env:UE_ROOT,
    [string]$ArchiveDirectory = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Project = Join-Path $Root "OfflineGame.uproject"
$GateStamp = Join-Path $Root "Saved\Automation\last_unreal_gate.json"

function Invoke-Checked {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [string[]]$Arguments = @()
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE: $FilePath"
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

if (-not (Test-Path $GateStamp)) {
    throw "Android packaging is blocked until the mandatory Win64 compile + automation gate passes for this commit."
}

$Gate = Get-Content $GateStamp -Raw | ConvertFrom-Json
$Head = (& git -C $Root rev-parse HEAD).Trim()

if ($Gate.commit -ne $Head -or
    $Gate.compile -ne "passed" -or
    $Gate.automation -ne "passed") {
    throw "Android packaging gate stamp is stale or does not match HEAD $Head."
}

$EngineRoot = Find-Unreal58 $EngineRoot
$RunUAT = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"

if (-not $ArchiveDirectory) {
    $ArchiveDirectory = Join-Path $Root "Saved\AndroidBuild"
}

New-Item -ItemType Directory -Force -Path $ArchiveDirectory | Out-Null

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

Write-Host "Android package completed." -ForegroundColor Green
Write-Host "Artifacts: $ArchiveDirectory"
