param([int]$MaxSeconds=420)
$ErrorActionPreference='Stop'
$w='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010'
$f='D:\UnrealProjects\Offline-Game'
if((git -C $w branch --show-current).Trim() -ne 'production/gate12-koikatsu-ue583-fixture-20261010'){throw 'Wrong test branch'}
if((git -C $f rev-parse HEAD).Trim() -ne '3f54f07cacbf193e160d0be15a7be5a7bc730c3b' -or (git -C $f status --porcelain)){throw 'Frozen Foundation has changes'}
if(@(Get-Process -Name 'UnrealEditor*' -ErrorAction SilentlyContinue).Count -gt 0){Write-Output 'NO_LAUNCH_OTHER_EDITOR_RUNNING';exit 12}
$o=Get-CimInstance Win32_OperatingSystem
if($o.FreePhysicalMemory/1024 -lt 1800 -or $o.FreeVirtualMemory/1024 -lt 3300){Write-Output 'NO_LAUNCH_INSUFFICIENT_AVAILABLE_MEMORY';exit 12}
$sdk='C:\Users\mimim\AppData\Local\Android\Sdk'
$env:ANDROID_HOME=$sdk;$env:ANDROID_SDK_ROOT=$sdk
$env:JAVA_HOME='C:\Program Files\Android\Android Studio\jbr'
$env:NDKROOT=Join-Path $sdk 'ndk\27.2.12479018'
$env:NDK_ROOT=$env:NDKROOT
$r=Join-Path $w 'Saved\Gate12Koikatsu\PlayableASTC'
New-Item -ItemType Directory -Force -Path $r|Out-Null
$exe='D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$map='/Game/Experimental/Gate12Koikatsu/PlayableQA/Maps/L_KoikatsuPlayable_UE583'
$args=@(('"'+(Join-Path $w 'OfflineGame.uproject')+'"'),'-run=Cook',
 '-TargetPlatform=Android_ASTC',('-Map='+$map),'-CookMapsOnly','-iterate',
 '-unattended','-nop4','-nullrhi','-nosplash','-MaxParallelShaderJobs=1',
 ('-abslog="'+(Join-Path $r 'ue583_android_playable_map_cook.log')+'"'))
$t=[DateTime]::UtcNow
$p=Start-Process -FilePath $exe -ArgumentList $args -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $r 'stdout.log') -RedirectStandardError (Join-Path $r 'stderr.log')
Write-Output "PLAYABLE_ASTC_COOK_START_PID=$($p.Id)"
$status='RUNNING';$samples=@()
try{
 while(-not $p.HasExited){
  Start-Sleep -Seconds 3;$p.Refresh();$o=Get-CimInstance Win32_OperatingSystem
  $physical=[math]::Round($o.FreePhysicalMemory/1024)
  $commit=[math]::Round($o.FreeVirtualMemory/1024)
  $sec=[math]::Round(([DateTime]::UtcNow-$t).TotalSeconds)
  $samples+=@([pscustomobject]@{seconds=$sec;physical_free_mb=$physical;commit_headroom_mb=$commit;ue_rss_mb=[math]::Round($p.WorkingSet64/1MB)})
  if($sec%18 -lt 3){Write-Output "COOK_TIME=$sec RAM_FREE_MB=$physical COMMIT_FREE_MB=$commit"}
  if($physical -lt 450){$status='STOPPED_RAM_PRESSURE';break}
  if($commit -lt 2400){$status='STOPPED_COMMIT_PRESSURE';break}
  if($sec -gt $MaxSeconds){$status='STOPPED_TIMEOUT';break}
 }
 if($status -eq 'RUNNING'){$status='PROCESS_EXITED'}
}finally{
 if(-not $p.HasExited){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}
 $p.Refresh()
 $v=[ordered]@{status=$status;started_utc=$t.ToString('o');completed_utc=[DateTime]::UtcNow.ToString('o');
  exit_code=if($p.HasExited){$p.ExitCode}else{$null};target_platform='Android_ASTC';
  requested_map=$map;update_to_original_android_app='NOT_PERFORMED';
  device_visual_validation='NOT_PERFORMED';samples=$samples}
 $v|ConvertTo-Json -Depth 6|Set-Content (Join-Path $r 'cook_watchdog.json') -Encoding UTF8
 Write-Output "PLAYABLE_ASTC_COOK_STATUS=$status"
}
if(Test-Path (Join-Path $r 'ue583_android_playable_map_cook.log')){
 Select-String -Path (Join-Path $r 'ue583_android_playable_map_cook.log') -Pattern 'Cooked packages','Packages Cooked','LogCook: Display:','Cook complete','ERROR' | Select-Object -Last 10
}
