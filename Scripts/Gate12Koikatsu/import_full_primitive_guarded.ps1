param([ValidateRange(0,5)][int]$MeshIndex,[ValidateRange(0,11)][int]$PrimitiveIndex)
$ErrorActionPreference='Stop'
$w='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010'
$r=Join-Path $w 'Saved\Gate12Koikatsu'
$src=Join-Path $r ('full_fidelity_primitive_shards\Mesh_{0:00}_Prim_{1:00}.glb' -f $MeshIndex,$PrimitiveIndex)
$dest=('/Game/Experimental/Gate12Koikatsu/FullPrimitives/Mesh{0:00}Prim{1:00}' -f $MeshIndex,$PrimitiveIndex)
$assetFolder=Join-Path $w ('Content\Experimental\Gate12Koikatsu\FullPrimitives\Mesh{0:00}Prim{1:00}' -f $MeshIndex,$PrimitiveIndex)
if(!(Test-Path $src)){throw "Missing lossless source $src"}
if(Test-Path $assetFolder){throw "Refusing overwrite of full-fidelity import $assetFolder"}
$manifest=Get-Content (Join-Path $r 'full_fidelity_primitive_shards\manifest_full_fidelity_primitives.json') -Raw|ConvertFrom-Json
$meta=@($manifest.shards|Where-Object {$_.mesh_index -eq $MeshIndex -and $_.primitive_index -eq $PrimitiveIndex})
if($meta.Count -ne 1){throw "Source manifest missing / ambiguous for mesh $MeshIndex prim $PrimitiveIndex"}
$hash=(Get-FileHash $src -Algorithm SHA256).Hash.ToLowerInvariant()
if($hash -ne $meta[0].sha256){throw "Source hash mismatch"}
$id=('{0:00}_{1:00}' -f $MeshIndex,$PrimitiveIndex)
$settings=Join-Path $r ('fullprimitive_'+$id+'_settings.json')
@{ImportGroups=@(@{GroupName=('Gate12LosslessPrimitive'+$id);Filenames=@($src);DestinationPath=$dest;bReplaceExisting=$false;bSkipReadOnly=$true})}|ConvertTo-Json -Depth 6|Set-Content $settings -Encoding UTF8
$log=Join-Path $r ('fullprimitive_'+$id+'_commandlet.log')
$out=Join-Path $r ('fullprimitive_'+$id+'_stdout.log')
$err=Join-Path $r ('fullprimitive_'+$id+'_stderr.log')
$report=Join-Path $r ('fullprimitive_'+$id+'_process.json')
$exe='D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$args=@(('"'+(Join-Path $w 'OfflineGame.uproject')+'"'),'-run=ImportAssets',('-importSettings="'+$settings+'"'),'-unattended','-nop4','-nullrhi','-nosplash','-UTF8Output',('-abslog="'+$log+'"'))
$start=[DateTime]::UtcNow
$proc=Start-Process $exe -ArgumentList $args -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
"FULL_FIDELITY_INDEX=$id PID=$($proc.Id) SOURCE_SHA=$hash"
$status='RUNNING';$samples=@()
try {
 while(-not $proc.HasExited){
  Start-Sleep -Seconds 4;$proc.Refresh()
  $free=(Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1024
  $elapsed=([DateTime]::UtcNow-$start).TotalSeconds
  $samples+=@([pscustomobject]@{seconds=[math]::Round($elapsed);free_ram_mb=[math]::Round($free);unreal_rss_mb=[math]::Round($proc.WorkingSet64/1MB)})
  if($free -lt 500){$status='STOPPED_RAM_GUARD';break}
  if($elapsed -gt 190){$status='STOPPED_TIME_GUARD';break}
 }
 if($status -eq 'RUNNING'){$status='EXITED'}
}finally {
 if(-not $proc.HasExited){Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue}
 $proc.Refresh()
 $files=@(Get-ChildItem $assetFolder -Recurse -File -ErrorAction SilentlyContinue)
 $v=[ordered]@{mesh_index=$MeshIndex;primitive_index=$PrimitiveIndex;status=$status;src_sha256=$hash;
  exit_code=if($proc.HasExited){$proc.ExitCode}else{$null};assets=$files.Count;
  asset_bytes=($files|Measure-Object Length -Sum).Sum;start_utc=$start.ToString('o');end_utc=[DateTime]::UtcNow.ToString('o');samples=$samples}
 $v|ConvertTo-Json -Depth 5|Set-Content $report -Encoding UTF8
 "RESULT=$status ASSETS=$($files.Count) ASSET_BYTES=$($v.asset_bytes) FREE_RAM_MB=$([math]::Round((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1024))"
}
