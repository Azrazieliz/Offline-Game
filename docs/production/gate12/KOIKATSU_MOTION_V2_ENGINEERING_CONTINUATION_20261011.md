# Gate 12 — Post-freeze Koikatsu Motion V2 engineering continuation

**Date:** 2026-10-11. **Worktree:** production/gate12-koikatsu-ue583-fixture-20261010. **Frozen non-animation baseline:** G12-KK-NONANIM-1.0.0 at 11b4461917cdb07df369eb186beef209164026cc. The freeze's TEN SHA-256 locked references, frozen Foundation, user save data and Gate 11 registry authority remain untouched. This document records a new incremental engineering milestone, **not** production-ready locomotion or a new-character acceptance.

## Real implementation completed

1. **Native motion controller changed in isolated Gate12 source only.** OGKoikatsuPlayableCharacter synchronizes playback of 19 different exact imported skeletons to one actor-level normalized state clock. It retains phase across Walk <-> Run and smooths measured horizontal speed with a 0.16-second gait switch hold to reduce oscillating state resets. A separate UCharacterMovementComponent::IsSwimming() branch selects Swim / SwimIdle, instead of erroneously selecting Walk/Run underwater. Exact per-asset skeleton equality still mandatory, and a missing native V2 part triggers all-or-none fallback to the v1 engineering clips. Phase/timing synchronization does NOT supply a complete AnimBP blend tree; transitions between other action states remain hard cuts.
2. **Dynamic expression API** added: PulseExpressionMorph(name, peak, rise, hold, fade) applies a time-varying morph weight to all mesh parts that genuinely expose that native target. This method is compiled but not physically demonstrated with a verified nonzero-movement face/eyebrow target. Many 4,611 original GLB morph references contain zero geometry deltas; checking a target name alone is insufficient.
3. **New, separate motion assets:** four bounded, loop-continuous provisional profiles (Walk, Run, Swim, SwimIdle), written as real Unreal UAnimSequence assets into Animation/NativeQualityV2. 19 separate source imported skeletons x four states = 76 real .uasset files. Original 171 V1 engineering action sequences and all original source GLB/card/mesh/texture/morphs remain untouched. These new clips are still deterministic in-place procedural test motion, not final mocap/artist-approved locomotion or convincing fully horizontal swimming.
4. **Reproducible generation + checks:** kk_native_motion_v2_profiles.py, author_kk_quality_v2_synchronized_actions_ue583.py, run_kk_quality_v2_single_low_ram.ps1, author_quality_v2_all_19_parts_guarded.ps1, verify_kk_native_quality_v2_cold_ue583.py, run_kk_quality_v2_cold_low_ram.ps1. Explicit packaged-runtime path cooking in isolated Config/DefaultGame.ini. The playable-world launch map remains isolated to the Gate12 test package.

## Actual validation

| Gate | Result | Evidence |
|---|---|---|
| Frozen non-animation byte lock, all 10 references, original save/foundation | **PASS** repeatedly | read-only SHA-256 verifier |
| Motion model bounded native joints, loop position/derivative continuity | **PASS STATIC** | kk_native_motion_v2_profiles.py |
| Full Windows Unreal Engine 5.8.3 gameplay source rebuilt and linked | **PASS** | 12/12 native UBT build actions, Result: Succeeded |
| Unreal native V2 authoring | **PASS** | 76 original native UAnimSequence .uassets across all 19 component identities |
| New independent UE editor cold-load (exact skeleton, actual bone tracks, clip duration, no root tracks) | **PASS** | quality_v2_cold_unreal.json: 19/19 components, 76/76 clips |
| Playable-world Android_ASTC cook | **PASS** | 912 packages processed, 79 cooked, 790 cache skipped, 43 platform skipped, 0 errors/warnings; all 76 V2 .uassets in cooked output |
| Android ARM64 native gameplay compilation, link and Android debug build | **PASS** | second incremental UBT run: Result: Succeeded; first attempt safely paused for RAM |
| Full staged Pak APK, v2 asset inclusion and Android signature | **PASS** | 3,875 pak entries, 76 unique V2 sequences (228 pak list references), AutomationTool ExitCode=0 |
| APK native executable identity | **PASS** | linked and APK-stripped ELF build ID both a334f7fed1009e63999d99a9784768ec8fb29f7d |
| New V2 APK installed/running on S26 Ultra | **NOT TESTED** | Device currently disconnected; original installed experimental V1 fixture must not be treated as V2 |
| Walk/Run natural gait, visible swim/body posture, camera wall clipping, clothing collisions, timed facial expressions | **NOT APPROVED / NOT DEVICE TESTED** | New APK requires physical test / screen recording |
| UE frame times, GPU frame timing, S26 sustained renderer FPS/thermals | **NOT MEASURED** | No active ADB device. Script can collect Android SurfaceFlinger/gfxinfo proxies, not direct UE GPU timings |

