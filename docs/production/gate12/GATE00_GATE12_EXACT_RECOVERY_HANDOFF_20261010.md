# GATE 00 / Replacement GATE 12 — Authoritative Koikatsu UE 5.8.3 Recovery Handoff

**As of:** 2026-10-10 ~21:03 Europe/Paris (19:03 UTC).  
**Role:** Gate 12 engineering fixture evidence and implementation. This is a continuation, **not** a new Foundation or P0 project.  
**Engineering base commit independently checked locally AND through GitHub:** `8e67ee2a7502ae8d2b6430a5aaa70e66c4b81f1a` on `production/gate12-koikatsu-ue583-fixture-20261010`.  
**Repository:** https://github.com/Azrazieliz/Offline-Game  
**Existing Gate 00 technical handoff:** https://github.com/Azrazieliz/Offline-Game/issues/18  
**Existing Gate 11 real-production-intake blocker:** https://github.com/Azrazieliz/Offline-Game/issues/17

> **READ THIS FIRST — no drift.** The real full-resolution Koikatsu experimental fixture imports, renders and performs basic/stronger skeletal motion on the physical Galaxy S26 Ultra. A UE 5.8.3 player-character Blueprint has also been created and compiled using the existing Foundation mobile-control pawn, but this *playable Blueprint has not been cooked to a working Android APK*. **It is NOT yet proven that touch input moves this Koikatsu player character in the game, or that facial animation displays correctly**. No second original export has been through the entire native flow. A source-only C++ action-state adapter is NOT compiled. The experiment is NOT `ARTIFACT_READY` or globally `DEVICE_VALIDATED`. Exact distinctions below are mandatory.

## 1. Immutable preservation and location

- **Frozen Foundation project**: `D:\UnrealProjects\Offline-Game`. HEAD verified clean at `3f54f07cacbf193e160d0be15a7be5a7bc730c3b`. Freeze tag `foundation-v1-freeze-2026-10-08`; annotated Git tag object may resolve to a different object SHA until dereferenced; do **not** mistake that for a changed foundation commit.
- Foundation player database (PC): `Saved\OfflineGame\WorldState.db` SHA256 `4DBB961D45823763EBCE7E16DC62B1102B32B8FFD46003AB61AB05F0EF83554A`, unchanged throughout tests. Device save is separate.
- **Worktree for all experimentation**: `D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010`, branch `production/gate12-koikatsu-ue583-fixture-20261010`. Original Foundation not merged or migrated; **PR #16 stays draft/unmerged**.
- UE install: `D:\Epic Games\UE_5.8\Engine`. `OfflineGame.uproject` `EngineAssociation=5.8`; exact UE 5.8.3 verified in prior test; verify loaded editor executable again before any future acceptance.
- ADB: `C:\Users\mimim\AppData\Local\Android\Sdk\platform-tools\adb.exe`. Real device serial `R3GYC0LSC7K`; Galaxy S26 Ultra, `SM-S948B`, Android 16, ARM64.
- **Current physical phone**: original game package `com.azrazieliz.OfflineGame` present; experimental `com.azrazieliz.gate12fixture` **not installed at this latest checkpoint** (it was installed for earlier experiments, later deliberately removed). Do not describe the current phone as running the fixture.
- Laptop has **7.63 GiB (~8 GB) RAM**. Multiple native UE 5.8.3 cook launches triggered safety-stop when free RAM fell under ~500–850 MB. Do NOT weaken/remove RAM protection, restart mass cooks on this host, alter system pagefile unapproved, or reduce game asset fidelity to accommodate the laptop. Build host 16 GB minimum to attempt, 32 GB preferable; not verified as an exact threshold.
- Latest branch has unrelated **untracked historical experiments**, excluded from accepted handoff. Do not recursively `git add -A` or delete untracked real assets; use targeted staging.

## 2. Actual fixture and immutable source

- Fixture ID `G01-KK-PIPELINE-TEST-0002`, appearance Chika Haruno. *Experimental engineering fixture*, NOT approved creative production content.
- Original local GLB `D:\Tools\GameAssetPipeline\KoikatsuParty\characters\G01-KK-PIPELINE-TEST-0002\conversion\character_unreal_skeleton.glb`.
- Source: **29,572,372 bytes**, SHA256 **`ad06955e6f725c63c6ee014828877a9a8fa3a19479ca29936c58c96ed6d012e0`**.
- Original structure **1 glTF skin, 6 meshes, 25 source primitives, 25 used material slots, 20 embedded images (including 4096×4096 face), 4,611 referenced shape/morph targets**. All counts based on actual source parser and manifests; they are NOT counts of independent playable game actions.
- Original GLB, private character source/card and phone backups **not committed to public GitHub**. Derived authorized-to-test Unreal assets, scripts, manifests and redacted evidence are in experimental branch.
- Earlier intentionally degraded diagnostic imports (256×256 downsampling/material-loss, white model, broken camera/black screen) are **historical FAIL**, never production reference.

