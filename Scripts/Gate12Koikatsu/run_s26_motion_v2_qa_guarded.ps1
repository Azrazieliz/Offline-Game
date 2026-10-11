param(
 [switch]$Execute,
 [ValidateRange(10,180)][int]$InteractiveSeconds=45,
 [string]$Serial='R3GYC0LSC7K',
 [string]$Worktree='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010'
)
$ErrorActionPreference='Stop'
$w=[IO.Path]::GetFullPath($Worktree)
$f='D:\UnrealProjects\Offline-Game'
$adb='C:\Users\mimim\AppData\Local\Android\Sdk\platform-tools\adb.exe'
$pkg='com.azrazieliz.gate12fixture'
$original='com.azrazieliz.OfflineGame'
$py='D:\Tools\Python311\python.exe'
$verify=Join-Path $w 'Scripts\Gate12Koikatsu\verify_kk_nonanimation_baseline.py'
$r=Join-Path $w ('Saved\Gate12Koikatsu\NativeMotionV2\S26QA_'+[DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss'))
if((git -C $w branch --show-current).Trim() -ne 'production/gate12-koikatsu-ue583-fixture-20261010'){throw 'Wrong worktree'}
if((git -C $f rev-parse HEAD).Trim() -ne '3f54f07cacbf193e160d0be15a7be5a7bc730c3b' -or (git -C $f status --porcelain)){throw 'Frozen Foundation changed'}
$intact=& $py $verify|ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $intact.status -ne 'PASS_NONANIMATION_BASELINE_BYTE_LOCK_ONLY'){throw 'Non-animation frozen reference drift'}
$device=(& $adb devices -l|Select-String ([regex]::Escape($Serial))).Line
if(!$device -or $device -notmatch '\sdevice\s'){Write-Output "S26_V2_QA_NOT_STARTED_DEVICE_DISCONNECTED_SERIAL=$Serial";exit 12}
$model=(& $adb -s $Serial shell getprop ro.product.model).Trim()
if($model -notmatch 'SM-S948B'){throw "Unexpected Android device $model"}
$origPath=@(& $adb -s $Serial shell pm path $original)
if(!$origPath -or $origPath[0] -notmatch '^package:'){throw 'Cannot establish original game installed unchanged'}
$fixturePath=@(& $adb -s $Serial shell pm path $pkg)
if(!$fixturePath -or $fixturePath[0] -notmatch '^package:'){Write-Output 'S26_V2_QA_NOT_STARTED_EXPERIMENTAL_NOT_INSTALLED';exit 12}
New-Item -ItemType Directory -Force $r|Out-Null
$origPath|Set-Content (Join-Path $r 'original_package_path_before.txt')
@($fixturePath)|Set-Content (Join-Path $r 'fixture_package_path.txt')
$metadata=[ordered]@{status='PREFLIGHT_PASS_ONLY';device_model=$model;serial=$Serial;fixture_id=$pkg;original_id=$original;original_app_touched=$false;recording='NONE';unreal_renderer_cpu_frame_ms='NOT_MEASURED';unreal_renderer_gpu_frame_ms='NOT_MEASURED';user_visible_motion_approved=$false;qa_dir=$r;start_utc=[DateTime]::UtcNow.ToString('o')}
if(!$Execute){
 $metadata|ConvertTo-Json -Depth 6|Set-Content (Join-Path $r 'receipt.json') -Encoding UTF8
 Write-Output "S26_V2_QA_PREFLIGHT_ONLY=$r (use -Execute after user authorizes interactive run)"
 exit 0
}
Write-Output '=== INTERACTIVE S26 EXPERIMENTAL QA ==='
& $adb -s $Serial shell am force-stop $pkg
Write-Output 'Preserve existing device log buffers; only this session log is captured below'
& $adb -s $Serial shell monkey -p $pkg -c android.intent.category.LAUNCHER 1 |Out-Null
Start-Sleep -Seconds 12
$pidPre=@(& $adb -s $Serial shell pidof $pkg)
if(!$pidPre){$metadata.status='FAIL_LAUNCH_PROCESS_MISSING'}else{$metadata.status='LAUNCHED_NOT_YET_VISUAL_APPROVED'}
$localFirst=Join-Path $r 'early_frame.png'
& $adb -s $Serial shell screencap -p '/sdcard/Pictures/g12_v2_early.png'|Out-Null
& $adb -s $Serial pull '/sdcard/Pictures/g12_v2_early.png' $localFirst |Out-Null
Write-Output "Use existing phone game HUD now for $InteractiveSeconds seconds (walk, run, dodge, jump, swim, camera, face)."
Start-Sleep -Seconds $InteractiveSeconds
$localLast=Join-Path $r 'late_frame.png'
& $adb -s $Serial shell screencap -p '/sdcard/Pictures/g12_v2_late.png'|Out-Null
& $adb -s $Serial pull '/sdcard/Pictures/g12_v2_late.png' $localLast |Out-Null
& $adb -s $Serial logcat -d -v threadtime > (Join-Path $r 'whole_bounded_session_logcat.txt')
& $adb -s $Serial shell dumpsys gfxinfo $pkg framestats > (Join-Path $r 'android_gfxinfo_framestats.txt')
& $adb -s $Serial shell dumpsys meminfo $pkg > (Join-Path $r 'app_meminfo.txt')
& $adb -s $Serial shell dumpsys battery > (Join-Path $r 'battery_thermal_proxy.txt')
& $adb -s $Serial shell dumpsys SurfaceFlinger --list > (Join-Path $r 'surfaceflinger_surface_list.txt')
& $adb -s $Serial shell pidof $pkg > (Join-Path $r 'game_pid_after.txt')
$log=Get-Content (Join-Path $r 'whole_bounded_session_logcat.txt') -Raw -ErrorAction SilentlyContinue
$metrics=[ordered]@{
 native_motion_controller_messages=([regex]::Matches($log,'Gate12 KK MotionV2:')).Count
 state_transition_messages=([regex]::Matches($log,'Gate12 KK: state=')).Count
 expression_pulse_messages=([regex]::Matches($log,'Gate12 KK MotionV2: morph pulse')).Count
 fatal_signals=([regex]::Matches($log,'Fatal signal|SIGSEGV|Fatal error')).Count
}
$metadata.session_metrics=$metrics
$metadata.elapsed_interaction_seconds=$InteractiveSeconds
$metadata.process_survived=([bool](@(& $adb -s $Serial shell pidof $pkg|Where-Object{$_ -match '\d'}).Count))
$metadata.screenshot_early=Test-Path $localFirst
$metadata.screenshot_late=Test-Path $localLast
$metadata.status=if($metadata.process_survived -and $metadata.screenshot_late){'PASS_LAUNCHED_SURVIVED_COLLECTED_DEVICE_TELEMETRY_NOT_MOTION_QUALITY'}else{'NOT_PASS_DEVICE_SURVIVAL'}
$metadata.renderer_performance='NOT_MEASURED_ENGINE_DIRECTLY__SURFACEFLINGER_OR_GFXINFO_IF_PRESENT_ONLY'
$metadata.finished_utc=[DateTime]::UtcNow.ToString('o')
$after=@(& $adb -s $Serial shell pm path $original)
if(($origPath -join '|') -ne ($after -join '|')){throw 'Original game path changed'}
$after|Set-Content (Join-Path $r 'original_package_path_after.txt')
$metadata|ConvertTo-Json -Depth 8|Set-Content (Join-Path $r 'receipt.json') -Encoding UTF8
Write-Output "S26_V2_QA=$($metadata.status) LOGS=$r"
