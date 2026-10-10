param(
 [string]$ProjectRoot='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010',
 [int]$MaxSeconds=480
)
$ErrorActionPreference='Stop'
$w=[IO.Path]::GetFullPath($ProjectRoot)
$f='D:\UnrealProjects\Offline-Game'
if((git -C $w branch --show-current).Trim() -ne 'production/gate12-koikatsu-ue583-fixture-20261010'){throw 'Wrong branch'}
if((git -C $f rev-parse HEAD).Trim() -ne '3f54f07cacbf193e160d0be15a7be5a7bc730c3b' -or (git -C $f status --porcelain)){throw 'Frozen Foundation changed'}
if(@(Get-Process -Name 'UnrealEditor*' -ErrorAction SilentlyContinue).Count -ne 0){Write-Output 'UBT_BLOCKED_UNREAL_EDITOR_RUNNING';exit 12}
$o=Get-CimInstance Win32_OperatingSystem
$startFree=[math]::Round($o.FreePhysicalMemory/1024)
$startCommit=[math]::Round($o.FreeVirtualMemory/1024)
if($startFree -lt 1600 -or $startCommit -lt 3600){Write-Output "UBT_NOT_STARTED_FREE_MB=$startFree COMMIT_MB=$startCommit";exit 12}
$dotnet='D:\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe'
$ubt='D:\Epic Games\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'
$r=Join-Path $w 'Saved\Gate12Koikatsu\NativeGameplayCompiler'
New-Item -ItemType Directory -Force $r|Out-Null
$args=@(('"'+$ubt+'"'),'OfflineGameEditor','Win64','Development',('-Project="'+(Join-Path $w 'OfflineGame.uproject')+'"'),
 '-MaxParallelActions=1','-NoHotReload','-WaitMutex','-NoUBA','-NoXGE')
$out=Join-Path $r 'ubt_single_parallel_stdout.log'
$err=Join-Path $r 'ubt_single_parallel_stderr.log'
$receipt=Join-Path $r 'ubt_single_parallel_watchdog.json'
$t=[DateTime]::UtcNow
$p=Start-Process $dotnet -ArgumentList $args -PassThru -WindowStyle Hidden -RedirectStandardOutput $out -RedirectStandardError $err
Write-Output "UBT_STARTED_PID=$($p.Id) FREE_RAM_MB=$startFree FREE_COMMIT_MB=$startCommit"
$samples=@();$state='RUNNING'
try{
 while(-not $p.HasExited){
  Start-Sleep -Seconds 2;$p.Refresh();$os=Get-CimInstance Win32_OperatingSystem
  $free=[math]::Round($os.FreePhysicalMemory/1024)
  $commit=[math]::Round($os.FreeVirtualMemory/1024)
  $elapsed=[math]::Round(([DateTime]::UtcNow-$t).TotalSeconds)
  $samples+=@([pscustomobject]@{seconds=$elapsed;free_physical_mb=$free;commit_headroom_mb=$commit;ubt_rss_mb=[math]::Round($p.WorkingSet64/1MB)})
  if($elapsed%15 -lt 2){Write-Output "UBT_SECONDS=$elapsed FREE_RAM_MB=$free COMMIT_HEADROOM_MB=$commit"}
  if($free -lt 450){$state='STOPPED_RAM_PRESSURE';break}
  if($commit -lt 2400){$state='STOPPED_COMMIT_PRESSURE';break}
  if($elapsed -gt $MaxSeconds){$state='STOPPED_TIME_GUARD';break}
 }
 if($state -eq 'RUNNING'){$state='PROCESS_EXITED'}
}finally{
 if(-not $p.HasExited){& taskkill.exe /T /F /PID $p.Id |Out-Null}
 $p.Refresh()
 $exit=if($p.HasExited){$p.ExitCode}else{$null}
 $report=[ordered]@{state=$state;exit_code=$exit;started_utc=$t.ToString('o');
  completed_utc=[DateTime]::UtcNow.ToString('o');start_free_physical_mb=$startFree;
  start_commit_headroom_mb=$startCommit;max_parallel_actions=1;samples=$samples}
 $report|ConvertTo-Json -Depth 6|Set-Content $receipt -Encoding UTF8
 Write-Output "UBT_END_STATE=$state EXIT=$exit"
 if(Test-Path $out){Get-Content $out -Tail 18}
 if(Test-Path $err){Get-Content $err -Tail 10}
}
