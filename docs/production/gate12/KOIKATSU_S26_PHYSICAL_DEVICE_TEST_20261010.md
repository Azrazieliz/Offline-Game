# Gate 12 — Experimental Koikatsu character, physical Galaxy S26 Ultra test

**Date:** 2026-10-10. **Engineering-only fixture:** `G01-KK-PIPELINE-TEST-0002` (Chika Haruno), not a Gate 11 `ARTIFACT_READY` character. **Overall result:** PARTIAL. Installation and launch passed; visible character rendering failed. Not `DEVICE_VALIDATED`.

## Device and project
- Samsung Galaxy S26 Ultra, Android 16, `SM-S948B`, device serial `R3GYC0LSC7K`, arm64-v8a, observed physically through authorized ADB on 2026-10-10.
- Unreal Engine **5.8.3**, changelist `58210709`.
- Isolated worktree `D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010`, branch `production/gate12-koikatsu-ue583-fixture-20261010`, historical base freeze `3f54f07cacbf193e160d0be15a7be5a7bc730c3b`.
- Test app `com.azrazieliz.gate12fixture`. The original `com.azrazieliz.OfflineGame` remains installed and was not updated or launched by the test.

## Actual device actions and evidence
1. Android ASTC cook **PASS:** 569/569 packages, commandlet return 0, approximately 45.42 s. Source: `Saved/Gate12Koikatsu/android_astc_cook_engine.log`.
2. Isolated APK build/package **PASS:** BuildCookRun result success, ExitCode 0 (127.18 s); apk 168,040,605 bytes, SHA256 **`50d9b42121b241ae050e1a1431908d6e52ffe6d6c8760073c1b6c096f97bd4cb`**, arm64, minSDK 26, targetSDK 36, Android APK Signature Scheme v2 verified, one signer. Relative path `Saved/Gate12Koikatsu/IsolatedAndroidPackage/Android_ASTC/OfflineGame-arm64.apk`.
3. **Real physical install PASS**: `adb -s R3GYC0LSC7K install ...` -> `Success` / exit 0, 2026-10-10 13:47:37–13:47:51 UTC; no replacement because the experimental package was absent. `pm path com.azrazieliz.gate12fixture` returned new device APK; original `pm path com.azrazieliz.OfflineGame` still present. Evidence `Saved/Gate12Koikatsu/physical_s26_install_20261010.txt`.
4. **Cold start and process survival PASS**: `am start -W -n com.azrazieliz.gate12fixture/com.epicgames.unreal.SplashActivity`; status `ok`, `LaunchState: COLD`, `GameActivity`, 466 ms Android activity wait, ADB exit 0. Fixture processes PID 32617 and 32698 remained present while the GameActivity had foreground focus through subsequent screens at 13:49:10 and 13:50:24 UTC. This does **not** measure time to fully rendered character. Evidence `physical_s26_launch_20261010.txt`.
5. **Visible output FAIL**: first screenshot `Saved/Gate12Koikatsu/physical_s26_runtime_screenshot.png` SHA256 `36F05FA1054E97149D1C5E4680B811EBED3418FE710B6D0DBD379E092543AC63`; second screenshot `Saved/Gate12Koikatsu/physical_s26_final_qa.png` SHA256 `60353A58532A3C44BB699AC8F1F3F8DBB363BA2753D908106741FA2EBA891B60`. Both show nearly all-black output with a small white curved region on the left; no recognizable character. This remains a rendering/camera/skeletal-transform/blocking issue, **not device-level acceptance**. The screenshots do not distinguish root cause.
6. App memory while running, observed from `dumpsys meminfo`: PSS ~571,006 KB initially and ~559,671 KB later; RSS ~715,744 KB then ~627,208 KB. No validated FPS, bone deformation, expression playback, material cutout visual fidelity, device GPU time, or stable performance metrics.
7. Device log captures preserved in `Saved/Gate12Koikatsu/physical_s26_logcat_last3000.txt` and `physical_s26_logcat_final4500.txt`. No assertion of zero warnings/errors or passing Unreal graphics tests is made based only on process survival.
8. **Clean finish**: `am force-stop com.azrazieliz.gate12fixture` (app stays installed), returned phone to `com.openai.chatgpt/.MainActivity`; verified resumed/focused ChatGPT. Both test and original packages remain installed. No player data cleared.
9. Frozen original project checkout remains **clean** on `3f54f07cacbf193e160d0be15a7be5a7bc730c3b`. Existing Windows player `Saved/OfflineGame/WorldState.db` SHA256 `4DBB961D45823763EBCE7E16DC62B1102B32B8FFD46003AB61AB05F0EF83554A`, unchanged. PR #16 remains draft/unmerged; no Foundation merge.

## Independent final verdicts
| Category | Status |
|---|---|
| Experimental UE 5.8.3 split-glb importer | **PASS** for four skeletal meshes and retained limited morph/mode data |
| Native desktop gameplay frame and correct rig deformation | **NOT VERIFIED**; earlier render remained on shader preparation |
| Android ASTC cook | **PASS** |
| Android APK packaging/signature | **PASS** |
| Physical S26 Ultra install | **PASS** |
| Physical S26 Ultra cold launch / process | **PASS** |
| Physical S26 Ultra character render | **FAIL** (near-black screen, repeatable) |
| Facial animation / skin deformation on phone | **NOT TESTED** (no visible character) |
| Physical device full acceptance | **NOT VALIDATED** |
| Original app/user data/frozen Foundation preservation | **PASS** |

**Next blocker:** Determine whether the split skeletal-actor bounds/orientation, camera setup, frame shading or incompatible skeletons place/mask the content. Test corrected image on the same actual device, not another diagnostic placeholder. Gate 11 artifact authority remains absent.
