# Gate 12 — Koikatsu morph shader parent persistence: read-only recovery and guarded native fix

Date: 2026-10-10. Experimental fixture `G01-KK-PIPELINE-TEST-0002`. This report is **not** a device visual PASS, not Gate 11 production acceptance and not permission to update the original game.

## Authoritative context and invariants

- Source recovery: `aff5f4fdeed6a4f11d7e5438ace6208f93e00603`, branch `production/gate12-koikatsu-ue583-fixture-20261010`.
- Frozen Foundation `3f54f07cacbf193e160d0be15a7be5a7bc730c3b`, clean at inspection.
- Source GLB SHA256: `ad06955e6f725c63c6ee014828877a9a8afa3a19479ca29936c58c96ed6d012e0` (byte-exact verified again).
- Physical Galaxy S26 Ultra `R3GYC0LSC7K` is connected. Only original `com.azrazieliz.OfflineGame` is installed; the fixture `com.azrazieliz.gate12fixture` is not.
- Original PC `WorldState.db` SHA256: `4DBB961D45823763EBCE7E16DC62B1102B32B8FFD46003AB61AB05F0EF83554A`, unchanged.
- Available laptop physical RAM at inspection: 1.99 / 7.63 GiB; no UE editor or Android cook launched. Previous full cooks triggered guard below ~500–850 MB available. **Do not override this limit or reduce source art.**

## New objective evidence

A new read-only audit, `Scripts/Gate12Koikatsu/audit_saved_kk_material_ownership.py`, hashed and inspected **15/15 actual saved material-instance .uasset files** enumerated by the prior native morph report. **15/15** include external shader-parent references under `/InterchangeAssets/gltf/`, such as `MI_Default_Opaque_DS`. The older in-editor material usage report also resolves all 15 to base `/InterchangeAssets/gltf/M_Default.M_Default`. This explains why an in-editor set-usage/save call on the shared external shader was inadequate persistence evidence: it did not prove a project-owned shader compiled and saved with MorphTargets usage.

One actual saved face material instance `KK_cf_m_face_00.uasset` SHA256 `3DB5AAE185A0901E0246B704A0C340AE402FC475EC41E312FC0FFA19106F2DC9` was unchanged across the static audit. Native player assets are still present: `BP_KoikatsuFixturePlayable.uasset` (59,955 bytes), `BP_KoikatsuFixtureWorldMode.uasset` (21,845 bytes), and `L_KoikatsuPlayable_UE583.umap` (6,274,327 bytes).

The input GLB retains the source 4096×4096 face image and all the other previously verified source textures, geometry and morph references. **These are ownership/persistence diagnostics, not proof of visual parity.** Prior physical Android grey face remains a FAIL; native shader persistence remains NOT_TESTED.

## Isolated implementation added, not yet Unreal-executed

1. `audit_saved_kk_material_ownership.py`: native-runtime-free physical file+SHA inventory; refuses paths outside the isolated project; JSON receipt in `Saved/Gate12Koikatsu/material_parent_ownership_static_audit.json`. **PASS_STATIC_ASSET_INVENTORY_ONLY**. Python compilation passed.
2. `repair_kk_morph_shader_ownership_ue583.py`: **deferred native commandlet** for an approved high-memory Unreal 5.8.3 host. Requires `G12_MATERIAL_REPAIR_APPROVED=1` and refuses the known 8 GiB laptop. Duplicates exact Interchange shader-base and material-instance parent chain into fixture-owned `/Game/Experimental/Gate12Koikatsu/MaterialShaderOverrides`, explicitly enables MorphTargets usage on the project-owned shader, reparents only the 15 imported material instances and preserves their texture/vector/scalar override arrays. Backs up leaf .uasset bytes privately and records before/after hashes; in-process rollback on error. **Python compilation PASS; native execution NOT_TESTED.**
3. `verify_kk_morph_shader_cold_reload_ue583.py`: **independent subsequent UE process**, read-only; checks exact saved parent chains, on-disk base material files/SHA and material MorphTargets usage after cold reload, and refuses to pass if any reference persists under the old imported shader chain. **Python compilation PASS; native execution NOT_TESTED.**

The scripts do not alter glTF source, original bitmaps, saved gameplay DB, frozen Foundation, approved Registry v2 or the installed APK. No copy into active production registry.

## Next native acceptance sequence (requires authorized capable host)

- Clone/check out the isolated branch, copy approved fixture assets and local Saved prerequisites without publishing private source or backups. Verify UE5.8.3, Android SDK/NDK and safe physical RAM first.
- Run the repair script in UE5.8.3 Editor commandlet with `G12_MATERIAL_REPAIR_APPROVED=1`. Capture actual exit code, compile log, new .uasset SHA and script JSON; failure remains FAIL.
- Close editor completely. Start a **fresh** UE5.8.3 process and execute the cold-reload verifier, capture on-disk proof. Only then mark material persistence PASS (not visual rendering).
- Android ASTC cook in isolated `com.azrazieliz.gate12fixture`, install on S26, compare facial expression close-up with prior grey-face failure, verify eyes, hair cutout/translucency and named morph changes; capture physical screenshots, video and Unreal warnings. Then test player movement and actual action states separately.
- Do not update original `com.azrazieliz.OfflineGame` or mark Pak runtime activation PASS without their independent signed-install, receipts and rollback evidence.

## Strict status

| Gate | Current status |
|---|---|
| Recovery SHA / isolated worktree / frozen Foundation | PASS |
| Real source GLB SHA and saved 15 material assets | PASS |
| Static external shader-parent finding | PASS (15/15) |
| Repair and cold reload in UE5.8.3 | NOT_TESTED — capable authorized host required |
| Fixed face/eye/hair on physical S26 | NOT_TESTED (earlier facial runtime FAIL preserved) |
| Playable phone input / genuine action states / UE FPS | NOT_TESTED |
| Trusted native Pak installation and spawn | NOT_TESTED |
| Original app, private user data preservation | PASS for read-only checks this session |

Historical videos, full reports and five evidence directories remain untouched and authoritative. No previously successful experimental native skeletal motion result is downgraded.
