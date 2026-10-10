# Gate 12 — Koikatsu facial morph and advanced movement Android QA (2026-10-10)

**This is a diagnostic engineering result. The prior native-rest-safe skeletal sway remains the only accepted experimental animation level. Do not promote to `ARTIFACT_READY` or complete `DEVICE_VALIDATED`.**

## Test fixture and scope

- Project: exact `OfflineGame` Unreal 5.8.3 isolated branch `production/gate12-koikatsu-ue583-fixture-20261010`.
- Production Foundation frozen at `3f54f07cacbf193e160d0be15a7be5a7bc730c3b`; PR #16 is draft/unmerged; original Android `com.azrazieliz.OfflineGame` is not modified.
- Real full-resolution Koikatsu engineering fixture: 19 native skeletal mesh components representing 6 original meshes, 25 source primitive materials, original-resolution textures and genuine imported morph targets. The model has not been converted to video sprites.
- Baseline working full-body scene: `/Game/Experimental/Gate12Koikatsu/FullFidelityQA/Maps/L_KK_NativeRestSafeSway_UE583`.
- Advanced stress and facial diagnostic scene: `/Game/Experimental/Gate12Koikatsu/FullFidelityQA/Maps/L_KK_StressFace_UE583`.
- Source for stress/face anim authoring: `Scripts/Gate12Koikatsu/author_advanced_facial_stress.py`.
- Candidate (NOT YET EXECUTED) material remediation: `Scripts/Gate12Koikatsu/fix_morph_target_material_usage.py`. It uses actual Unreal `MaterialEditingLibrary.set_base_material_usage(material,MaterialUsage.MATUSAGE_MORPH_TARGETS,True)`; after execution it MUST be followed by Android recook/device visual revalidation.

## Actual completed work

Created **19 different AnimSequences** for the 19 exact imported skeletons in `Animation/StressFaceQA`. These are not 19 different gameplay actions. Each runs a synchronized 2-second 61-frame loop with larger rotation excursions on spine, left/right upper/lower arms and neck (12, 36, 22, 9 degrees respectively). All base bone translations, scales and root orientation are preserved. **No root-motion tracks**. The sequences include **8 float facial-expression curves** mapped to existing imported morph names including `Mouth_Happy`, `Mouth__Ah__Sound__L_`, and `Eyebrow_Surprised`. Import unchanged; no geometry reductions.

Created and saved a separate closer-camera scene with the new sequences assigned to all 19 mesh actors. Android ASTC cooked **649 of 649 packages**, Cook commandlet exit code **0**. Signed/development Android arm64 APK produced successfully by `BuildCookRun` exit code **0**, SHA256 `91E840D033014FB324F8D4D8824356BC7886017B574B6296B261CD74479B024D`, size 176,780,309 bytes. APK installed successfully using ADB on physical **Samsung Galaxy S26 Ultra SM-S948B** as separate `com.azrazieliz.gate12fixture`; original game package remained installed.

### Independent results

| Check | Result | Evidence |
|---|---|---|
| Real morph-target data and named targets imported | **PASS** | Native asset audit; targets exist by name |
| Facial float curves saved in exact native clips | **PASS** | 8 curves in 19 exact-skeleton sequences |
| Facial appearance/skin shading on Android | **FAIL** | Physical-device screenshot and video: face becomes abnormally dark/grey |
| Rendered facial expression changing correctly | **NOT_ACCEPTED** | Cannot establish correct appearance while face materials fail |
| Stronger upper-body/arm/neck skeletal motion | **PASS for this stress range** | Real 15-second S26 video; 25.111% silhouette difference opposite phase, 0.644% same phase after 2 seconds |
| Gross outfit separation in sampled torso/skirt frames | **No gross separation observed** | Visual review, **not** a full collision/clipping PASS |
| Android ASTC cook/install | **PASS** | 649/649, process result 0, ADB install success |
| 60-second Android stability | **PASS for non-crash + static memory/thermal observation** | Remained alive every 20s; total PSS 503,849 / 505,150 / 502,606 / 502,694 kB; device thermal status 0; battery 28.8 -> 28.5 C |
| Actual UE renderer FPS/frame-time stability | **NOT_TESTED** | Android `dumpsys gfxinfo` measures few activity/UI frames and cannot be used for Unreal's Vulkan rendered FPS; no validated in-engine frame timing yet |
| Full-body leg/foot collision, running/walking/combat, gameplay input | **NOT_TESTED** | Close-up test cropped feet/part of legs; repetitive procedural sway is not a locomotion/montage system |

### Precise reason for facial FAIL

Physical Android Unreal log reports:
- `LogMaterial: Warning: Material ...KK_cf_m_mayuge_00 missing usage flag MorphTargets! Default Material will be used in game.`
- `LogMaterial: Had to pass SMU back to game thread. Please fix material usage flag MorphTargets` on face, tooth, nose line and tongue materials.
- Non-fatal animation bulk-data memory mapping warnings also appear. This log proves missing morph-target material usage, and the visual failure is consistent with default shader/material substitution. **Do not assume this is the sole visual cause until fixed and retested**.

Fix candidate script is present in repository but **not executed or verified** at the time of this report; the workstation was left alone due to low free RAM (around 1 GB), rather than repeating earlier memory-pressure freezes. It requires one guarded UE 5.8.3 editor commandlet, native material saves, recook/reinstall and visual verification. No renderer color correction should be achieved by downsampling source art.

### Are these game-ready player actions?

**No.** Current QA scenes contain 19 directly placed `SkeletalMeshActor` components performing one predetermined synchronized loop. They are authentic UE skeletal meshes with imported morphs, not video, but:
- No `ACharacter`/pawn with player input possession and CharacterMovement system for this fixture.
- No animation blueprint state machine or blend spaces for movement-directed idle/walk/run/jump/turn.
- No interaction/combat animation montages or seamless facial-expression state controller.
- No Gate 11 provenance/content approval.
A future interactive integration should create an actor with a primary skeletal rig and coordinated attached parts, bind to gameplay input and action tags, retarget/author real action clips with the exact reference skeleton, and use a shared state machine or leader-pose mechanism. **The prior exact-skeleton animation method is technically reusable; the input/gameplay binding is missing.**

### Repository and device policy

This diagnostic level and its stress sequences are *not* set as the isolated branch's default map. The previously PASS native-rest-safe full-body QA map remains the default and its APK is archived locally. The diagnosis must not be copied to Gate 11 as a production character. Gate 00 can use these findings to track the actual remaining material and gameplay blockers. Recorded captures and frame metrics are under `docs/production/gate12/evidence/koikatsu-stressface-s26-20261010/`. Video and screenshots are real ADB output, not generated illustrations.
