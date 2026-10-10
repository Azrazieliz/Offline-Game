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
