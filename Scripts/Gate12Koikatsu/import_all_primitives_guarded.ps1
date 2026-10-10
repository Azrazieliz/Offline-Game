param([switch]$IncludeAlreadyImportedFullHairClothes)
$ErrorActionPreference='Stop'
$w='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010'
$r=Join-Path $w 'Saved\Gate12Koikatsu'
$manifest=Get-Content (Join-Path $r 'full_fidelity_primitive_shards\manifest_full_fidelity_primitives.json') -Raw|ConvertFrom-Json
if($manifest.status -ne 'SOURCE_LOSSLESS_SHARDS_VERIFIED'){throw 'Source manifest unverified'}
if((git -C 'D:\UnrealProjects\Offline-Game' rev-parse HEAD).Trim() -ne '3f54f07cacbf193e160d0be15a7be5a7bc730c3b'){throw 'Frozen Foundation modified'}
$report=Join-Path $r 'full_primitive_batch_report.json'
$results=[System.Collections.Generic.List[object]]::new()
$script=Join-Path $w 'Scripts\Gate12Koikatsu\import_full_primitive_guarded.ps1'
foreach($item in $manifest.shards){
 $mesh=[int]$item.mesh_index;$part=[int]$item.primitive_index
 $id=('{0:00}_{1:00}' -f $mesh,$part)
 $folder=Join-Path $w ('Content\Experimental\Gate12Koikatsu\FullPrimitives\Mesh{0:00}Prim{1:00}' -f $mesh,$part)
 $files=@(Get-ChildItem $folder -File -Recurse -ErrorAction SilentlyContinue)
 $state='NOT_TESTED'
 if($files.Count -gt 0){$state='PRESENT_ALREADY'}
 elseif(($mesh -eq 4 -or $mesh -eq 5) -and !$IncludeAlreadyImportedFullHairClothes){
  $whole=Join-Path $w ('Content\Experimental\Gate12Koikatsu\FullFidelity\Mesh{0:00}' -f $mesh)
  if(@(Get-ChildItem $whole -Recurse -File -ErrorAction SilentlyContinue).Count -gt 0){$state='PRESENT_IN_FULL_MESH_IMPORT'}
 }
 if($state -eq 'NOT_TESTED'){
  $free=(Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1024
  if($free -lt 1400){$state='BLOCKED_INSUFFICIENT_STARTING_RAM';"ABORT_NO_GPU_IMPORT_RAM_MB=$([math]::Round($free))"}
  else{
   "START_LOSSLESS_PART=$id EXPECTED_SHA=$($item.sha256)"
   & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script -MeshIndex $mesh -PrimitiveIndex $part
   $m=Join-Path $r ('fullprimitive_'+$id+'_process.json')
   if(Test-Path $m){
    $native=Get-Content $m -Raw|ConvertFrom-Json
    $state=if($native.status -eq 'EXITED' -and $native.assets -gt 0){'NATIVE_FILES_IMPORTED'}else{$native.status}
   }else{$state='COMMANDLET_FAILED_NO_REPORT'}
  }
 }
 $results.Add([pscustomobject]@{id=$id;mesh_index=$mesh;primitive_index=$part;state=$state;
  source_sha256=$item.sha256;expected_morph_targets=$item.shard_primitive_counts[0].target_count;
  imported_asset_count=@(Get-ChildItem $folder -File -Recurse -ErrorAction SilentlyContinue).Count})
 @{status='RUNNING';results=$results.ToArray();finished_at=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 6|Set-Content $report -Encoding UTF8
 "LOSSLESS_BATCH_STEP=$id STATE=$state"
 if($state -match 'BLOCKED_|STOPPED_|FAILED_'){break}
 Start-Sleep -Seconds 2
}
$all=@($results.ToArray())
$state=if($all.Count -eq $manifest.shards.Count -and @($all|Where-Object {$_.state -match 'BLOCKED_|STOPPED_|FAILED_'}).Count -eq 0){'COMPLETE'}else{'INCOMPLETE'}
@{status=$state;count=$all.Count;total=$manifest.shards.Count;results=$all;finished_at=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 6|Set-Content $report -Encoding UTF8
"FINAL_LOSSLESS_IMPORT=$state CHECKED=$($all.Count)/$($manifest.shards.Count)"