## Real artifact fingerprints

- New experimental signed APK: 181,196,929 bytes, SHA-256 **8a34ac0deff1838b3063638286e5d47eea48bb1fb09584a28e7b422e63100112**, identity **com.azrazieliz.gate12fixture**, arm64-v8a, V2 debug build. Local path: Binaries/Android/OfflineGame-arm64.apk.
- New Android native linked ELF: 452,594,120 bytes, SHA-256 **437cf370e62460b07e7e970408c0fa8ab94182d6bb03430756e0000e631b90af**; APK-stripped native library SHA-256 **15d0564f7f215e50deab6b7d86ac7a57d981d23b87c53b8aa38cedb9bfd16170**; matched ELF build ID above.
- Prior physically validated experimental APK preserved under Saved/Gate12Koikatsu/NativeMotionV2/before_motion_v2_OfflineGame_arm64.apk, SHA-256 3e68cdf3e300badcc5e26161c8f082948740189aad64949f25d551eb86fc9833. Prior ARM64 .so preserved alongside, SHA-256 ace57c555c0b40c25699b5a5785fccd3e3ba8113bd03a569703165a26c04e380. These are laptop-local backups, not checked into Git or replaced on the phone.

## Next steps under Gate12 responsibility

1. When S26 Ultra reconnects to USB/ADB, independently verify package ID and original application preservation, install ONLY the new experimental fixture APK, and test repeated Idle/Walk/Run/Jump/Fall/Land/TurnLeft/TurnRight/Dodge/Swim/SwimIdle/Action transitions with camera/stamina/HP HUD. Use run_s26_motion_v2_qa_guarded.ps1 -Execute for bounded device telemetry and screenshots; original app and original save must remain untouched.
2. Iterate with **real compatible rigged animation source** (artist-authored/mocap or retargeted from compatible licensed motions) and a full transition blending implementation (e.g., exact-skeleton per-part AnimBlueprint / BlendSpace), true forward locomotion contact and physically plausible swim pose; current V2 is merely a bounded engineering step and old action clips are still quality-failed.
3. Select effective nonzero facial morphs from source/native verified geometry, run independent GPU mobile mouth/brow expressions concurrently with locomotion, and inspect clothing/hair intersections under crouch/swim/dodge.
4. Investigate near-wall view blackout in actual S26 footage, separating occluding world geometry from character near-camera mesh. Preserve Foundation camera and mobile controls until exact isolated cause verified.
5. Capture real Unreal frame times / GPU timings by instrumenting the experimental build or an authorized UE profiling run, with SurfaceFlinger/gfxinfo as supplemental device-present statistics only. Video apparent smoothness is not a measured engine-FPS claim.
6. Validate the first **different** real Gate01 export independently from staged GLB through fresh Unreal native import, new blueprint, shader cook, isolated APK and physical device before expanding the single-fixture freeze's scope.

**Governance:** no Gate 11 ARTIFACT_READY; Gate00 governs start / creative priority; Gate01 makes source characters; Gate12 owns this reference fixture's engine/device acceptance; PR16 remains draft/unmerged. Frozen Foundation HEAD 3f54f07cacbf193e160d0be15a7be5a7bc730c3b; WorldState.db SHA-256 4dbb961d45823763ebce7e16dc62b1102b32b8ffd46003ab61ab05f0ef83554a.
