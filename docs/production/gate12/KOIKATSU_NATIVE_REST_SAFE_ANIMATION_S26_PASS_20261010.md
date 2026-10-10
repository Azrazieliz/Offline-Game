# Gate 12 — native-rest-safe Koikatsu skeletal animation (UE 5.8.3 / Galaxy S26 Ultra)

**Status: PASS for full-resolution static character and basic looping skeletal motion on physical Android.**
**Not Gate 11 ARTIFACT_READY, not full DEVICE_VALIDATED, and not a Gate 00 production-asset approval.**
Experimental fixture only: G01-KK-PIPELINE-TEST-0002 (Chika Haruno).

## Frozen baseline and scope

- Foundation SHA: `3f54f07cacbf193e160d0be15a7be5a7bc730c3b`, tag `foundation-v1-freeze-2026-10-08` — unchanged.
- Isolated branch: `production/gate12-koikatsu-ue583-fixture-20261010`.
- PR #16 remains draft/unmerged. The original Android package `com.azrazieliz.OfflineGame` is untouched.
- Experimental Android package: `com.azrazieliz.gate12fixture`. Tested on **Galaxy S26 Ultra / SM-S948B / Android 16**, ADB serial `R3GYC0LSC7K`.
- Source export: `D:\Tools\GameAssetPipeline\KoikatsuParty\characters\G01-KK-PIPELINE-TEST-0002\conversion\character_unreal_skeleton.glb`
  SHA256 `ad06955e6f725c63c6ee014828877a9a8fa3a19479ca29936c58c96ed6d012e0`.
- Original source GLB and Blender source were never modified. **Source provenance/licensing requires Gate 11 review before any production promotion.**

## Why previous animation failed

The old animation `AN_ChikaHaruno_SpineSway_UE583` was created against a different UE skeleton, then assigned to all 19 newly imported skeletons using compatibility flags. The old sequence stored local bone transforms (including a `Center` root rotation) derived from that old rig. Matching animation names or allowing skeleton compatibility **does not** ensure equal rest-pose translations/orientations between separately imported rigs. The previous S26 build produced an incorrect apparent-scale presentation and no accepted deformation evidence.

Import problems solved independently:
1. Previous diagnostic imports intentionally shrank original 4096x4096 images to 256x256, discarded material slots and treated most materials as translucent. **Do not use diagnostic assets in production.**
2. The original QA camera used incorrectly ordered positional `unreal.Rotator` arguments and lacked `PlayerStart`. Saved native QA map fixed with explicit named pitch/yaw/roll.
3. Unreal converts some source morph references with effectively zero positional impact (<=0.025mm tolerance). Preserve all source deltas; native reconciliation uses *effective* displacement counts.

## Reusable full-fidelity pipeline (engineering)

1. Starting with the immutable original GLB, run `Scripts/Gate12Koikatsu/build_lossless_shards.py`, then `build_lossless_primitive_shards.py`. They preserve source **6 meshes / 25 primitives / 4,611 target references**, retain embedded texture bytes and original material coverage, and only correct transparent materials if the original image alpha channel is fully opaque.
2. Import via `import_full_shard_guarded.ps1` for full hair/outfit (mesh 4 and 5) and `import_all_primitives_guarded.ps1` for other original primitives. Each part is independently hash-gated and saved under `/Game/Experimental/Gate12Koikatsu/FullPrimitives` or `FullFidelity`. Batch processing protects laptop interactivity; **never downsample final game assets because of workstation RAM**.
3. Run `verify_full_fidelity_unreal.py` and `reconcile_native_morph_threshold.py` to compare native `.uasset` material slots, morphs and texture dimensions against source manifest. Verified **19 native UE skeletal assets / 25 material slots** and full original texture resolutions.
4. The saved **static** complete real-assets scene is `/Game/Experimental/Gate12Koikatsu/FullFidelityQA/Maps/L_KK_CompleteSource_UE583`.
5. For skeletal motion, **do not reuse the unrelated old spine-sway sequence**. Run `Scripts/Gate12Koikatsu/author_native_rest_pose_sway.py` in the exact UE 5.8.3 isolated editor Python commandlet:
   - For EACH of 19 actual imported `USkeleton`s, call `get_reference_pose()` and `pose.get_bone_pose(bone, AnimPoseSpaces.LOCAL)`.
   - Construct a unique `AnimSequence` with `AnimSequenceFactory.target_skeleton` set to that EXACT skeleton.
   - Use the native `AnimDataController`: 30 fps / 60 intervals / 61 keys, and key `spine_02`, `upperarm_l/r`, `lowerarm_l/r`, and `neck` where present.
   - Preserve native bone reference translations and scales *exactly* and multiply original reference quaternions by small local rotational sway deltas.
   - **Never create a `Center`, `pelvis` or any root animation track.** Root motion remains reference pose.
   - Store unique animation assets under `/Game/Experimental/Gate12Koikatsu/Animation/NativeRestSafe/`.
