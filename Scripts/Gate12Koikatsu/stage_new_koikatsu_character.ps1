<#
Starts CONTENT PRODUCTION now, completely independent of UE cook or Android APK.
Input: immutable genuine Koikatsu -> glTF/GLB export; requires one-skin format.
Output: per-ID lossless shard files, audit and original GLB copy, no data loss.
Never classifies a file as approved production or device validated.
#>
param(
 [Parameter(Mandatory=$true)][string]$SourceGlb,
 [Parameter(Mandatory=$true)][string]$CharacterId,
 [string]$OutputRoot='D:\Tools\GameAssetPipeline\KoikatsuParty\characters\_Gate12StagedProduction'
)
$ErrorActionPreference='Stop'
if($CharacterId -cnotmatch '^[A-Za-z][A-Za-z0-9_-]{2,63}$'){throw 'CharacterId must be stable, alphanumeric with _ or -, starting with a letter (3-64 chars)'}
$src=[IO.Path]::GetFullPath($SourceGlb)
if(!(Test-Path $src -PathType Leaf) -or [IO.Path]::GetExtension($src).ToLowerInvariant() -ne '.glb'){throw 'Input must be a real GLB file'}
$srcRoot=[IO.Path]::GetDirectoryName($src)
$output=[IO.Path]::GetFullPath($OutputRoot)
$dest=Join-Path $output $CharacterId
if(Test-Path $dest){throw 'Refusing overwrite of existing character ID. Use new ID or manually review existing staged version.'}
if($output -ieq $srcRoot -or $src.StartsWith($output+'\')){throw 'Output must be a separate original-file-safe location'}
$tool='D:\Tools\Python311\python.exe'
$builder='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Scripts\Gate12Koikatsu\build_kk_export_generic.py'
if(!(Test-Path $tool) -or !(Test-Path $builder)){throw 'Validated source converter not installed'}
$originalSha=(Get-FileHash $src -Algorithm SHA256).Hash.ToLowerInvariant()
'NEW_CHARACTER='+ $CharacterId
'ORIGINAL_SOURCE_SHA256='+ $originalSha
& $tool $builder --source-glb $src --character-id $CharacterId --output-root $output --expected-sha256 $originalSha --inspect-only
if($LASTEXITCODE -ne 0){throw 'GLB inspection failed. Source has not been modified.'}
& $tool $builder --source-glb $src --character-id $CharacterId --output-root $output --expected-sha256 $originalSha
if($LASTEXITCODE -ne 0){throw 'Lossless sharding failed; source was not modified.'}
$manifest=Join-Path $dest 'shards\manifest_full_fidelity_primitives.json'
if(!(Test-Path $manifest)){throw 'Missing output manifest'}
$m=Get-Content $manifest -Raw|ConvertFrom-Json
if($m.status -ne 'SOURCE_LOSSLESS_SHARDS_VERIFIED' -or $m.original_glb_sha256 -ne $originalSha){throw 'Original source provenance mismatch'}
$archive=Join-Path $dest ('original_'+$CharacterId+'.glb')
Copy-Item $src $archive
if((Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $originalSha){throw 'Archival source copy checksum mismatch'}
$stage=[ordered]@{
 schema='GATE12_KK_SOURCE_INTAKE_1';character_id=$CharacterId;
 original_source=$src;archived_source=$archive;source_sha256=$originalSha;
 output_shard_manifest=$manifest;
 mesh_count=$m.full_mesh_count;primitive_count=$m.full_primitive_count;
 morph_target_references=$m.full_morph_links;
 state='SOURCE_VERIFIED_ENGINEERING_STAGED_NOT_ARTIFACT_READY';
 native_unreal_import='NOT_TESTED';android_cook='NOT_TESTED';device_validation='NOT_TESTED';
 timestamp_utc=[DateTime]::UtcNow.ToString('o')
}
$stage|ConvertTo-Json -Depth 6|Set-Content (Join-Path $dest 'intake_status.json') -Encoding utf8
'SOURCE_STAGED_WITH_IMMUTABLE_ARCHIVE='+ $CharacterId
'GENUINE_SOURCE_MESHES='+ $m.full_mesh_count + ' PRIMITIVES='+ $m.full_primitive_count
'PROVENANCE='+ (Join-Path $dest 'intake_status.json')
