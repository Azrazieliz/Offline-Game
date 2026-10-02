param(
    [string]$Repository = "Azrazieliz/Offline-Game",
    [string]$RunnerDirectory = "C:\actions-runner"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Write-Host "Offline-Game Windows runner bootstrap" -ForegroundColor Cyan

$os = Get-CimInstance Win32_OperatingSystem
$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
$ramBytes = (Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory
$ramGb = [Math]::Round($ramBytes / 1GB, 1)

Write-Host ("OS: {0}" -f $os.Caption)
Write-Host ("CPU: {0} ({1} cores / {2} logical)" -f $cpu.Name, $cpu.NumberOfCores, $cpu.NumberOfLogicalProcessors)
Write-Host ("RAM: {0} GB" -f $ramGb)

if ($ramGb -lt 8) {
    throw "At least 8 GB RAM is required for this fallback runner profile."
}

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw "GitHub CLI is missing and winget is unavailable. Install GitHub CLI, then rerun."
    }

    Write-Host "Installing GitHub CLI..." -ForegroundColor Yellow
    winget install --id GitHub.cli --exact --accept-package-agreements --accept-source-agreements
}

if (-not (gh auth status 2>$null)) {
    Write-Host "GitHub authentication is required once." -ForegroundColor Yellow
    gh auth login --hostname github.com --git-protocol https --web
}

$registrationToken = gh api --method POST "repos/$Repository/actions/runners/registration-token" --jq ".token"

if (-not $registrationToken) {
    throw "Could not obtain a self-hosted runner registration token."
}

$release = gh api "repos/actions/runner/releases/latest" | ConvertFrom-Json
$asset = $release.assets | Where-Object {
    $_.name -match '^actions-runner-win-x64-.*\.zip$'
} | Select-Object -First 1

if (-not $asset) {
    throw "Could not locate the current Windows x64 GitHub Actions runner package."
}

New-Item -ItemType Directory -Force -Path $RunnerDirectory | Out-Null
$zip = Join-Path $env:TEMP $asset.name

Write-Host "Downloading GitHub Actions runner $($release.tag_name)..." -ForegroundColor Yellow
Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zip
Expand-Archive -Path $zip -DestinationPath $RunnerDirectory -Force

Push-Location $RunnerDirectory
try {
    $configArgs = @(
        "--unattended",
        "--url", "https://github.com/$Repository",
        "--token", $registrationToken,
        "--name", $env:COMPUTERNAME,
        "--labels", "unreal-5.8",
        "--work", "_work",
        "--replace"
    )

    & .\config.cmd @configArgs
    if ($LASTEXITCODE -ne 0) {
        throw "GitHub Actions runner registration failed with exit code $LASTEXITCODE."
    }

    Write-Host "Runner registered. Start it with C:\actions-runner\run.cmd for the first validation." -ForegroundColor Green
    Write-Host "Install prebuilt Unreal Engine 5.8 through Epic Games Launcher before launching the Unreal workflow." -ForegroundColor Green
} finally {
    Pop-Location
}
