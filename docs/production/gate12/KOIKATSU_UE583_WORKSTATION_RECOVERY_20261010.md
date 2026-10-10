# Gate 12 — Koikatsu fixture, workstation recovery & GLB repair
Date: 2026-10-10. Fixture: G01-KK-PIPELINE-TEST-0002 (Chika Haruno). Not Gate11-approved. Not redistributable.

## Immediate workstation recovery
- Terminated **only our Gate12 UE test UnrealEditor.exe PID 14636**. Follow-up: zero associated UE/Blender processes and zero ShaderCompileWorker processes.
- The frozen original `D:\UnrealProjects\Offline-Game` remains clean on `3f54f07cacbf193e160d0be15a7be5a7bc730c3b`; no player save or original Android installation targeted.
- **Do not relaunch resource-heavy Unreal/Blender while the user is using this 8 GB RAM / Intel UHD workstation.**

## Prior actual native test
- Unreal 5.8.3 imported the prepared **FBX** to isolated worktree: **46 .uasset files** = 1 skeletal mesh + 1 skeleton + 24 material instances + 20 textures. Imported spine-motion AnimSequence and QA map opened for play with D3D11.
- Defects independently identified: **0 morph targets**, **24/24 materials opaque**, implausible actor bounds `(680,7185,174) cm` half-extents (roughly **144 m width**). Correct visual skinning, transparency and facial expressions FAIL/NOT VERIFIED despite runtime startup.
- Face/body real asset not production-accepted, and the graphics screenshot was obstructed by other foreground content; do not use it as proof.

## Lower-resource source inspection
The original **GLB** at `D:\Tools\GameAssetPipeline\KoikatsuParty\characters\G01-KK-PIPELINE-TEST-0002\conversion\character_unreal_skeleton.glb` has SHA256 `ad06955e6f725c63c6ee014828877a9a8fa3a19479ca29936c58c96ed6d012e0`, size 29,572,372 bytes.
- 6 meshes / 1 skin / 25 primitive/material entries; **21 materials alphaMode=BLEND**, 4 OPAQUE, all double-sided.
- Morph target data exists: 4,611 **primitive-local** target references (not 4,611 distinct named expressions).
- Position accessor values are within ~1.585 metres maximum axis magnitude; no GLB node local translation exceeded 2 metres. Thus the FBX bounds anomaly is *likely conversion-related*; a GLB import must still prove that.
- Exact UE 5.8 Interchange source exposes `UInterchangeGenericMeshPipeline::bImportMorphTargets=true` by default; unlike the prior FBX override, this pipeline can read GLB shape keys.

## Remediation staged WITHOUT launching Unreal again
- `Scripts/gate12_koikatsu_glb_import_validate.py`: **offline-written, not executed in Unreal**; separate destination `/Game/Experimental/Gate12Koikatsu/GLBRecovered_20261010`. Locks original GLB hash and project path, refuses overwrites, uses Interchange GLB (no FBX override), and independently **rejects** missing morphs, opaque-only materials and oversized skinning bounds.
- Running it needs a future user-approved idle period; launching the editor now could again block the workstation. The prepared file is not an accepted fixed .uasset.
- If the GLB passes import gates, next run a live screenshot, visual deformation/facial test and Android isolated-app cook; preserve `com.azrazieliz.OfflineGame`, PR #16 and player saves. No automatic Android install.

| Validation | Verdict |
|---|---|
| Stop intrusive Gate12 Unreal runtime / restore control | PASS (processes eliminated; user interaction not independently verified) |
| Original FBX import into UE 5.8.3 | PASS (46 actual assets) |
| Morph preservation in imported FBX | FAIL |
| Transparent materials in imported FBX | FAIL |
| Plausible imported FBX skin bounds | FAIL |
| Morph and alpha preserved in source GLB | PASS (source inspection only) |
| Repaired GLB UE import/render/animation | NOT TESTED |
| Repaired Android cook/install | NOT TESTED |
| Repaired physical S26 Ultra validation | NOT TESTED |

Do not classify experimental source as ARTIFACT_READY or DEVICE_VALIDATED.
