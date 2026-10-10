param(
 [ValidateRange(0,18)][int]$PartIndex=0,
 [ValidateSet('Walk','Run','Jump','Fall','Land','TurnLeft','TurnRight','Dodge','Action','ALL')][string]$Action='Walk',
 [string]$Worktree='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010',
 [int]$MaxSeconds=180
)
$ErrorActionPreference='Stop'
$w=[IO.Path]::GetFullPath($Worktree)
if(!(Test-Path (Join-Path $w 'OfflineGame.uproject'))){throw 'Unreal project missing'}
if((git -C $w branch --show-current).Trim() -ne 'production/gate12-koikatsu-ue583-fixture-20261010'){
 throw 'Refusing wrong branch'
}
$f='D:\UnrealProjects\Offline-Game'
if((git -C $f rev-parse HEAD).Trim() -ne '3f54f07cacbf193e160d0be15a7be5a7bc730c3b' -or (git -C $f status --porcelain)){
 throw 'Frozen Foundation drift'
}
$os=Get-CimInstance Win32_OperatingSystem
$startFree=[math]::Round($os.FreePhysicalMemory/1024)
$startCommit=[math]::Round($os.FreeVirtualMemory/1024)
if($startFree -lt 1750 -or $startCommit -lt 3300){
 Write-Output "NOT_STARTED_RESOURCES physical_free_MB=$startFree commit_headroom_MB=$startCommit";exit 12
}
if(@(Get-Process -Name 'UnrealEditor*' -ErrorAction SilentlyContinue).Count -gt 0){
 Write-Output 'NOT_STARTED_ANOTHER_EDITOR_RUNNING';exit 12
}
$env:G12_KK_PART_INDEX=[string]$PartIndex
$env:G12_KK_ACTION=$Action
$env:G12_GATE12_WORKTREE=$w
$r=Join-Path $w 'Saved\Gate12Koikatsu\NativeActionClips'
New-Item -ItemType Directory -Force -Path $r|Out-Null
$key=('{0:00}_{1}' -f $PartIndex,$Action)
$exe='D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$py=Join-Path $w 'Scripts\Gate12Koikatsu\author_kk_incremental_native_actions_ue583.py'
$log=Join-Path $r ($key+'_unreal.log')
$out=Join-Path $r ($key+'_stdout.txt')
$err=Join-Path $r ($key+'_stderr.txt')
$report=Join-Path $r ($key+'_watchdog.json')
$args=@(('"'+(Join-Path $w 'OfflineGame.uproject')+'"'),'-run=pythonscript',('-script="'+$py+'"'),
 '-unattended','-nop4','-nullrhi','-nosplash','-NoShaderCompile',('-abslog="'+$log+'"'))
$t=[DateTime]::UtcNow
$p=Start-Process $exe -ArgumentList $args -PassThru -WindowStyle Hidden -RedirectStandardOutput $out -RedirectStandardError $err
Write-Output "STARTED_NATIVE_ACTION_AUTHORING pid=$($p.Id) part=$PartIndex action=$Action freeRAM_MB=$startFree freeCommit_MB=$startCommit"
$status='RUNNING';$samples=@()
try{
 while(-not $p.HasExited){
  Start-Sleep -Seconds 2;$p.Refresh();$o=Get-CimInstance Win32_OperatingSystem
  $free=[math]::Round($o.FreePhysicalMemory/1024)
  $commit=[math]::Round($o.FreeVirtualMemory/1024)
  $elapsed=[math]::Round(([DateTime]::UtcNow-$t).TotalSeconds)
  $samples+=@([pscustomobject]@{seconds=$elapsed;freeRAM_MB=$free;freeCommit_MB=$commit;RSS_MB=[math]::Round($p.WorkingSet64/1MB)})
  if($elapsed%15 -lt 2){Write-Output "NATIVE_ACTION_SECONDS=$elapsed FREE_RAM_MB=$free FREE_COMMIT_MB=$commit"}
  if($free -lt 500){$status='STOPPED_FREE_RAM';break}
  if($commit -lt 2500){$status='STOPPED_COMMIT_HEADROOM';break}
  if($elapsed -gt $MaxSeconds){$status='STOPPED_TIME_GUARD';break}
 }
 if($status -eq 'RUNNING'){$status='PROCESS_EXITED'}
}finally{
 if(-not $p.HasExited){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}
 $p.Refresh()
 $obj=[ordered]@{status=$status;part_index=$PartIndex;action=$Action;
   started_utc=$t.ToString('o');completed_utc=[DateTime]::UtcNow.ToString('o');
   exit_code=if($p.HasExited){$p.ExitCode}else{$null};samples=$samples}
 $obj|ConvertTo-Json -Depth 5|Set-Content $report -Encoding UTF8
 Write-Output "NATIVE_ACTION_END=$status EXIT=$($obj.exit_code)"
 if(Test-Path $log){Get-Content $log -Tail 5}
}
$native=Join-Path $r ("native_action_part_{0:00}_{1}.json" -f $PartIndex,$Action)
if(!(Test-Path $native)){Write-Output "NATIVE_ACTION_MISSING_UE_RECEIPT";exit 27}
$actual=Get-Content $native -Raw|ConvertFrom-Json
Write-Output "NATIVE_ACTION_RECEIPT=$($actual.status) COUNT=$(@($actual.action_assets).Count)"
if($status -ne "PROCESS_EXITED" -or $actual.status -ne "PASS_NATIVE_ACTION_SEQUENCE_ASSET_AUTHORING"){
 exit 28
}
