$ErrorActionPreference='Stop'
$w='D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010'
$f='D:\UnrealProjects\Offline-Game'
$root="$w\Saved\Gate12Koikatsu\NativeMotionV2"
$r="$root\NativeActionClips"
$runner="$w\Scripts\Gate12Koikatsu\run_kk_quality_v2_single_low_ram.ps1"
$results=[System.Collections.Generic.List[object]]::new()
if((git -C $w branch --show-current).Trim() -ne 'production/gate12-koikatsu-ue583-fixture-20261010'){throw 'Wrong experimental branch'}
if((git -C $f rev-parse HEAD).Trim() -ne '3f54f07cacbf193e160d0be15a7be5a7bc730c3b' -or (git -C $f status --porcelain)){throw 'Frozen Foundation moved'}
$py='D:\Tools\Python311\python.exe'
$verify="$w\Scripts\Gate12Koikatsu\verify_kk_nonanimation_baseline.py"
& $py $verify > "$root\pre_native_author_frozen_lock.json"
if($LASTEXITCODE){throw 'Frozen v1 integrity guard FAIL'}
Write-Output 'NATIVE_V2_BATCH_BEGIN_18_PARTS_EXPECTED_72_NEW_UASSETS'
$failed=$false
foreach($i in 1..18){
    $id=('{0:00}' -f $i)
    $existing=Join-Path $r ("native_action_part_{0:00}_ALL.json" -f $i)
    if(Test-Path $existing){
      $prev=Get-Content $existing -Raw|ConvertFrom-Json
      if($prev.status -eq 'PASS_NATIVE_QUALITY_V2_SEQUENCE_ASSET_AUTHORING' -and @($prev.action_assets).Count -eq 4){
        Write-Output "PART_3f193111-df57-4fc2-9e4f-1c4d8e8a5d87_ALREADY_ACCEPTED_NO_REWRITE"
        $results.Add([pscustomobject]@{part=$i;status='PASS_ALREADY_EXISTING';assets=4})
        continue
      }
    }
    if(@(Get-Process -Name 'UnrealEditor*' -ErrorAction SilentlyContinue).Count -gt 0){
      Write-Output "PART_3f193111-df57-4fc2-9e4f-1c4d8e8a5d87_BLOCKED_EXTERNAL_EDITOR"
      $failed=$true;break
    }
    $os=Get-CimInstance Win32_OperatingSystem
    $free=[math]::Round($os.FreePhysicalMemory/1024)
    $commit=[math]::Round($os.FreeVirtualMemory/1024)
    if($free -lt 1750 -or $commit -lt 3500){
      Write-Output "PART_3f193111-df57-4fc2-9e4f-1c4d8e8a5d87_RESOURCE_WAIT_FREE_RAM_MB=$free COMMIT_MB=$commit"
      $windowStart=[DateTime]::UtcNow
      while(([DateTime]::UtcNow-$windowStart).TotalSeconds -lt 120){
        Start-Sleep -Seconds 10
        $os=Get-CimInstance Win32_OperatingSystem
        $free=[math]::Round($os.FreePhysicalMemory/1024)
        $commit=[math]::Round($os.FreeVirtualMemory/1024)
        if($free -ge 1750 -and $commit -ge 3500){break}
      }
      if($free -lt 1750 -or $commit -lt 3500){
        $failed=$true;Write-Output "PART_3f193111-df57-4fc2-9e4f-1c4d8e8a5d87_BLOCKED_RESOURCE_WINDOW";break
      }
    }
    $start=[DateTime]::UtcNow
    Write-Output "PART_3f193111-df57-4fc2-9e4f-1c4d8e8a5d87_NATIVE_UNREAL_START_UTC=$($start.ToString('o'))"
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -PartIndex $i -Action ALL -MaxSeconds 240
    $exit=$LASTEXITCODE
    $state='FAIL_NO_NATIVE_RECEIPT';$assets=0
    if(Test-Path $existing){
      $report=Get-Content $existing -Raw|ConvertFrom-Json
      $state=[string]$report.status
      $assets=@($report.action_assets).Count
    }
    $ok=($exit -eq 0 -and $state -eq 'PASS_NATIVE_QUALITY_V2_SEQUENCE_ASSET_AUTHORING' -and $assets -eq 4)
    $results.Add([pscustomobject]@{part=$i;status=if($ok){'PASS_NATIVE_4_EXACT_PART_CLIPS'}else{'FAIL_NATIVE_CLIPS'};report_state=$state;exit=$exit;assets=$assets;durationSec=[math]::Round(([DateTime]::UtcNow-$start).TotalSeconds)})
    [ordered]@{status='BATCH_IN_PROGRESS';created_or_checked_parts=$results.Count;results=$results.ToArray();time_utc=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 7|Set-Content "$root\quality_v2_native_batch_receipt.json" -Encoding UTF8
    Write-Output "PART_3f193111-df57-4fc2-9e4f-1c4d8e8a5d87_END_NATIVE_STATUS=$state count=$assets exit=$exit"
    if(!$ok){$failed=$true;break}
}
$all=Get-ChildItem "$w\Content\Experimental\Gate12Koikatsu\Animation\NativeQualityV2" -Filter '*.uasset' -File -ErrorAction SilentlyContinue
$good=($results.Count -eq 18 -and !$failed -and @($all).Count -eq 76)
[ordered]@{status=if($good){'PASS_NATIVE_19_PARTS_76_V2_CLIPS_PRE_COLD_LOAD'}else{'PARTIAL_V2_ASSET_AUTHORING_NOT_RUNTIME_APPROVED'};total_native_v2_assets=@($all).Count;parts_completed=$results.Count;results=$results.ToArray();time_utc=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 7|Set-Content "$root\quality_v2_native_batch_receipt.json" -Encoding UTF8
& $py $verify > "$root\post_native_author_frozen_lock.json"
if($LASTEXITCODE){throw 'Frozen v1 integrity fail after new authoring'}
Write-Output "V2_BATCH_FINAL=$good TOTAL_UASSETS=$(@($all).Count)"
if(!$good){exit 29}
