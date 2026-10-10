param(
 [ValidateSet('Probe','Repair','Verify')][string]$Mode='Probe',
 [ValidateSet('face','remaining','all')][string]$Scope='face',
 [string]$ProjectRoot='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010'
)
$ErrorActionPreference='Stop'
$w=[IO.Path]::GetFullPath($ProjectRoot)
$r=Join-Path $w 'Saved\Gate12Koikatsu'
$py='D:\Tools\Python311\python.exe'
$exe='D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$guard=Join-Path $w 'Scripts\Gate12Koikatsu\preflight_kk_native_shader_host.py'
$receipt=Join-Path $r ('low_ram_native_'+$Mode.ToLowerInvariant()+'_preflight.json')
# Adaptive single-PC mode; do not confuse an editor startup threshold with cook capacity.
& $py $guard --project $w --foundation 'D:\UnrealProjects\Offline-Game' --engine 'D:\Epic Games\UE_5.8' --profile probe --receipt $receipt
if($LASTEXITCODE -ne 0){Write-Output "NOT_STARTED_CURRENT_RAM_$Mode";exit 10}
$scripts=@{
 Probe='probe_kk_face_material_native_low_ram.py'
 Repair='repair_kk_morph_shader_ownership_ue583.py'
 Verify='verify_kk_morph_shader_cold_reload_ue583.py'
}
$results=@{
 Probe='native_face_probe_low_ram.json'
 Repair='owned_morph_shader_repair_report.json'
 Verify='owned_morph_shader_cold_reload_report.json'
}
$script=Join-Path $w ('Scripts\Gate12Koikatsu\'+$scripts[$Mode])
$nativeReport=Join-Path $r $results[$Mode]
$env:G12_GATE12_WORKTREE=$w
if($Mode -eq 'Repair'){
 $env:G12_MATERIAL_REPAIR_APPROVED='1'
 $env:G12_GUARDED_LAUNCH='1'
 $env:G12_MATERIAL_SCOPE=$Scope
}
$log=Join-Path $r ('low_ram_native_'+$Mode.ToLowerInvariant()+'_unreal.log')
$out=Join-Path $r ('low_ram_native_'+$Mode.ToLowerInvariant()+'_stdout.txt')
$err=Join-Path $r ('low_ram_native_'+$Mode.ToLowerInvariant()+'_stderr.txt')
$monitor=Join-Path $r ('low_ram_native_'+$Mode.ToLowerInvariant()+'_watchdog.json')
$face=Join-Path $w 'Content\Experimental\Gate12Koikatsu\FullPrimitives\Mesh03Prim00\Mesh_03_Prim_00\Materials\KK_cf_m_face_00.uasset'
$before=(Get-FileHash $face -Algorithm SHA256).Hash
$args=@(('"'+(Join-Path $w 'OfflineGame.uproject')+'"'),'-run=pythonscript',
 ('-script="'+$script+'"'),'-unattended','-nop4','-nullrhi',
 '-nosplash','-NoShaderCompile',('-abslog="'+$log+'"'))
$start=[DateTime]::UtcNow
$p=Start-Process $exe -ArgumentList $args -PassThru -WindowStyle Hidden -RedirectStandardOutput $out -RedirectStandardError $err
Write-Output "STARTED_NATIVE_$Mode PID=$($p.Id)"
$samples=@();$state='RUNNING'
try{
 while(-not $p.HasExited){
  Start-Sleep -Seconds 2;$p.Refresh();$o=Get-CimInstance Win32_OperatingSystem
  $free=[math]::Round($o.FreePhysicalMemory/1024)
  $commit=[math]::Round($o.FreeVirtualMemory/1024)
  $elapsed=[math]::Round(([DateTime]::UtcNow-$start).TotalSeconds)
  $samples+=@([pscustomobject]@{seconds=$elapsed;physical_free_MB=$free;commit_headroom_MB=$commit})
  if($free -lt 500){$state='STOPPED_PHYSICAL_PRESSURE';break}
  if($commit -lt 2500){$state='STOPPED_COMMIT_PRESSURE';break}
  if($elapsed -gt 150){$state='STOPPED_TIMEOUT';break}
 }
 if($state -eq 'RUNNING'){$state='PROCESS_EXITED'}
}finally{
 if(-not $p.HasExited){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}
 $p.Refresh()
 $after=(Get-FileHash $face -Algorithm SHA256).Hash
 $m=[ordered]@{state=$state;mode=$Mode;started=$start.ToString('o');
  finished=[DateTime]::UtcNow.ToString('o');exit_code=if($p.HasExited){$p.ExitCode}else{$null};
  face_before_sha256=$before;face_after_sha256=$after;samples=$samples}
 $m|ConvertTo-Json -Depth 5|Set-Content $monitor -Encoding UTF8
 Write-Output "NATIVE_$Mode PROCESS=$state FACE_CHANGED=$($before -ne $after)"
}
if(!(Test-Path $nativeReport)){Write-Output "NO_NATIVE_UE_RESULT_$Mode";exit 27}
$d=Get-Content $nativeReport -Raw|ConvertFrom-Json
Write-Output "NATIVE_$Mode RECEIPT=$($d.status) SCOPE=$($d.scope) MATERIALS=$($d.expected_materials)"
if($state -ne 'PROCESS_EXITED' -or ([string]$d.status -notmatch '^PASS')){exit 28}