## 3. Independent acceptance matrix

| Work item | Strict outcome | Evidence / limitation |
|---|---|---|
| Real GLB import to UE5.8.3 skeletal meshes | **PASS for fixture** | 19 full-resolution skeletal assets representing source parts; source/native audit and actual Android static render |
| Source geometry, original texture encoded bytes, materials, morph links | **PASS source-preservation** | Full-fidelity shader/texture audit; 25 primitive GLBs; image SHA checks; no forced source reduction |
| Universal Koikatsu import from any card/exporter | **NOT PROVEN** | Tested **one single-skin** GLB; multi-skin/other skeleton schemas must be assessed separately |
| Generic source sharder `build_kk_export_generic.py` | **PASS on fixture** | Regenerated **25/25 SHA-identical GLB shards**, source 6/25/4611/20 preserved |
| New-character safe intake `stage_new_koikatsu_character.ps1` | **PASS on fixture** | `KKFixturePreProduction0002` staged source unchanged in 3.83 sec, stable ID/hash; no UE commandlet |
| `import_kk_character_guarded.ps1` new-path staging | **PASS PLAN-ONLY** | 25 hashes and safe unique paths validated, **-Execute for new character NOT TESTED** |
| UE native rest-pose body animation creation | **PASS fixture** | 19 per-skeleton exact animations, 61 samples/2 sec; original root translation/scale/orientation retained, zero root tracks |
| UE scene serialization/binding | **PASS fixture** | 19/19 exact matching skeleton+clip in separate QA scene |
| UE Android ASTC cook for basic animation | **PASS** | 649/649 packages, exit 0; isolated package |
| Android APK build/install, real basic body playback | **PASS physical S26** | 12-sec real phone video; 14.74% silhouette difference opposite pose, ~0.285% at identical cycle phase |
| Advanced spine/arm/neck movement | **PASS for sampled range only** | 19 exact-skeleton clips; 12° spine, 36° upper arm, 22° forearm, 9° neck; 15-sec real S26 video, 25.111% opposite-phase silhouette difference, 0.644% two-second repeat |
| Advanced complete clipping/collision and locomotion | **NOT TESTED** | Only QA sways; no verified walk/run/jump/dodge transitions, legs/feet collision stress |
| Facial animation curve authoring | **PASS assets only** | 8 named float curves targeting actual mouth and eyebrow morphs in imported meshes |
| Facial rendering in earlier physical close-up | **FAIL** | Face became grey/dark; UE runtime warnings about missing `MorphTargets` material usage and default shader substitution |
| Morph material flags check/fix | **PASS in-editor inspection; persistence UNVERIFIED** | 15 morph-materials inspected, 1 flag switched, all 15 true in session; material on-disk Git blob stayed unchanged at audit; cold-reload/morph-shader compilation not independently demonstrated |
| Corrected facial scene on phone | **NOT TESTED** | Cook interrupted safely at 364/649 packages when laptop RAM insufficient |
| 60-second Android stress non-crash | **PASS limited stability** | Continuous PID; ~503–505 MB total PSS; thermal status 0; battery 28.8→28.5 °C |
| Actual UE Vulkan renderer FPS/long soak | **NOT TESTED** | Android UI gfxinfo is not reliable UE renderer FPS evidence |
| Real player Blueprint / GameMode / starting-world clone | **PASS UE native asset authoring** | 19 UE skeletal components + exact idle clips, inherited movement class, default pawn and copied map saved/Blueprint compiled |
| Remove inherited tutorial mannequin visual | **PASS UE authoring** | Exactly 1 inherited `CharacterMesh0` hidden, 19 imported parts retained |
| UE Android cook/install of playable character | **BLOCKED** | RAM guard; editor startup/asset registry down to 422 MB free. Later incremental attempt -iterate/-CookMapsOnly/-MaxParallelShaderJobs=1 stopped after ~93 sec below 850 MB. No playable APK |
| Real physical touch-controlled KK player | **NOT TESTED** | No Android playable build or observable control input proof |
| C++ input-dependent idle/walk/run/airborne/dodge adapter | **SOURCE-ONLY, UNCOMPILED** | Staged under `Scripts/Gate12Koikatsu/NativeGameplayCandidate/`, outside active Source; no real action clips |
| Galaxy S26 ARM64 native morph computation | **PASS** | Real Android NDK binary, 25 primitives/4611 morph position accessors/61 frames; 12,740,399 weighted updates; zero invalid; 22 ms computation final repeat |
| Actual experimental UE cooked Pak | **PASS engineering packaging** | UnrealPak Android_ASTC legacy Pak, 234 files/36,633,270 cooked input bytes → 9,823,496 byte compressed Pak, UnrealPak -List matches all 234 |
| Pak ADB transfer + on-phone SHA check | **PASS** | Device SHA matches `f0e998038a124878e0611f34610b517c3697fe791f6168bdeedbd76fb6357b22`; temporary orphan files removed |
| Pak trusted receipt/register/mount/spawn in existing app | **NOT TESTED** | Merely copying Pak to phone is NOT installing it into game; no runtime trusted receipt, installed registry, gameplay character |
| Foundation package ID/signing match for in-place update | **PASS verification** | Existing, frozen-build and experimental signing cert same `9bc439712a0a460cc4703ebf254aaaf9bf3cc6e67b0ced6fc12162ed3ca3b9ad`; exact package handling tested |
| Same-package APK update with save protection | **PASS dry-run only** | `safe_update_foundation_v1.ps1` accepts legitimate package, rejects unrelated fixture, backs up sensitive saves before explicit `-Apply`; **no in-place update installed** |
| Gate 11 source provenance and registry approval | **NOT AUTHORIZED** | Engineering fixture never `ARTIFACT_READY` |
| Frozen Foundation, saved game protection | **PASS last check** | PC Foundation clean at frozen commit, PC DB hash unchanged; original phone game present; sensitive phone DB backups local only |

