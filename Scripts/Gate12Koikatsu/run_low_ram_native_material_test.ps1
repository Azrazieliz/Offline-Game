param(
 [ValidateSet('Probe','Repair')][string]$Mode='Probe',
 [string]$ProjectRoot='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010',
 [string]$FoundationRoot='D:\UnrealProjects\Offline-Game',
 [string]$EngineRoot='D:\Epic Games\UE_5.8'
)
$ErrorActionPreference='Stop'
$w=[IO.Path]::GetFullPath($ProjectRoot)
$r=Join-Path $w 'Saved\Gate12Koikatsu'
$py='D:\Tools\Python311\python.exe'
$guard=Join-Path $w 'Scripts\Gate12Koikatsu\preflight_kk_native_shader_host.py'
$exe=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$profile=if($Mode -eq 'Repair'){'repair'}else{'probe'}
$receipt=Join-Path $r ('low_ram_native_'+$Mode.ToLowerInvariant()+'_preflight.json')
& $py $guard --project $w --foundation $FoundationRoot --engine $EngineRoot --profile $profile --receipt $receipt
if($LASTEXITCODE -ne 0){Write-Output "BLOCKED_PRELAUNCH_PROFILE_$profile";exit 10}
if($Mode -eq 'Repair'){
 $script=Join-Path $w 'Scripts\Gate12Koikatsu\repair_kk_morph_shader_ownership_ue583.py'
 $env:G12_MATERIAL_REPAIR_APPROVED='1'
 $env:G12_GUARDED_LAUNCH='1'
 $env:G12_MATERIAL_SCOPE='face'
}else{
 $script=Join-Path $w 'Scripts\Gate12Koikatsu\probe_kk_face_material_native_low_ram.py'
}
$env:G12_GATE12_WORKTREE=$w
$log=Join-Path $r ('low_ram_native_'+$Mode.ToLowerInvariant()+'_unreal.log')
$stdout=Join-Path $r ('low_ram_native_'+$Mode.ToLowerInvariant()+'_stdout.txt')
$stderr=Join-Path $r ('low_ram_native_'+$Mode.ToLowerInvariant()+'_stderr.txt')
$resultFile=Join-Path $r ('low_ram_native_'+$Mode.ToLowerInvariant()+'_watchdog.json')
$face=Join-Path $w 'Content\Experimental\Gate12Koikatsu\FullPrimitives\Mesh03Prim00\Mesh_03_Prim_00\Materials\KK_cf_m_face_00.uasset'
$before=(Get-FileHash $face -Algorithm SHA256).Hash
$args=@(('"'+(Join-Path $w 'OfflineGame.uproject')+'"'),'-run=pythonscript',('-script="'+$script+'"'),
 '-unattended','-nop4','-nullrhi','-nosplash',('-abslog="'+$log+'"'))
$start=[DateTime]::UtcNow
$proc=Start-Process $exe -ArgumentList $args -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
Write-Output "NATIVE_$Mode STARTED_PID=$($proc.Id)"
$state='RUNNING';$samples=@()
try{
 while(-not $proc.HasExited){
  Start-Sleep -Seconds 2
  $proc.Refresh()
  $freeMb=[math]::Round((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1024)
  $seconds=[math]::Round(([DateTime]::UtcNow-$start).TotalSeconds)
  $samples+=@([pscustomobject]@{seconds=$seconds;free_ram_mb=$freeMb;unreal_rss_mb=[math]::Round($proc.WorkingSet64/1MB)})
  if($freeMb -lt 850){$state='STOPPED_RAM_GUARD';break}
  if($seconds -gt 160){$state='STOPPED_TIME_GUARD';break}
 }
 if($state -eq 'RUNNING'){$state='EXITED'}
}finally{
 if(-not $proc.HasExited){Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue;Start-Sleep -Milliseconds 250}
 $proc.Refresh()
 $after=(Get-FileHash $face -Algorithm SHA256).Hash
 $record=[ordered]@{status=$state;mode=$Mode;started_utc=$start.ToString('o');
 finished_utc=[DateTime]::UtcNow.ToString('o');exit_code=if($proc.HasExited){$proc.ExitCode}else{$null};
 original_face_sha256=$before;current_face_sha256=$after;face_asset_changed=($before -ne $after);
 native_editor_executed=$true;samples=$samples}
 $record|ConvertTo-Json -Depth 5|Set-Content $resultFile -Encoding UTF8
 Write-Output ("NATIVE_{0} STATE={1} CODE={2} FACE_CHANGED={3}" -f $Mode,$state,$record.exit_code,$record.face_asset_changed)
}
