# Gate 12 — Koikatsu GLB quick retest (2026-10-10)

**Result: ENGINE IMPORT NOT COMPLETED — both attempts interrupted for workstation memory safety.**

## Exact project/source and preservation
- Engine UE 5.8.3, installed source metadata changelist `58210709`.
- Test worktree: `D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010`; integration branch `production/gate12-koikatsu-ue583-fixture-20261010`, based on frozen Foundation `3f54f07cacbf193e160d0be15a7be5a7bc730c3b`.
- Real original Koikatsu KKBP GLB SHA256 `ad06955e6f725c63c6ee014828877a9a8fa3a19479ca29936c58c96ed6d012e0`, unchanged.
- Original player database `D:\UnrealProjects\Offline-Game\Saved\OfflineGame\WorldState.db` SHA256 `4DBB961D45823763EBCE7E16DC62B1102B32B8FFD46003AB61AB05F0EF83554A`, unchanged. Frozen Foundation checkout clean and frozen tag resolves to frozen commit.
- No modification to phone or installed app; PR #16 remains separate, untouched.

## Native attempts and exact evidence
1. **Full GLB** `Saved/Gate12Koikatsu/glb_recovery_engine.log`, script `Scripts/gate12_koikatsu_glb_import_validate.py`, guard `Saved/Gate12Koikatsu/glb_recovery_process_monitor.json`.
    - Imported via Unreal Interchange on the actual isolated project. Hash and isolation assertions PASS.
    - Native log warns that six non-root skinned mesh nodes' parent transforms will not affect their skinned meshes.
    - Memory monitor killed process after **67 seconds**, free system RAM falling to **291 MB**; process exit -1 is from safety stop, not proof of native import failure.
2. **GLB lite QA copy** `Saved/Gate12Koikatsu/character_glb_lite_qa_only.glb`, SHA256 `f6fd03ba589721d582b903a14b2ed43f51f67c62d1e5fc44f8384059c3a062ad`, engineering-only transformation.
    - Keeps all 25 materials (21 BLEND, 4 OPAQUE), the mesh geometry, skin, and representative face morph target names, but reduces **4,611 primitive-local target references to 62**. Does not represent an acceptable production character or the full facial capability.
    - Unreal Interchange entered import; isolation and source hash assertions PASS.
    - Monitor `Saved/Gate12Koikatsu/glb_lite_qa_process_monitor.json` stopped the process at **87.1 seconds**, free RAM **751 MB**, configured cutoff 800 MB. Engine log: `glb_lite_qa_engine.log`.
    - Neither import produced a complete accepted SkeletalMesh, material/morph/bounds report, or finished GLB `.uasset` directory. No screenshot proves visual fidelity.

## Independent verdict matrix
| Test | Verdict |
|---|---|
| Source GLB format, morph and alpha metadata | PASS (source-level only) |
| UE 5.8.3 isolated project, SHA and import initiation | PASS |
| Full GLB successfully imported | NOT COMPLETED — safety cutoff |
| Lite GLB successfully imported | NOT COMPLETED — safety cutoff |
| GLB morph targets preserved in native UE asset | NOT TESTED |
| GLB material transparency preserved natively | NOT TESTED |
| GLB skeleton dimensions and deformation | NOT TESTED |
| Unreal runtime scene correctly rendered | NOT TESTED for GLB |
| Android cook, APK and device test for repaired GLB | NOT TESTED |
| Existing Foundation and user save preservation | PASS |

**Environment:** Windows laptop with 8 GB total RAM / Intel UHD. Do not relaunch heavyweight Unreal on the occupied workstation without a stronger machine, more available RAM, or a fundamentally lower-memory import strategy. Workstation safety takes precedence over fixture acceptance. No Gate 11 `ARTIFACT_READY` or `DEVICE_VALIDATED` designation.

**Final process check:** zero Gate12 UnrealEditor/UnrealEditor-Cmd/Blender processes, about 3.5 GB free RAM after guards.
