# Gate 12 — 8 GiB laptop UE5.8.3 native action and morph-material milestone

Date: 2026-10-10. Experimental Koikatsu fixture `G01-KK-PIPELINE-TEST-0002`; no Gate 11 production asset approval. This is a continuation of authoritative exact recovery commit `aff5f4fdeed6a4f11d7e5438ace6208f93e00603`, not a new Foundation/P0.

## A. Real native engine authoring, validated

**UE5.8.3 actual `.uasset` animation creation: PASS** on authorized laptop `LAPTOP-1LI4VRCJ`, 7.63 GiB RAM. Serialized, one-UE-commandlet-at-a-time authoring avoided full graphical editor, using `UnrealEditor-Cmd.exe -run=pythonscript -unattended -nop4 -nullrhi -nosplash -NoShaderCompile`. Live watchdog measured actual physical RAM, Windows commit headroom and bounded runtime. No unrelated user process terminated. Full-quality original 19 imported skeletal meshes were **reused**, not regenerated or downsampled.

- Nine **distinct procedural** action types: Walk, Run, Jump, Fall, Land, TurnLeft, TurnRight, Dodge, Action. These are engineering in-place native bone animation assets, *not imported motion-capture or proven visually natural locomotion*.
- **19 native exact-skeleton components × 9 actions = 171 saved AnimSequences**, not video/sprites; additionally the old 19 looping rest-safe sequences remain preserved.
- All 19 independent Unreal commandlet batch receipts returned `PASS_NATIVE_ACTION_SEQUENCE_ASSET_AUTHORING`; 171 verified exact native mesh/skeleton references and **zero root tracks**, 30 fps, per-action looping metadata and distinct curves.
- Pure motion-profile static tests PASS. Full asset-inventory static audit `PASS_STATIC_171_REAL_UE_UASSET_PACKAGES`; 171 real Unreal package magic headers, total 4,569,691 bytes, nine action counts of 19 each, aggregate reproducible ordered-name-and-SHA256 digest `188f874055d0238f047f27e897dbc9f1cdc2b175f122b2c3a340466a2410c57f`.
- Local execution receipts: `Saved/Gate12Koikatsu/NativeActionClips/native_action_part_??_ALL.json`, all watchdogs and raw Unreal logs, `171_native_action_assets_static_integrity.json`. The earlier first single `Walk` asset was hash/skeleton checked and **reused without overwrite**.
- New source: `Scripts/Gate12Koikatsu/kk_native_action_profiles.py`, `author_kk_incremental_native_actions_ue583.py`, `run_kk_native_action_single_low_ram.ps1`, `audit_kk_action_assets_static.py`.
- **Limit:** No C++ player state binding proven, mobile cooking not passed for new action clips, visual transition quality/foot-grounding/collision not tested.

## B. Facial morph material persistence — native cold reload PASS, Android visuals outstanding

Earlier engineering session's in-memory `15/15 MorphTargets true` did **not** persist. Fresh UE5.8.3 commandlet inspection of genuine face instance proved its external `/InterchangeAssets/gltf/M_Default` material base had `morph_flag=false` after cold load. Saved `KK_cf_m_face_00.uasset` initial SHA256 `3DB5AAE185A0901E0246B704A0C340AE402FC475EC41E312FC0FFA19106F2DC9`.

**Repair executed natively**, first one face instance, then all remaining 14:
- Exactly **15** morph-bearing source material instances, one per actual saved imported slot, now reference a project-owned shader-parent chain in `/Game/Experimental/Gate12Koikatsu/MaterialShaderOverrides/` with `MATUSAGE_MORPH_TARGETS` enabled.
- On first native test, two external shader levels copied into local assets and the face instance reparented. Face save became SHA256 `E2CCDD95A5C4AFF5E0E562BAE580693C52548369653C9661B9285FA7D1E1A4C1`. Imported scalar/vector/texture override arrays preserved. A private pre-edit byte backup exists in `Saved/Gate12Koikatsu/MaterialRepairBackups/`.
- Second native material pass: 14 additional saved instances reparented, one already-owned face instance verified without overwrite; shared shader reused, any new import-parent variants retained separately as needed. **Zero native errors.**
- **Independent fresh UE5.8.3 process** ran `verify_kk_morph_shader_cold_reload_ue583.py`: **PASS_PERSISTED_MATERIAL_PARENT_AND_MORPH_FLAG_ONLY, 15/15**, one project-owned base shader with MorphTargets usage true and on-disk package digests matching native repair receipts. No external import-base references in the verified chains. No desktop GUI needed.
- Source files: `repair_kk_morph_shader_ownership_ue583.py`, `verify_kk_morph_shader_cold_reload_ue583.py`, `run_low_ram_native_material_test.ps1`.
- Local evidence: `Saved/Gate12Koikatsu/owned_morph_shader_face_repair_pass_20261010.json`, `owned_morph_shader_face_cold_reload_pass_20261010.json`, final `owned_morph_shader_repair_report.json`, final `owned_morph_shader_cold_reload_report.json`, commandlet Unreal logs and watchdogs.
- **Do not call this Android shader PASS.** `-NoShaderCompile` explicitly deferred shader permutations to a real ASTC cook. The historical grey/dark Android face FAIL remains **UNRESOLVED on device** until independent phone recook/render/expressions proof.

