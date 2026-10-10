param(
 [string]$ProjectRoot='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010',
 [string]$CharacterId='KK_TEST_0002',
 [string]$Version='v0001'
)
$ErrorActionPreference='Stop'
if($CharacterId -cnotmatch '^[A-Za-z][A-Za-z0-9_-]{2,63}$' -or $Version -cnotmatch '^v[0-9]{4,8}$'){throw 'Unsafe character or version identifier'}
$w=[IO.Path]::GetFullPath($ProjectRoot)
if((git -C $w branch --show-current).Trim() -ne 'production/gate12-koikatsu-ue583-fixture-20261010'){throw 'Not isolated test branch'}
if((git -C 'D:\UnrealProjects\Offline-Game' rev-parse HEAD).Trim() -ne '3f54f07cacbf193e160d0be15a7be5a7bc730c3b'){throw 'Frozen foundation changed'}
$root=Join-Path $w 'Saved\Cooked\Android_ASTC\OfflineGame\Content\Experimental\Gate12Koikatsu'
if(!(Test-Path $root)){throw 'Cooked character files missing; cook first'}
$paktool='D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealPak.exe'
if(!(Test-Path $paktool)){throw 'UE 5.8 UnrealPak not found'}
$dest=Join-Path $w ('Saved\Gate12Koikatsu\ContentBundles\'+$CharacterId+'\'+$Version)
New-Item -ItemType Directory -Force $dest|Out-Null
$pak=Join-Path $dest ('OG_KK_'+$CharacterId+'_'+$Version+'_Android_ASTC.pak')
$manifest=Join-Path $dest 'manifest.json'
$paklist=Join-Path $dest 'unrealpak_response.txt'
if((Test-Path $pak) -or (Test-Path $manifest)){throw 'Refusing to overwrite an existing versioned bundle'}
$items=@(Get-ChildItem $root -Recurse -File |Where-Object {$_.Extension -in '.uasset','.uexp','.ubulk','.uptnl'})
if($items.Count -lt 20){throw 'Too few cooked assets for full fixture'}
$entries=[System.Collections.Generic.List[object]]::new()
$mapping=[System.Collections.Generic.List[string]]::new()
foreach($file in ($items|Sort-Object FullName)){
 $relative=$file.FullName.Substring($root.Length+1).Replace('\','/')
 if($relative.Contains('..') -or $relative.StartsWith('/')){throw 'Unsafe cooked relative path'}
 $mount='../../../OfflineGame/Content/Experimental/Gate12Koikatsu/'+$relative
 $mapping.Add(('"{0}" "{1}"' -f $file.FullName,$mount))
 $entries.Add([pscustomobject]@{file=$relative;size=$file.Length;sha256=(Get-FileHash $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()})
}
[IO.File]::WriteAllLines($paklist,$mapping.ToArray(),[Text.UTF8Encoding]::new($false))
$out=Join-Path $dest 'build_stdout.txt';$err=Join-Path $dest 'build_stderr.txt'
$argv=@(('"' + $pak + '"'),('-Create="' + $paklist + '"'),'-compress','-compressionformats=Zlib')
$p=Start-Process $paktool -ArgumentList $argv -PassThru -Wait -NoNewWindow -RedirectStandardOutput $out -RedirectStandardError $err
if($p.ExitCode -ne 0 -or !(Test-Path $pak)){throw "UnrealPak failed exit=$($p.ExitCode): "+((Get-Content $err -Tail 12)-join ';')}
$bytes=(Get-Item $pak).Length
$report=[ordered]@{schema='OG_KK_COOKED_BUNDLE_V1';status='PAK_BUILT_NOT_RUNTIME_MOUNT_VALIDATED';
 character_id=$CharacterId;version=$Version;target_platform='Android_ASTC';
 engine='UE_5.8';source_cook_root=$root;
 bundle_file=(Split-Path $pak -Leaf);bundle_sha256=(Get-FileHash $pak -Algorithm SHA256).Hash.ToLowerInvariant();
 bundle_bytes=$bytes;cooked_input_bytes=(($items|Measure-Object Length -Sum).Sum);
 cooked_file_count=$items.Count;entries=$entries.ToArray();
 loader_mount_required=$true;registry_status='EXPERIMENTAL_FIXTURE_NOT_ARTIFACT_READY';
 creation_utc=[DateTime]::UtcNow.ToString('o')}
$report|ConvertTo-Json -Depth 7|Set-Content $manifest -Encoding UTF8
"PAK_BUILT=$pak INPUT_FILES=$($items.Count) INPUT_BYTES=$($report.cooked_input_bytes) PAK_BYTES=$bytes SHA256=$($report.bundle_sha256)"
& $paktool ('"' + $pak + '"') -List 2>&1 |Out-File (Join-Path $dest 'pak_list_output.txt') -Encoding utf8
"LIST_RC=$LASTEXITCODE"
if($LASTEXITCODE -ne 0){throw 'UnrealPak list verification failed'}
$cnt=@(Get-Content (Join-Path $dest 'pak_list_output.txt')|Where-Object {$_ -like '*offset:*'}).Count
if($cnt -ne $items.Count){throw "Pak listing mismatch: expected $($items.Count), listed $cnt"}
"PAK_LISTED_FILES=$cnt"
