param([string]$Worktree='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010')
$ErrorActionPreference='Stop'
$w=[IO.Path]::GetFullPath($Worktree)
$f='D:\UnrealProjects\Offline-Game'
if((git -C $w branch --show-current).Trim() -ne 'production/gate12-koikatsu-ue583-fixture-20261010'){throw 'Wrong isolated branch'}
if((git -C $f rev-parse HEAD).Trim() -ne '3f54f07cacbf193e160d0be15a7be5a7bc730c3b' -or (git -C $f status --porcelain)){throw 'Frozen Foundation modified'}
if(@(Get-Process -Name 'UnrealEditor*' -ErrorAction SilentlyContinue).Count -gt 0){throw 'Another editor running'}
$o=Get-CimInstance Win32_OperatingSystem
if($o.FreePhysicalMemory/1024 -lt 1700 -or $o.FreeVirtualMemory/1024 -lt 3300){Write-Output 'NOT_STARTED_AVAILABLE_RESOURCES';exit 12}
$env:G12_GATE12_WORKTREE=$w
$r=Join-Path $w 'Saved\Gate12Koikatsu\NativeMotionV2\NativeActionClips'
$py=Join-Path $w 'Scripts\Gate12Koikatsu\verify_kk_native_quality_v2_cold_ue583.py'
$exe='D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$args=@(('"'+(Join-Path $w 'OfflineGame.uproject')+'"'),'-run=pythonscript',('-script="'+$py+'"'),
 '-unattended','-nullrhi','-nop4','-nosplash','-NoShaderCompile',('-abslog="'+(Join-Path $r '76_v2_cold_ue_unreal.log')+'"'))
$p=Start-Process -FilePath $exe -ArgumentList $args -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $r '76_v2_cold_ue_stdout.txt') -RedirectStandardError (Join-Path $r '76_v2_cold_ue_stderr.txt')
$t=[DateTime]::UtcNow;$state='RUNNING';$samples=@()
Write-Output "COLD_NATIVE_V2_AUDIT_PID=$($p.Id)"
try{
 while(-not $p.HasExited){
  Start-Sleep -Seconds 2;$p.Refresh();$o=Get-CimInstance Win32_OperatingSystem
  $free=[math]::Round($o.FreePhysicalMemory/1024);$commit=[math]::Round($o.FreeVirtualMemory/1024)
  $sec=[math]::Round(([DateTime]::UtcNow-$t).TotalSeconds)
  $samples+=@([pscustomobject]@{seconds=$sec;free_mb=$free;free_commit_mb=$commit})
  if($free -lt 450){$state='STOPPED_RAM';break}
  if($commit -lt 2400){$state='STOPPED_COMMIT';break}
  if($sec -gt 140){$state='STOPPED_TIMEOUT';break}
 }
 if($state -eq 'RUNNING'){$state='EXITED'}
}finally{
 if(-not $p.HasExited){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}
 $p.Refresh()
 @{status=$state;timestamp=[DateTime]::UtcNow.ToString('o');samples=$samples}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $r '76_v2_cold_ue_watchdog.json') -Encoding UTF8
 Write-Output "COLD_NATIVE_V2_PROCESS=$state"
}
$receipt=Join-Path $w 'Saved\Gate12Koikatsu\NativeMotionV2\quality_v2_cold_unreal.json'
if(!(Test-Path $receipt)){Write-Output 'UE_COLD_AUDIT_NO_RECEIPT';exit 27}
$v=Get-Content $receipt -Raw|ConvertFrom-Json
Write-Output "UE_COLD_AUDIT=$($v.status) VERIFIED=$($v.verified)"
if($v.status -ne 'PASS_COLD_UNREAL_19_PARTS_76_EXACT_SKELETON_V2_MOTION_PRE_ANDROID'){Write-Output "$($v.errors)";exit 28}