## C. Gameplay engineering under integration

- New experiment-only native `AOGKoikatsuPlayableCharacter` source has been added under `Source/OfflineGame/Public+Private/Koikatsu/`. Intended to inherit existing touch/gameplay/movement pawn, discover the already-authored 19 Blueprint mesh components without duplicating them, map movement state to exact native action clips, preserve root motion under CharacterMovement, and expose direct morph control. **C++ compiled-and-linked runtime has NOT yet passed**.
- `Scripts/Gate12Koikatsu/bind_kk_native_action_player_ue583.py` is prepared for reparenting the existing full-quality fixture Blueprint only *after* native C++ compilation. GameMode and original test world are retained. **NOT_EXECUTED**, no player-controlled Android evidence.
- Isolated `Config/DefaultGame.ini` explicitly cooks experimental native action/rest-safe directories for future runtime `LoadObject`; no change to frozen Foundation. Packaging effect **NOT_TESTED**.

### Actual native build obstacles, not policy bans

Windows page file is **system managed** (`C:\pagefile.sys`, previously 5,888 MB allocated); at recovery 7.63 GiB physical, ~2.34 GiB free and ~6.86 GiB free commit. Full engine build unnecessary; UBT was invoked for existing `OfflineGameEditor Win64 Development` with `-MaxParallelActions=1`. UnrealHeaderTool processed the new class successfully and generated files, but subsequent 14-action UBA phase reached monitored physical-memory pressure twice and was stopped before a completed DLL (logs and watchdogs in `Saved/Gate12Koikatsu/NativeGameplayCompiler/`). `-NoUBA` in UE5.8 source disables UBA detouring but **does not** remove the UBA executor, verified against actual engine `ExecutorFactory.cs`.
A single-object targeted MSVC attempt using UBT's already-generated response file and existing PCH returned `C1852 invalid precompiled header`, not yet compiled. An alternate without reused PCH is pending. **Do not conflate a UHT PASS, generated response, or source file with a compiled/integrated class.**

## D. Preservation / production authority

- Frozen Foundation `3f54f07cacbf193e160d0be15a7be5a7bc730c3b` and approved Gate11 Registry v2 untouched; PR #16 draft/unmerged.
- The original local Koikatsu GLB SHA256 `ad06955e6f725c63c6ee014828877a9a8fa3a19479ca29936c58c96ed6d012e0` intact. Six meshes / 25 primitives / 4,611 source morph links / 20 original images preserved.
- Original PC WorldState database SHA256 `4DBB961D45823763EBCE7E16DC62B1102B32B8FFD46003AB61AB05F0EF83554A` unchanged. Original Galaxy S26 Ultra game installed; no signed Foundation update made.
- **Device PASS from earlier checkpoint remains narrowly applicable only to actual rest-safe skeletal sway and stronger upper-body QA, not to new 171 clips or fixed facial material.** Package installer/mount/spawn/rollback remains NOT_TESTED.
- Experimental fixture must remain outside Gate11 `ARTIFACT_READY` until provenance, import/runtime/device acceptance.

## Next increasing-cost validation

1. Keep all native validated `.uasset` assets and local receipt hashes, rerun independent cold-load sequence audit when possible.
2. Resolve incremental source compilation independently of full UE cooking; compile/link state adapter and reparent already-existing player Blueprint without duplicating imported meshes.
3. Cook test-only ASTC on an operation-specific watchdog using existing cooked/cache state, not a RAM-capacity guess. Verify real mobile shader permutations/face/eyes/hair, actual morph curves concurrently with body motion.
4. On physical Galaxy S26 Ultra, prove touch input → actual player movement → distinct visual action transitions and UE renderer frame time, clipping and save-state integrity.
5. Test signed trusted character Pak receipt/mount/select/spawn/rollback separately; transport alone remains insufficient.

