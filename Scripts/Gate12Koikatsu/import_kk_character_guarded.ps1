param(
 [Parameter(Mandatory=$true)][string]$Manifest,
 [string]$ProjectRoot='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010',
 [string]$EngineRoot='D:\Epic Games\UE_5.8',
 [switch]$Execute
)
$ErrorActionPreference='Stop'
$mfile=[IO.Path]::GetFullPath($Manifest)
if(!(Test-Path -LiteralPath $mfile -PathType Leaf)){throw "Missing source manifest"}
$m=Get-Content -LiteralPath $mfile -Raw|ConvertFrom-Json
$id=[string]$m.character_id
if($id -cnotmatch '^[A-Za-z][A-Za-z0-9_-]{2,63}$'){throw "Unsafe character ID"}
if($m.status -ne 'SOURCE_LOSSLESS_SHARDS_VERIFIED'){throw "Unverified manifest"}
if(!($m.shards.Count -gt 0)){throw "Empty import manifest"}
$w=[IO.Path]::GetFullPath($ProjectRoot)
$exe=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project=Join-Path $w 'OfflineGame.uproject'
if(!(Test-Path $project) -or !(Test-Path $exe)){throw 'Unreal project or engine missing'}
$branch=(git -C $w branch --show-current).Trim()
if($branch -ne 'production/gate12-koikatsu-ue583-fixture-20261010'){
 throw "Only the dedicated isolated Gate 12 branch is authorized"
}
$foundation='D:\UnrealProjects\Offline-Game'
if((git -C $foundation rev-parse HEAD).Trim() -ne '3f54f07cacbf193e160d0be15a7be5a7bc730c3b'){throw 'Frozen baseline changed'}
$shardsDir=[IO.Path]::GetDirectoryName($mfile)
$fullRoot=(Resolve-Path -LiteralPath $shardsDir).Path
$qaRoot=Join-Path $w ("Saved\Gate12Koikatsu\ImportIntake\"+$id)
New-Item -ItemType Directory -Force $qaRoot|Out-Null
$seen=[System.Collections.Generic.HashSet[string]]::new()
$plan=[System.Collections.Generic.List[object]]::new()
foreach($part in $m.shards){
 $ix=[int]$part.mesh_index;$pr=[int]$part.primitive_index
 if($ix -lt 0 -or $pr -lt 0 -or $ix -gt 999 -or $pr -gt 999){throw 'Index outside supported safe range'}
 $key=('{0:000}_{1:000}' -f $ix,$pr)
 if(!$seen.Add($key)){throw "Duplicate part "+$key}
 $source=[IO.Path]::GetFullPath([string]$part.path)
 if(!([IO.Path]::GetDirectoryName($source) -ieq $fullRoot)){throw 'Import shard must reside directly in manifest directory'}
 if(!(Test-Path $source -PathType Leaf)){throw 'Missing original lossless shard'}
 $sha=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
 if($sha -ne ([string]$part.sha256).ToLowerInvariant()){throw "Hash mismatch "+$key}
 if($part.primitives_equal -ne $true -or $part.source_image_payloads_preserved -ne $true){
  throw "Source invariants failed: "+$key
 }
 $ueDest="/Game/Experimental/KoikatsuConverted/$id/Parts/Mesh$('{0:000}' -f $ix)Prim$('{0:000}' -f $pr)"
 $diskDest=Join-Path $w ('Content\Experimental\KoikatsuConverted\'+$id+'\Parts\Mesh'+('{0:000}' -f $ix)+'Prim'+('{0:000}' -f $pr))
 $plan.Add([pscustomobject]@{part=$key;source=$source;sha256=$sha;ue_destination=$ueDest;disk_destination=$diskDest;
     target_count=[int]$part.shard_primitive_counts[0].target_count;status="PENDING"})
}
$state=[ordered]@{status='PLAN_VERIFIED';character_id=$id;source_sha256=$m.original_glb_sha256;
 source_meshes=$m.full_mesh_count;source_primitives=$m.full_primitive_count;source_morph_links=$m.full_morph_links;
 planned_count=$plan.Count;stage=$plan.ToArray();frozen_foundation='3f54f07cacbf193e160d0be15a7be5a7bc730c3b';
 asset_registry_status='ENGINEERING_FIXTURE_ONLY_NOT_ARTIFACT_READY';timestamp_utc=[DateTime]::UtcNow.ToString('o')}
$stateFile=Join-Path $qaRoot 'import_stage_report.json'
$state|ConvertTo-Json -Depth 7|Set-Content $stateFile -Encoding UTF8
"KK_INTAKE_PLAN_VALID ID=$id SOURCE_PRIMITIVES=$($plan.Count) MORPH_REFERENCES=$($m.full_morph_links)"
if(!$Execute){"PLAN_ONLY_NO_UE_IMPORT_CHANGES";exit 0}
$state.status='IMPORTING'
foreach($item in $plan){
 if(Test-Path -LiteralPath $item.disk_destination){throw "Refusing to overwrite existing native assets: "+$item.disk_destination}
 $free=(Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1024
 if($free -lt 2100){$state.status='BLOCKED_STARTING_RAM';$item.status='NOT_IMPORTED_RAM_GUARD';break}
 $job=Join-Path $qaRoot ($item.part+'.importsettings.json')
 @{ImportGroups=@(@{GroupName=('KK_'+$id+'_'+$item.part);Filenames=@($item.source);
  DestinationPath=$item.ue_destination;bReplaceExisting=$false;bSkipReadOnly=$true})}|ConvertTo-Json -Depth 6|Set-Content $job -Encoding UTF8
 $log=Join-Path $qaRoot ($item.part+'.unreal.log')
 $args=@(('\"'+$project+'\"'),'-run=ImportAssets',('-importSettings=\"'+$job+'\"'),
  '-unattended','-nop4','-nullrhi','-nosplash',('-abslog=\"'+$log+'\"'))
 $p=Start-Process -FilePath $exe -ArgumentList $args -WindowStyle Hidden -PassThru
 $beg=[DateTime]::UtcNow;$result='RUNNING'
 while(!$p.HasExited){
  Start-Sleep -Seconds 5;$p.Refresh()
  $free=(Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1024
  $secs=([DateTime]::UtcNow-$beg).TotalSeconds
  if($free -lt 650){$result='STOPPED_RAM_GUARD';break}
  if($secs -gt 240){$result='STOPPED_TIME_GUARD';break}
 }
 if($result -ne 'RUNNING' -and !$p.HasExited){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}
 $p.Refresh()
 if($result -eq 'RUNNING'){$result=if($p.ExitCode -eq 0){'NATIVE_IMPORT_PROCESS_EXIT0'}else{'NATIVE_IMPORT_FAILED_EXIT_'+$p.ExitCode}}
 $item.status=$result
 $state|ConvertTo-Json -Depth 7|Set-Content $stateFile -Encoding UTF8
 "SHARD $($item.part) RESULT=$result"
 if($result -ne 'NATIVE_IMPORT_PROCESS_EXIT0'){$state.status='BLOCKED_OR_FAILED';break}
}
if(@($plan|Where-Object {$_.status -ne 'NATIVE_IMPORT_PROCESS_EXIT0'}).Count -eq 0){
 $state.status='IMPORT_PROCESSES_COMPLETE_AWAIT_UE_NATIVE_ASSET_AUDIT'
}
$state.timestamp_utc=[DateTime]::UtcNow.ToString('o')
$state|ConvertTo-Json -Depth 7|Set-Content $stateFile -Encoding UTF8
"CHARACTER_INTAKE_STATUS=$($state.status)"
