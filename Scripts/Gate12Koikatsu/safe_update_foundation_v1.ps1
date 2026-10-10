<#
Gate 12: reversible signed in-place Android update for the ORIGINAL Foundation v1 app.
- DRY RUN by default.
- Requires a NEW externally produced and signed true PLAYABLE APK.
- Never overwrites frozen Foundation or original phone app data with another app package.
- Do not call with an experimental com.azrazieliz.gate12fixture APK.
#>
param(
 [Parameter(Mandatory=$true)][string]$UpdateApk,
 [string]$Serial='R3GYC0LSC7K',
 [switch]$Apply
)
$ErrorActionPreference='Stop'
$w='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010'
$f='D:\UnrealProjects\Offline-Game'
$proof=Join-Path $w 'Saved\Gate12Koikatsu\FoundationV1InPlaceUpdate_Safeguards'
$adb='C:\Users\mimim\AppData\Local\Android\Sdk\platform-tools\adb.exe'
$aapt='C:\Users\mimim\AppData\Local\Android\Sdk\build-tools\35.0.1\aapt.exe'
$signer='C:\Users\mimim\AppData\Local\Android\Sdk\build-tools\35.0.1\apksigner.bat'
$env:JAVA_HOME='C:\Program Files\Android\Android Studio\jbr'
$pkg='com.azrazieliz.OfflineGame'
$expectedBase='3f54f07cacbf193e160d0be15a7be5a7bc730c3b'
$expectedCert='9bc439712a0a460cc4703ebf254aaaf9bf3cc6e67b0ced6fc12162ed3ca3b9ad'
if((git -C $f rev-parse HEAD).Trim() -ne $expectedBase -or @((git -C $f status --porcelain)).Count -ne 0){throw 'Frozen Foundation HEAD/tracked working state not clean'}
if((git -C $w branch --show-current).Trim() -ne 'production/gate12-koikatsu-ue583-fixture-20261010'){throw 'Wrong isolated branch'}
$src=[IO.Path]::GetFullPath($UpdateApk)
if(!(Test-Path -LiteralPath $src -PathType Leaf) -or [IO.Path]::GetExtension($src) -ne '.apk'){throw 'Supply a real signed .apk file'}
$installed=((& $adb -s $Serial shell pm path $pkg)-join ' ').Trim()
if(!$installed.StartsWith('package:/')){throw 'Foundation app not installed; refuse fresh install'}
$badging=(& $aapt dump badging $src|Select-String '^package:')|Select-Object -First 1
if(!$badging -or $badging.Line -notmatch ("name='"+[regex]::Escape($pkg)+"'")){throw "This is not a Foundation in-place update APK"}
$certLines=& $signer verify --print-certs $src 2>&1
if($LASTEXITCODE -ne 0){throw 'APK signature verification failed'}
$certLine=($certLines|Select-String 'Signer #1 certificate SHA-256 digest'|Select-Object -First 1).Line
if(!$certLine -or $certLine.ToLowerInvariant() -notmatch $expectedCert){throw 'APK was not signed with Foundation app certificate'}
$baseCopy=Join-Path $proof 'CURRENT_INSTALLED_FOUNDATION_V1__BACKUP.apk'
if(!(Test-Path $baseCopy)){throw 'Make a verified installed-base APK backup before updating'}
$baseCertLines=& $signer verify --print-certs $baseCopy 2>&1
if($LASTEXITCODE -ne 0 -or !(($baseCertLines|Select-String $expectedCert))){throw 'Installed-base APK backup certificate mismatch'}
$dbRemote='/sdcard/Android/data/'+$pkg+'/files/UnrealGame/OfflineGame/OfflineGame/Saved/OfflineGame/WorldState.db'
$dbHash=( (& $adb -s $Serial shell sha256sum $dbRemote)-join ' ' ).Trim().Split(' ')[0].ToLowerInvariant()
if($dbHash -notmatch '^[0-9a-f]{64}$'){throw 'Could not confirm original on-device save checksum'}
$out=[ordered]@{status='DRY_RUN_SIGNED_IN_PLACE_UPDATE_COMPATIBLE';package=$pkg;serial=$Serial;
 frozen_foundation_sha=$expectedBase;candidate_apk=$src;
 candidate_sha256=(Get-FileHash $src -Algorithm SHA256).Hash.ToLowerInvariant();
 candidate_bytes=(Get-Item $src).Length;signing_cert_sha256=$expectedCert;
 installed_apk_backup=$baseCopy;save_sha256_before=$dbHash;apply_requested=[bool]$Apply;updated=$false}
$filename=Join-Path $proof 'verified_apk_update_receipt.json'
New-Item -ItemType Directory -Force -Path $proof|Out-Null
$out|ConvertTo-Json -Depth 5|Set-Content $filename -Encoding utf8
"APK_COMPATIBLE_FOR_UPDATE=true SIGNATURE=$expectedCert SAVES=$dbHash"
if(!$Apply){'DRY_RUN_COMPLETE_NOT_INSTALLED';exit 0}
$saveCopy=Join-Path $proof ('PRESAVE_BEFORE_APK_UPDATE_'+([DateTime]::UtcNow.ToString('yyyyMMddTHHmmss'))+'.db')
$pullOut=Join-Path $proof 'pull_stdout.txt'
$pullErr=Join-Path $proof 'pull_stderr.txt'
$pullProc=Start-Process -FilePath $adb -ArgumentList @('-s',$Serial,'pull',$dbRemote,$saveCopy) -PassThru -Wait -NoNewWindow -RedirectStandardOutput $pullOut -RedirectStandardError $pullErr
$global:LASTEXITCODE=$pullProc.ExitCode
if($LASTEXITCODE -ne 0 -or !(Test-Path $saveCopy)){throw 'Pre-update live phone save backup failed'}
if((Get-FileHash $saveCopy -Algorithm SHA256).Hash.ToLowerInvariant() -ne $dbHash){throw 'Pre-update live save hash mismatch'}
& $adb -s $Serial shell am force-stop $pkg|Out-Null
$installOutPath=Join-Path $proof 'update_install_stdout.txt'
$installErrPath=Join-Path $proof 'update_install_stderr.txt'
$installProc=Start-Process -FilePath $adb -ArgumentList @('-s',$Serial,'install','-r',$src) -PassThru -Wait -NoNewWindow -RedirectStandardOutput $installOutPath -RedirectStandardError $installErrPath
$global:LASTEXITCODE=$installProc.ExitCode
$installOut=Get-Content $installOutPath
if($LASTEXITCODE -ne 0 -or !($installOut -match 'Success')){throw ('ADB signed in-place install failed: '+($installOut -join '|'))}
$dbAfter=((& $adb -s $Serial shell sha256sum $dbRemote)-join ' ').Trim().Split(' ')[0].ToLowerInvariant()
$out.status='PASS_SIGNED_UPDATE_INSTALLED_SAVES_PRESERVED_BEFORE_LAUNCH'
$out.updated=$true
$out.save_sha256_after=$dbAfter
$out.save_preserved=($dbHash -eq $dbAfter)
$out.installed_utc=[DateTime]::UtcNow.ToString('o')
if(!$out.save_preserved){$out.status='WARNING_SAVES_CHANGED_STOP_BEFORE_LAUNCH'}
$out|ConvertTo-Json -Depth 5|Set-Content $filename -Encoding utf8
"UPDATE_INSTALLED=true SAVE_PRESERVED=$($out.save_preserved)"
if(!$out.save_preserved){throw 'Phone save changed during update; STOP and use preserved original backup'}