## Follow-up evidence — independent native clips, direct MSVC compilation, Android cache invalidation

**Independent UE5.8.3 cold-load PASS:** `verify_kk_native_action_clips_cold_ue583.py` executed in a *new* real Unreal Editor commandlet process with `-NoShaderCompile`. It loaded and inspected **171/171** originally authored `AnimSequence` packages, covering all 19 parts and nine actions, each with the expected original native skeleton, at least six movement bone tracks, no root animation tracks, and matched required action durations. Local receipt: `Saved/Gate12Koikatsu/NativeActionClips/171_native_action_clips_cold_unreal_ue583.json`; redacted repository copy committed under `docs/production/gate12/evidence/koikatsu-native-actions-morph-20261010/`. This is **cold serialization validation**, not proof of Android motion or physically natural locomotion.

**Actual C++ single translation-unit compilation PASS**, whole module still not linked:
- Earlier UBT attempts generated reflected `OGKoikatsuPlayableCharacter` headers successfully but the default 14-action UBA build exhausted physical headroom and stopped safely. The `-NoUBA` flag in UE5.8 does **not** disable the UBA executor, only detouring, according to its own `ExecutorFactory.cs`.
- Reused UnrealBuildTool's precise generated `OGKoikatsuPlayableCharacter.cpp.obj.rsp`, stripped incompatible precompiled-header arguments, invoked MSVC once via `vcvars64.bat`, **in Unreal's actual `Engine/Source` working directory** because relative Unreal include directories otherwise fail.
- `cl.exe` produced a **17,629,860-byte** object file, SHA256 `09a4eb63dad5aecaedde2553ebdac893d935bf0172955139c2fe219bef6b1e0a`, zero compiler/fatal error lines. Actual watchdog had minimum 1,490 MiB free physical and 5,763 MiB free commit. The object remains a **local engineering compilation receipt only**, not part of any linked DLL, Blueprint class or Android APK.
- Source staged under `Source/OfflineGame/Public+Private/Koikatsu/`; the existing asset `BP_KoikatsuFixturePlayable` has **NOT** been reparented (that would require a linked Unreal class); the target map/GameMode have not been phone cooked.
- The scoped Python integration script `Scripts/Gate12Koikatsu/bind_kk_native_action_player_ue583.py` is staged but not Unreal-executed. There is no assertion that input→state→actual movement has been observed on the S26.

**Targeted Android ASTC iterative cook attempted, no full-cook/device PASS:** `run_cached_facial_android_astc_cook_lowram.ps1` invoked a single UE5.8.3 Cook commandlet for the old actual facial-stress map with `-iterate -CookMapsOnly -MaxParallelShaderJobs=1`. Unreal itself reported that changed global packaging ini settings invalidated the legacy iterative cache, so `-iterate` became a **FULL COOK** requiring **843 packages**. While rebuilding changed global shaders it logged a genuine `Out of memory condition has been detected`; it reached **560/843** packages but had a recorded minimum of 584 MiB free physical RAM. Gate12 proactively terminated **only its own native cook PID**, not user apps, to avoid an unintended unbounded full rebuild (local explicit-stop receipt and engine log under `Saved/Gate12Koikatsu/IncrementalFacialASTC/`). Android ASTC **NOT_COMPLETE**, no new APK and no physical shader/face correction evidence. Cook data may be partial; do not package/transfer it as an accepted artifact.

**Five new shareable engineering receipts** under `docs/production/gate12/evidence/koikatsu-native-actions-morph-20261010/`: 171-source-asset SHA audit, 171 native-cold audit, 15 material-native-repair and 15 material-native-cold verification, single C++ object compilation. They exclude raw original GLB, phone user databases and signing credentials. The frozen Foundation, original mobile app, personal saves, Gate11 Registry v2 and draft PR16 stay untouched.

**Remaining real blockers:** link all required C++ Unreal Editor/Android module objects under measured process-level memory pressure; repoint original fixture Blueprint to compiled motion-state native subclass without duplicating imported meshes; targeted mobile shader permutations and package dependencies; S26 animated face+eyes+hair visual comparison and runtime input/state clips; synchronized pose transitions/foot-floor contact; real UE frame time; trusted Pak native receipt/mount/spawn/rollback. No blanket minimum RAM requirement is imposed.
