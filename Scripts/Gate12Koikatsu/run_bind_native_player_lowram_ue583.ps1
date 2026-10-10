param([int]$MaxSeconds=150)
$ErrorActionPreference='Stop'
$w='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010'
$f='D:\UnrealProjects\Offline-Game'
if((git -C $w branch --show-current).Trim() -ne 'production/gate12-koikatsu-ue583-fixture-20261010'){throw 'Wrong isolated branch'}
if((git -C $f rev-parse HEAD).Trim() -ne '3f54f07cacbf193e160d0be15a7be5a7bc730c3b' -or (git -C $f status --porcelain)){throw 'Frozen Foundation mismatch'}
$u=Join-Path $w 'Binaries\Win64\UnrealEditor-OfflineGame.dll'
$src=Join-Path $w 'Source\OfflineGame\Private\Koikatsu\OGKoikatsuPlayableCharacter.cpp'
if(!(Test-Path $u) -or ((Get-Item $u).LastWriteTimeUtc -lt (Get-Item $src).LastWriteTimeUtc)){Write-Output 'NOT_STARTED_NATIVE_EDITOR_DLL_NOT_FRESHLY_LINKED';exit 14}
if(@(Get-Process -Name 'UnrealEditor*' -ErrorAction SilentlyContinue).Count -gt 0){Write-Output 'NOT_STARTED_ANOTHER_UNREAL_EDITOR';exit 12}
$o=Get-CimInstance Win32_OperatingSystem
if(($o.FreePhysicalMemory/1024) -lt 1650 -or ($o.FreeVirtualMemory/1024) -lt 3200){Write-Output 'NOT_STARTED_MEMORY_HEADROOM';exit 12}
$r=Join-Path $w 'Saved\Gate12Koikatsu\NativeActionClips'
$py=Join-Path $w 'Scripts\Gate12Koikatsu\bind_kk_native_action_player_ue583.py'
$exe='D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$args=@(('"'+(Join-Path $w 'OfflineGame.uproject')+'"'),'-run=pythonscript',('-script="'+$py+'"'),'-unattended','-nullrhi','-nop4','-nosplash','-NoShaderCompile',('-abslog="'+(Join-Path $r 'native_player_reparent_unreal.log')+'"'))
$t=[DateTime]::UtcNow
$p=Start-Process -FilePath $exe -ArgumentList $args -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $r 'native_player_reparent_stdout.txt') -RedirectStandardError (Join-Path $r 'native_player_reparent_stderr.txt')
$status='RUNNING';$samples=@()
Write-Output "UE_PLAYER_BIND_STARTED_PID=$($p.Id)"
try{
 while(-not $p.HasExited){
  Start-Sleep -Seconds 2;$p.Refresh();$o=Get-CimInstance Win32_OperatingSystem
  $free=[math]::Round($o.FreePhysicalMemory/1024);$commit=[math]::Round($o.FreeVirtualMemory/1024)
  $sec=[math]::Round(([DateTime]::UtcNow-$t).TotalSeconds)
  $samples+=@([pscustomobject]@{seconds=$sec;physicalFreeMiB=$free;commitHeadroomMiB=$commit})
  if($free -lt 450){$status='STOP_PHYSICAL';break}
  if($commit -lt 2200){$status='STOP_COMMIT';break}
  if($sec -gt $MaxSeconds){$status='STOP_TIME';break}
 }
 if($status -eq 'RUNNING'){$status='EXITED'}
}finally{
 if(-not $p.HasExited){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}
 $p.Refresh()
 @{status=$status;startedUTC=$t.ToString('o');completedUTC=[DateTime]::UtcNow.ToString('o');dllSHA256=(Get-FileHash $u -Algorithm SHA256).Hash;samples=$samples}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $r 'native_player_reparent_watchdog.json') -Encoding UTF8
 Write-Output "UE_PLAYER_BIND_PROCESS=$status"
}
$receipt=Join-Path (Join-Path $w 'Saved\Gate12Koikatsu') 'native_action_blueprint_bind_report.json'
if(!(Test-Path $receipt)){Write-Output 'NO_NATIVE_BIND_RECEIPT';exit 27}
$data=Get-Content $receipt -Raw|ConvertFrom-Json
Write-Output "UE_PLAYER_BIND_RECEIPT=$($data.status) PARTS=$($data.component_count)"
if($status -ne 'EXITED' -or $data.status -ne 'PASS_BP_REPARENT_SERIALIZED_REQUIRES_COLD_RELOAD_ANDROID'){exit 28}
