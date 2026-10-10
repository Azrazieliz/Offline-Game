# Gate 12 — Native gameplay linkage and Android ASTC cooker recovery

Date: 2026-10-10. Isolated engineering branch `production/gate12-koikatsu-ue583-fixture-20261010`; source checkpoint `b116bee5b45f0193abb3bd78ff8381655ac48dbe`. Experimental character **not** Gate 11 ARTIFACT_READY. No Foundation or saved-data modification authorized/performed.

## A. Completed and independently Unreal 5.8.3 validated

**Win64 editor project module compiler/linker: PASS.** The previous 8 GiB laptop blocker was not a universal hardware limit. `UnrealBuildTool OfflineGameEditor Win64 Development -MaxParallelActions=1 -NoPCH -NoUBA -NoXGE` successfully compiled all required native .cpp unity and non-unity translation units and linked `UnrealEditor-OfflineGame.dll`. Missing direct include `Components/InputComponent.h` in the existing world's player input C++ implementation was exposed by `-NoPCH`, diagnosed by actual compiler errors, and corrected **in the isolated Gate12 copy only**. Incremental rebuild reused the earlier compiled modules; the final no-PCH build again returned **Result: Succeeded** after a component-name parser correction. DLL SHA256 `1e0f08f6d339c467aabbe1f4d85033efc06a0905ee3ebd6f49e9ddfae6f22044`.

**Existing 19-component playable Blueprint: PASS native reparent, compiled/saved, independent cold load PASS.** Exact original `/Game/Experimental/Gate12Koikatsu/PlayableQA/BP_KoikatsuFixturePlayable` was reparented to `/Script/OfflineGame.OGKoikatsuPlayableCharacter` rather than recreated. The prior full-quality .uasset was SHA-backed up privately before editing. UE5.8-specific Python API correctly uses `BlueprintEditorLibrary.get_blueprint_parent_class` and `unreal.get_type_from_class`. All 19 SCS template names carry a harmless `_GEN_VARIABLE` suffix, normalized only for authoritative primitive identity checking. `Source/OfflineGame/Private/Koikatsu/OGKoikatsuPlayableCharacter.cpp` accepts both template/runtime suffix variants. Native cold reload independently confirmed exact **19/19 original SkeletalMesh names and native skeletons**, new direct parent, and `BP_KoikatsuFixtureWorldMode` still points to this actual playable Blueprint as default pawn. No Android touch/movement-state transition has been claimed.

**Reusable conversion descriptor: PASS static and source-hash verified.** Existing Gate12 `build_playable_descriptor.py` now accepts optional 171-clip native cold verification evidence to map Idle and nine real engine-authored procedural engineering action types (Walk/Run/Jump/Fall/Land/TurnLeft/TurnRight/Dodge/Action) across 19 distinct native part skeletons and stable character ID `G01-KK-PIPELINE-TEST-0002`; all 19 with ten maps present, original GLB SHA verified and Android states marked *zero approved*. Separate pre-existing lossless source exporter and Blueprint author script preserved; no unsupported generic production claim.

**Android_ASTC full facial QA map cook: PASS (content/shaders only).** UE 5.8.3 cooked map `/Game/Experimental/Gate12Koikatsu/FullFidelityQA/Maps/L_KK_StressFace_UE583` in a true single headless cook commandlet, from 843/843 evaluated packages, 800 cooked and 43 skipped by platform, 43/43 shaders complete, **Success - 0 error(s), 0 warning(s)**. **NO APK, no original game update, NO phone-rendered face proof**.

### Root cause and successful host-only fix

Earlier ASTC cook attempts aborted at 387 and 560 packages because the cooker had low physical headroom and, with default `PackagesPerGC=0`, deferred/suppressed collections. Native engine `Engine/Config/BaseEditor.ini` documented `[CookSettings]` GC controls. Gate12 added project-local `Config/DefaultEditor.ini` with `PackagesPerGC=12`, `SoftGCTimeFractionBudget=0.20`, `SoftGCMinimumPeriodSeconds=5`, `MemoryMinFreePhysical=1536`, `MaxConcurrentShaderJobs=1`, `CookProcessCount=1`; original mesh, 4K textures, shaders, morphs and input systems unchanged. Logs showed repeated `GarbageCollection... (Exceeded packages per GC)` and the complete native cook. 8 GiB machine lowest sampled free physical memory **458 MiB** and Windows commit headroom **4,745 MiB**; page file was already system managed, so no speculative system-wide change was needed. Raw local logs and watchdog in `Saved/Gate12Koikatsu/IncrementalFacialASTC/`; shareable receipt in `docs/production/gate12/evidence/koikatsu-final-20261010/android_astc_facial_map_cook_pass.json`.

## B. Real unresolved Android installation issue (distinct from cook/RAM)

`UnrealBuildTool OfflineGame Android Development` failed fast with **“Missing files required to build Android targets. Enable Android as an optional download component in the Epic Games Launcher.”** The existing installed UE5.8.3 machine lacks `Engine/Build/Android` and `Engine/Binaries/Android`; Android SDK, NDK 27.2 and JDK are installed. A separate actual AutomationTool `BuildCookRun -skipbuild -skipcook -stage -package` confirmed **“GetBuildPlatform: No BuildPlatform found for Android”**. No new Android ARM64 binary, signed APK, installation or S26 facial/gameplay test was possible via this incomplete engine installation. Original previously produced Android APK and .so backed up with checksums in private `Saved/Gate12Koikatsu/`, not overwritten or treated as new gameplay.

**Action required to unblock:** install only **Android Target Platform** optional files for the already installed Epic Games Launcher Unreal Engine 5.8.3 (Launcher → Unreal Engine → Library → UE 5.8 tile menu → Options → Android Target Platform → Apply), then rerun the native Android UBT with the existing operation-specific GC profile and full QA app cook/packager. Do not conflate SDK/NDK with the separate engine Android target component.

## C. Acceptance matrix

| Target | Evidence |
|---|---|
| Original full-quality source and exact animation skeletons | **PASS** earlier 19 + 171 UE cold receipt, unmodified |
| Win64 fully native game movement/action adapter linked | **PASS** |
| Existing full-quality 19-part player BP reparent / GameMode / UE cold reload | **PASS** |
| Source descriptor maps all 9 procedural engineering action types | **PASS STATIC**; animation natural quality NOT_APPROVED |
| Saved facial shaders MorphTargets usage 15/15 cold | **PASS** earlier |
| New facial ASTC QA cook including shaders | **PASS** |
| Android ARM64 Unreal native executable build | **FAIL: Engine Android optional platform absent** |
| Android application stage/package/sign/install | **FAIL/NOT_TESTED: target platform missing** |
| Real phone touch/animation state, mouth/eyebrow, eyes/hair/clothes, UE FPS, clipping, stability | **NOT_TESTED** for new integration |
| Trusted Pak activation / production acceptance Gate11 | **NOT_TESTED / NO APPROVAL** |

All work remains on the isolated Gate12 branch. Frozen Foundation at `3f54f07cacbf193e160d0be15a7be5a7bc730c3b` remains clean; original PC `WorldState.db` SHA256 `4DBB961D45823763EBCE7E16DC62B1102B32B8FFD46003AB61AB05F0EF83554A`, original Galaxy S26 Ultra app `com.azrazieliz.OfflineGame` and PR#16 draft/unmerged preserved. Provide source/evidence link and accurate statuses to Gate00 issue #19. Never elevate this fixture to Gate11 ARTIFACT_READY.
