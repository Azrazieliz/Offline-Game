param([ValidateRange(0,5)][int]$MeshIndex)
$ErrorActionPreference='Stop'
$w='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010'
$r=Join-Path $w 'Saved\Gate12Koikatsu'
$src=Join-Path $r ('full_fidelity_shards\FullMesh_{0:00}.glb' -f $MeshIndex)
$dest='/Game/Experimental/Gate12Koikatsu/FullFidelity/Mesh{0:00}' -f $MeshIndex
$assetFolder=Join-Path $w ('Content\Experimental\Gate12Koikatsu\FullFidelity\Mesh{0:00}' -f $MeshIndex)
if(!(Test-Path $src)){throw "Missing lossless source $src"}
if(Test-Path $assetFolder){throw "Refusing overwrite of full-fidelity UE import $assetFolder"}
$meta=(Get-Content (Join-Path $r 'full_fidelity_shards\manifest_full_fidelity.json') -Raw|ConvertFrom-Json).shards[$MeshIndex]
$observed=(Get-FileHash $src -Algorithm SHA256).Hash.ToLowerInvariant()
if($observed -ne $meta.sha256){throw "Source changed since manifest"}
$settings=Join-Path $r ('fullshard_{0:00}_settings.json' -f $MeshIndex)
@{ImportGroups=@(@{GroupName=('Gate12LosslessMesh{0:00}' -f $MeshIndex);Filenames=@($src);DestinationPath=$dest;bReplaceExisting=$false;bSkipReadOnly=$true})}|ConvertTo-Json -Depth 6|Set-Content $settings -Encoding UTF8
$log=Join-Path $r ('fullshard_{0:00}_commandlet.log' -f $MeshIndex)
$out=Join-Path $r ('fullshard_{0:00}_commandlet_stdout.log' -f $MeshIndex)
$err=Join-Path $r ('fullshard_{0:00}_commandlet_stderr.log' -f $MeshIndex)
$report=Join-Path $r ('fullshard_{0:00}_process.json' -f $MeshIndex)
$exe='D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$args=@(('"'+(Join-Path $w 'OfflineGame.uproject')+'"'),'-run=ImportAssets',('-importSettings="'+$settings+'"'),'-unattended','-nop4','-nullrhi','-nosplash','-UTF8Output',('-abslog="'+$log+'"'))
$start=[DateTime]::UtcNow
$proc=Start-Process $exe -ArgumentList $args -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
"FULL_QUALITY_MESH_INDEX=$MeshIndex PID=$($proc.Id) SOURCE_SHA=$observed"
$status='RUNNING';$mem=@()
try {
 while(-not $proc.HasExited){
  Start-Sleep -Seconds 4;$proc.Refresh()
  $mb=(Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1024
  $s=([DateTime]::UtcNow-$start).TotalSeconds
  $mem+=@([pscustomobject]@{sec=[math]::Round($s);free_mb=[math]::Round($mb);editor_working_mb=[math]::Round($proc.WorkingSet64/1MB)})
  if($mb -lt 500){$status='STOPPED_WORKSTATION_RAM_SAFEGUARD';break}
  if($s -gt 220){$status='STOPPED_TIME_SAFEGUARD';break}
 }
 if($status -eq 'RUNNING'){$status='EXITED'}
}finally {
 if(-not $proc.HasExited){Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue}
 $proc.Refresh()
 $files=@(Get-ChildItem $assetFolder -Recurse -File -ErrorAction SilentlyContinue)
 $record=[ordered]@{mesh_index=$MeshIndex;source=$src;sha256=$observed;destination=$dest;status=$status;
   native_exit_code=if($proc.HasExited){$proc.ExitCode}else{$null};asset_count=$files.Count;
   asset_bytes=($files|Measure-Object Length -Sum).Sum;started_utc=$start.ToString('o');
   ended_utc=[DateTime]::UtcNow.ToString('o');ram_samples=$mem}
 $record|ConvertTo-Json -Depth 6|Set-Content $report -Encoding UTF8
 "NATIVE_RESULT=$status EXIT=$($record.native_exit_code) ASSET_COUNT=$($files.Count) ASSET_BYTES=$($record.asset_bytes) FREE_MB=$([math]::Round((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1024))"
}
