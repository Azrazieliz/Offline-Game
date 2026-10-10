param(
 [Parameter(Mandatory=$true)][string]$SourceGlb,
 [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z][A-Za-z0-9_-]{2,63}$')][string]$CharacterId,
 [string]$Serial='R3GYC0LSC7K',
 [switch]$KeepRemoteSource
)
$ErrorActionPreference='Stop'
$w='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010'
$root='D:\UnrealProjects\Offline-Game'
$scriptDir=Join-Path $w 'Scripts\Gate12Koikatsu\PhoneCompute'
$adb='C:\Users\mimim\AppData\Local\Android\Sdk\platform-tools\adb.exe'
$ndk='C:\Users\mimim\AppData\Local\Android\Sdk\ndk\27.2.12479018\toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android26-clang++.cmd'
$python='D:\Tools\Python311\python.exe'
$work=Join-Path $w ('Saved\Gate12Koikatsu\PhoneCompute\Runs\'+$CharacterId)
New-Item -ItemType Directory -Force $work|Out-Null
$report=Join-Path $work 'phone_compute_report.json'
if((git -C $root rev-parse HEAD).Trim() -ne '3f54f07cacbf193e160d0be15a7be5a7bc730c3b'){throw 'Foundation freeze gate mismatch'}
if((git -C $w branch --show-current).Trim() -ne 'production/gate12-koikatsu-ue583-fixture-20261010'){throw 'Not isolated Gate12 branch'}
$source=[IO.Path]::GetFullPath($SourceGlb)
if(!(Test-Path -LiteralPath $source -PathType Leaf) -or [IO.Path]::GetExtension($source) -ine '.glb'){throw 'Missing source GLB'}
$sha=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
$verified=@(& $adb -s $Serial devices -l)
if(!$((@(& $adb devices -l)) -match ('^'+[regex]::Escape($Serial)+'\s+device\s+'))){throw "Phone is not connected over ADB"}
$abi=(& $adb -s $Serial shell getprop ro.product.cpu.abi).Trim()
if($abi -ne 'arm64-v8a'){throw "Unsupported phone ABI "+$abi}
& $python (Join-Path $scriptDir 'prepare_morph_workload.py') --glb $source --out (Join-Path $work 'morph_meta.txt')
if($LASTEXITCODE -ne 0){throw 'Could not build source-accessor index'}
$binary=Join-Path $work 'kk_morph_validate_arm64'
& $ndk '-O3' '-std=c++17' '-fexceptions' '-frtti' '-static-libstdc++' (Join-Path $scriptDir 'kk_morph_validate_arm64.cpp') '-o' $binary
if($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $binary)){throw 'Android NDK compile failed'}
$remote='/data/local/tmp/gate12_kk_compute_'+$CharacterId
$sourceRemote=$remote+'/export.glb'
$metaRemote=$remote+'/morph_meta.txt'
$workerRemote=$remote+'/kk_morph_validate_arm64'
try{
 & $adb -s $Serial shell "mkdir -p $remote"|Out-Null
 if($LASTEXITCODE -ne 0){throw 'Cannot create isolated Android temporary directory'}
 & $adb -s $Serial push -q $source $sourceRemote |Out-Null
 if($LASTEXITCODE -ne 0){throw 'Cannot transfer GLB to phone'}
 & $adb -s $Serial push -q (Join-Path $work 'morph_meta.txt') $metaRemote |Out-Null
 if($LASTEXITCODE -ne 0){throw 'Cannot transfer metadata'}
 & $adb -s $Serial push -q $binary $workerRemote |Out-Null
 if($LASTEXITCODE -ne 0){throw 'Cannot transfer native worker'}
 & $adb -s $Serial shell "chmod 700 $workerRemote"|Out-Null
 $phoneSha=((& $adb -s $Serial shell "sha256sum $sourceRemote") -split '\s+')[0].ToLowerInvariant()
 if($phoneSha -ne $sha){throw "Phone GLB integrity mismatch ($phoneSha vs $sha)"}
 $raw=(& $adb -s $Serial shell "$workerRemote $sourceRemote $metaRemote" 2>&1)
 $exitCode=$LASTEXITCODE
 $jsonLine=@($raw | Where-Object {$_ -match '^\{.*\}$'})|Select-Object -Last 1
 if($exitCode -ne 0 -or !$jsonLine){throw "Android execution failed RC=$exitCode OUTPUT=$($raw -join ' ')"}
 $test=$jsonLine|ConvertFrom-Json
 if($test.status -ne 'PASS_REAL_PHONE_CPU_MORPH_EVALUATION' -or $test.invalid_vertices -ne 0){throw 'Phone morph validation did not pass'}
 $index=(Get-Content (Join-Path $work 'morph_meta.json') -Raw|ConvertFrom-Json)
 if($test.primitives -ne $index.base_primitive_count -or $test.source_morph_targets -ne $index.morph_targets){throw 'Phone did not process all source morph targets'}
 $document=[ordered]@{status='PASS_PHONE_NATIVE_CPU';character_id=$CharacterId;device='Galaxy S26 Ultra';
 adb_serial=$Serial;abi=$abi;original_sha256=$sha;remote_glb_sha256=$phoneSha;
 source_glb=$source;source_bytes=(Get-Item -LiteralPath $source).Length;
 worker_sha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash;
 workload_sha256=(Get-FileHash -LiteralPath (Join-Path $work 'morph_meta.txt') -Algorithm SHA256).Hash;
 computation=$test;foundation_unchanged=$true;
 ue_cook_validated=$false;gameplay_character_validated=$false;
 time_utc=[DateTime]::UtcNow.ToString('o')}
 $document|ConvertTo-Json -Depth 8|Set-Content $report -Encoding UTF8
 "GALAXY_COMPUTE=PASS CHARACTER=$CharacterId REAL_MORPH=$($test.source_morph_targets) FRAMES=$($test.frames) CPU_MS=$($test.compute_elapsed_ms)"
 "EVIDENCE=$report"
}finally{
 # Only the temporary per-character worker directory is deleted. Original apps/data untouched.
 if(!$KeepRemoteSource){& $adb -s $Serial shell "rm -rf $remote" 2>&1|Out-Null}
}