## 4. Exact outputs, source workflow and commands

**Authoritative guides — read in chronological order**:

1. `docs/production/gate12/KOIKATSU_UE583_BLACK_SCREEN_REPAIR_PHYSICAL_S26_20261010.md` — earlier camera/rendering repair; do not revert.
2. `docs/production/gate12/KOIKATSU_NATIVE_REST_SAFE_ANIMATION_S26_PASS_20261010.md` — true physical S26 motion PASS.
3. `docs/production/gate12/KOIKATSU_FACIAL_STRESS_S26_DIAGNOSTIC_20261010.md` — stronger motion PASS; facial shader FAIL.
4. `docs/production/gate12/KOIKATSU_EXPORT_TO_PLAYABLE_CHARACTER_FINAL_PASS_20261010.md` — reusable sharding/Blueprints and RAM blockers.
5. `docs/production/gate12/KOIKATSU_PHONE_CPU_OFFLOAD_PLAYABLE_INTEGRATION_20261010.md` — actual ARM64 phone geometry compute.
6. `docs/production/gate12/KOIKATSU_FOUNDATION_V1_UPDATE_AND_SOURCE_PRODUCTION_20261010.md` — private-save-safe dry-run update, verified legacy Pak production/phone transfer, single-command source intake.

**Real production-oriented GLB intake** (PowerShell, after an actual Koikatsu-created and exported GLB exists):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Scripts\Gate12Koikatsu\stage_new_koikatsu_character.ps1" -SourceGlb "D:\path\to\your_export.glb" -CharacterId "KK_Original001"
```

This only stages *source/interchange bytes* with hashes; no native UE character appears yet. It does not need Unreal Editor, and accepts only the validated one-skin source contract.

**Direct inspect / hash-gated source sharding**:

```powershell
& "D:\Tools\Python311\python.exe" "D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Scripts\Gate12Koikatsu\build_kk_export_generic.py" --source-glb "D:\path\to\your_export.glb" --character-id "KK_Original001" --output-root "D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu\NewCharacters" --inspect-only
```

Generate shards with same command omitting `--inspect-only`. Never overwrite a preexisting character folder. Original exported GLB remains immutable and gets SHA256 provenance.

**Plan-only native import** (verified 25/25 on fixture):
`Scripts/Gate12Koikatsu/import_kk_character_guarded.ps1 -Manifest "PATH_TO_manifest_full_fidelity_primitives.json"`.
`-Execute` is guarded but NOT exercised for *another* character and must be executed only on authorized isolated staging when host resources permit.

**Real player BP saved at**:
- `/Game/Experimental/Gate12Koikatsu/PlayableQA/BP_KoikatsuFixturePlayable`
- `/Game/Experimental/Gate12Koikatsu/PlayableQA/BP_KoikatsuFixtureWorldMode`
- `/Game/Experimental/Gate12Koikatsu/PlayableQA/Maps/L_KoikatsuPlayable_UE583`
- `Scripts/Gate12Koikatsu/build_playable_descriptor.py` and `author_kk_playable_from_descriptor.py` generalize character-ID native Blueprint assembly, **validated for this fixture only**.
- `Scripts/Gate12Koikatsu/NativeGameplayCandidate/OGKoikatsuPlayableCharacter.h/.cpp` implements potential exact-skeleton movement state switch and `SetExpressionMorph`; NOT compiled, used or validated.

**Previously phone-validated animation map**: `/Game/Experimental/Gate12Koikatsu/FullFidelityQA/Maps/L_KK_NativeRestSafeSway_UE583`. Keep as the experimental default rather than the failed facial QA map or unbuilt playable world.

**Phone-native compute** (genuine ARM64 executable, no Unreal Editor):
```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "Scripts/Gate12Koikatsu/PhoneCompute/run_on_galaxy_s26.ps1" -SourceGlb "D:\path\to\your_export.glb" -CharacterId "KK_Original001"
```
Physical Android computation is useful for source geometry/morph analysis, **NOT** an alternative to UE Windows import/shaders/cook.

**Experimental Pak build, package receipt and source-update plan**:
- `Scripts/Gate12Koikatsu/package_cooked_character_pak.ps1` produced and verified one UnrealPak against *already-cooked* UE5.8.3 files. No Pak installed through game's trusted runtime.
- `Scripts/Gate12Koikatsu/safe_update_foundation_v1.ps1` is a *protected dry-run* unless `-Apply`; **do not invoke -Apply without explicit Gate 00/user approval, verified original signing cert and full phone save backup**.
- Original phone-installed APK backup and personal DB backups remain local under `Saved/Gate12Koikatsu/FoundationV1InPlaceUpdate_Safeguards/`. **Never publish these to GitHub or include them in a public handoff archive.**
- Original phone app installed APK: 169,804,278 bytes, SHA256 `87e703e2f9aaaf2a0d0778271320a84a06facd4c3a73177060bcf0e1dcb46059`. Device `WorldState.db` SHA256 `220a61bf8d23b098706c0eea74b95897e5bb64723b1943977644de21356318fa`, distinct from unchanged PC save.
- Frozen optional content host already includes `Runtime/OGOptionalPackageHost.cpp`, `Runtime/OGLocallyInstalledPackageProvider.cpp`, `Runtime/OGGameCoreSubsystem.cpp`; package config `bUseIoStore=False`, `bUseZenStore=False`, `bUsePakFile=True`. Native trusted receipt, staged dependency closure and installer action are required; *manual file transfer does not authorize registration*.

## 5. Real evidence entry points in GitHub

Under `docs/production/gate12/evidence/` on this branch:
- `koikatsu-restsafe-s26-20261010/` — genuine 12-second S26 recording, full-quality screenshot, frame metric JSON, two motion phases.
- `koikatsu-stressface-s26-20261010/` — 15-second physical recording, failed facial screenshot, UE Android runtime log, 60s memory/thermal and 2s motion loop JSON.
- `koikatsu-generalized-intake-20261010/` — source hash and 25/25 shard parity, intake/Blueprint evidence.
- `koikatsu-phone-cpu-20261010/` — native ARM64 phone compute source/run receipt, GLB phone hash and totals.
- `koikatsu-incremental-foundation-20261010/` — redacted Pak, source intake and dry-run update receipt; **private phone backups excluded**.
- Native commandlet JSONs under local `Saved/Gate12Koikatsu/`: `rest_pose_sway_authored_report.json`, `rest_safe_animated_map_report.json`, `facial_stress_authoring_report.json`, `playable_blueprint_authoring_report.json`, `playable_blueprint_visual_finalize_report.json`, `morph_material_usage_fix_report.json`, `android_playable_kk_cook_monitor.json`, `android_stressface_material_fixed_monitor.json`. Preserve them.

Earlier GitHub commits on same branch: `4c9b4fb` skeletal fix, `4325720` actual phone motion evidence, `8aecdc4` facial-stress diagnostic, `e1f9894` generic sharder/player Blueprint, `204461d` phone ARM64 native offload, `8e67ee2` signed update and Pak workflow. GitHub verified 8e67ee2 before this handoff.

## 6. What the next Gate 12 MUST do (not a restart)

**Gate 00 should issue a newly scoped technical Gate 12.** Keep Gate 11 Registry v2, historical compatibility, completed P0 approval and frozen Foundation intact. Do not merge draft PR #16, import sample fixture into approved creative registry, or repeat old source-only P0 tests.

**Priority 1 — Build host**: obtain a sufficiently provisioned authorized UE5.8.3 Windows build host or explicit safe resources; the 8GB laptop repeatedly fails legitimate Android cooks. Do not default to modifying system memory settings or downsampling final quality.

**Priority 2 — facial/material closure**: cold-reload the changed native material and verify actual `MATUSAGE_MORPH_TARGETS` persists, correct all face/eye/tooth/tongue/eyebrow default-material substitution warnings, preserve source skin/eyes/hair shader characteristics, recook a close-up test and compare full-resolution Android screenshots and expression curves. Previous in-memory pass was not a complete persisted shader fix.

**Priority 3 — controlled real player**: use *already-authored* `BP_KoikatsuFixturePlayable` + GameMode + actual starting-world clone; cook/install on S26 with separate `com.azrazieliz.gate12fixture`. Observe phone touchscreen input causing controlled avatar movement and camera response. Verify native mesh segment alignment, inherited prototype hidden, collision bounds, no major hair/clothing detachment. Do not mark game control PASS from Blueprint compilation alone.

**Priority 4 — real action animation**: compile/review or replace staged `NativeGameplayCandidate` only after source review; author exact native skeleton idle/walk/run/jump/turn/dodge clips (distinct actions, not the existing 19 independent body-part idle clips), synchronize attachments, target blend states, expressions and action commands. Record input→state→visible pose changes on physical S26. Avoid unverified skeleton retarget flags.

**Priority 5 — credible visual upgrade, full fidelity**: improve Koikatsu-to-UE look via faithful mobile-ready skin/eye/toon shader, correct alpha cutouts vs transparency, double-sided hair only where appropriate, normals/tangents, eye catchlights, color-space, stable exposure/lighting and mobile shadowing; compare screenshots on target. Preserve full source art, all morphs and editable materials; introduce optional quality profiles / target-device LOD, **never sacrifice assets just because laptop RAM is low**.

**Priority 6 — truly usable content delivery**: build one signed Android base update (only with authorized `-Apply` after save and cert check) implementing a user-facing trusted content installer, versioned per-character Pak manifests/hashes/dependency receipts, character selector/spawn. Validate native mount, load, animation, visibility, rollback, saves. Do not assume Pak phone transport = game integration. Gate 11 approval/provenance is a separate production condition.

**Priority 7 — final acceptance evidence**: exact build/asset SHA256, explicit engine version, native import, player movement state transitions, all material/morph facial visuals, S26 phone video, actual UE render frame time/FPS and memory/thermal observations, saves byte hashes, rollback and startup. Update Gate 00 GitHub tracking and close only the legitimately passed gates.

## 7. Continuation command for the replacement Gate 12

> **GATE 12 — KOIKATSU UE 5.8.3 PRODUCTION CONTINUATION.** Read this file and six chronological authoritative gate12 documents plus committed `docs/production/gate12/evidence/`; access `D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010` and its `Saved/Gate12Koikatsu` evidence. Continue from independent engineering base commit `8e67ee2a7502ae8d2b6430a5aaa70e66c4b81f1a` or a newer verified handoff commit on branch `production/gate12-koikatsu-ue583-fixture-20261010`. Preserve clean frozen Foundation `3f54f07cacbf193e160d0be15a7be5a7bc730c3b`, original phone APK/user saves, PR #16 draft/unmerged and approved Gate11 registry. Do not recreate the import/sharding/phone morphology tests or substitute video sprites. The test fixture is genuine 3D Unreal assets, but player-controlled Android KK runtime, facial material closure, actual independent walk/run/jump/dodge animations, and optional-Pak trusted in-game installation remain unproven. Use a safe larger-memory UE build host to finish those tests. Strict PASS/FAIL/NOT_TESTED evidence; non-destructive experimental branch; no production `ARTIFACT_READY` absent Gate 11 acceptance. Keep full original visual detail; no 8GB-laptop-driven reductions. Gate00 tracking: issue #18 (tech) and #17 (Gate11 intake). Report exact hashes, phone recordings and source files; do not classify staging/plans as installed playable game content.

*This recovery handoff was compiled from current checked native worktree, saved UE test reports, physical-device reports and GitHub verified commits, not reconstructed by starting Foundation again.*