6. Run `bind_native_rest_sway_map.py`, which clones the original static map without changing it. It verifies *exact equality* between each mesh's skeleton and its assigned sequence, sets serialized looping `override_animation_data`, and saves:
   `/Game/Experimental/Gate12Koikatsu/FullFidelityQA/Maps/L_KK_NativeRestSafeSway_UE583`.
7. In **isolated worktree config only**, set `GameDefaultMap` to that new map and package ID to `com.azrazieliz.gate12fixture`. Android ASTC cook via `run_guarded_restsafe_sway_cook.ps1`, followed by isolated `run_guarded_android_package.ps1`. Build with the frozen Foundation's previously validated Android arm64 native target receipt; do not alter frozen main.
8. Install on Galaxy S26 Ultra with `adb -s R3GYC0LSC7K install -r <isolated APK>`. Capture actual screenshot, landscape video, device log, hashes and user-data preservation.

**Source interchange reproducibility requires the original local GLB bytes identified by SHA256; derived full-quality .uasset and animation assets are committed on this isolated branch.** The public repo does not make licensing or Gate 11 acceptance claims.

## Evidence (2026-10-10)

| Test | Status | Real evidence |
|---|---|---|
| Full source import and actual detailed materials | PASS | 19 full-quality UE skeletal meshes, 25 material slots, original texture dimensions; screenshot on Galaxy |
| Native full-quality animation creation | PASS | 19/19 exact-skeleton sequences; each 61 keys / 2 seconds; **zero root tracks** |
| Native saved animated level | PASS | 19/19 mesh/sequence/skeleton checks; `rest_safe_animated_map_report.json` |
| Android ASTC cook | PASS | **649/649 packages**, commandlet exit 0 |
| Android APK assemble/sign/install | PASS | `BuildCookRun` exit 0; separate package installed with ADB success |
| S26 full character size, alignment, clothes, texture | PASS for static scene appearance | `S26_NativeRestSafeSway_Initial.png` |
| S26 actual looping bone motion | PASS for basic sway | 12-second 1280x720 **physical** video, 48 evaluated frames |
| Body motion amount across opposite poses | PASS | 14.74% different foreground silhouette, 28.44% changed foreground RGB |
| 2-second loop repeatability | PASS | Only 0.285% silhouette difference, 0% RGB difference at matching cycle phase |
| Facial expression morph **playback** | **NOT_TESTED** | Morphs imported, but no runtime keyed facial sequence has passed device evidence |
| Broad animation retargeting/gameplay/controller/FPS/stress | **NOT_TESTED** | Only native test sway validated |
| Production asset registry / Gate 11 acceptance | **NOT_AUTHORIZED** | Experimental fixture only |

Recorded APK SHA256: `CF65A4A36C6A27ABE6F4D27C0861CE4EE980B09D9C8410FA756098158C487ACC`
(size **176,771,973 bytes**).
Landscape motion video SHA256:
`469BBA3154E0D6941124C8FFC1FB9F8FC2FC769F339C06F049B508156CC34F35`
(12s, 9,705,425 bytes).
Portrait diagnostic video SHA256:
`356621F2BD365DE29336385B6E5ECBE5503FDC523C9E895B7F9E870B289B47B2`.
Local evidence is at `Saved/Gate12Koikatsu/`, including `S26_NativeRestSway_frame_motion_metrics.json`, `S26_NativeRestSafeSway_Runtime.log`, and original screenshots.

The **bulk-data memory-mapping warnings** in Android logs persist but are not proof of a playback failure; fallback loading occurred and full rendered motion was observed. There is no claim of zero runtime warnings. Assets still require production-level visual/art-direction and provenance review.

## Handoff to Gate 00 / Gate 11

**Accept as Gate 12 isolated engineering milestone only**: complete-detail UE5.8.3 skeletal import and **physical Android basic animation** can work when each animation is generated from the native target skeleton's rest pose. This provides a repeatable method for future Koikatsu engineering fixtures.

**Do not merge into frozen Foundation, make PR #16 ready, promote to ARTIFACT_READY, or claim full DEVICE_VALIDATED.** Open requirements include visually reviewed facial-expression curves, advanced motion/skin stress, engine shader parity, asset rights/provenance, stable mobile performance, and Gate 11 intake.

## Reviewable evidence committed to this isolated GitHub branch

The actual physical-device screenshots, 12-second landscape recording, and quantitative per-frame motion JSON (not AI-generated or illustrative) are under `docs/production/gate12/evidence/koikatsu-restsafe-s26-20261010/`. These files show phase A vs phase B of the same two-second loop. They are engineering validation evidence, not production art approvals.
