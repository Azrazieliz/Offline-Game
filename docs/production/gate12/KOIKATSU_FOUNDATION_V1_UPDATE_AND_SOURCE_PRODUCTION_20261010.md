# Gate 12 — Foundation v1 in-place update feasibility, content bundles, and immediate Koikatsu production

**Date:** 2026-10-10. **Scope:** isolated Gate12 engineering branch only. No change to frozen Foundation `3f54f07cacbf193e160d0be15a7be5a7bc730c3b`; no merge to PR #16; no Gate11 production registration.

## Direct user answer

Yes, a *properly built* Unreal 5.8.3 update can replace the original Android application's APK **in place** while retaining saves. This must be a package signed with the original certificate, retaining the exact `com.azrazieliz.OfflineGame` package ID, and verified with a dry-run before actual installation. **An already-existing character PAK cannot currently be treated as a game-playable update by simply copying it into phone storage**; runtime install/registration and actual character binding are separate, guarded tasks. Do not pass display-only or untested APKs as playable.

## Physical Galaxy S26 Ultra and original game preservation

- Android original package exists: `com.azrazieliz.OfflineGame`. Experimental `com.azrazieliz.gate12fixture` is **NOT installed** at checkpoint.
- Original installed APK was pulled from device and preserved *locally* at:
  `Saved/Gate12Koikatsu/FoundationV1InPlaceUpdate_Safeguards/CURRENT_INSTALLED_FOUNDATION_V1__BACKUP.apk`.
- Exact installed app APK SHA256: `87e703e2f9aaaf2a0d0778271320a84a06facd4c3a73177060bcf0e1dcb46059`, 169,804,278 bytes.
- Original installed, frozen-build and experimental-build APK certificates share the same SHA256:
  `9bc439712a0a460cc4703ebf254aaaf9bf3cc6e67b0ced6fc12162ed3ca3b9ad`. Verified by Android `apksigner`, exit code 0.
- The current phone `WorldState.db` and RecoveryCatalog were backed up and SHA256-verified against on-device originals, as were two manual backups, one pre-import recovery DB, and the 2026-10-10 snapshot. **Never upload these private user databases or original APK to GitHub.**
- On-device `WorldState.db` SHA256: `220a61bf8d23b098706c0eea74b95897e5bb64723b1943977644de21356318fa`. PC frozen world's database is separate and remained unchanged at `4DBB961D45823763EBCE7E16DC62B1102B32B8FFD46003AB61AB05F0EF83554A`.
- `Scripts/Gate12Koikatsu/safe_update_foundation_v1.ps1` checks package ID, exact signing certificate, branch and frozen Foundation, backs up currently installed save before mutating and installs only on explicit `-Apply`. Dry-run against real Foundation APK **PASS**, dry-run against unrelated experimental package **REJECTED**. No APK update installed.

## No full APK rebuild needed for ordinary content revisions — architecture verified, integration pending

Frozen Foundation source already implements the canonical, conservative optional Pak host:
- `Runtime/OGOptionalPackageHost.cpp`: legacy Pak runtime mounting, exact dependencies, leases, rollback and safe unmount.
- `Runtime/OGLocallyInstalledPackageProvider.cpp`: managed local storage, SHA256 byte verification, native receipt, trusted optional-Pak installation.
- `Runtime/OGGameCoreSubsystem.cpp`: `InstallVerifiedOptionalPackage` path; no user-facing content installer or debug command was found invoking it.
- `Config/DefaultGame.ini`: `bUseIoStore=False`, `bUseZenStore=False`, `bUsePakFile=True`.
- Foundation loader requires **trusted package installer metadata and native-generated receipts**; importing JSON or externally dropping a Pak is NOT authority to mount/activate it. Experimental fixture remains engineering-only and not `ARTIFACT_READY`.

**Physical packaging proof:** `Scripts/Gate12Koikatsu/package_cooked_character_pak.ps1` made a versioned experimental `Android_ASTC` Pak using **previously cooked** UE5.8.3 character bytes and the exact legacy Pak format:
- 234 cooked files, 36,633,270 bytes input.
- Actual generated UnrealPak compressed file: 9,823,496 bytes; SHA256 `f0e998038a124878e0611f34610b517c3697fe791f6168bdeedbd76fb6357b22`.
- UnrealPak build and `-List` passed, listing **all 234 files** (not just one filename).
- The Pak+manifest were pushed to the Galaxy S26 via ADB; SHA256 verified on the **device**, exact match. This passed **without rebuilding/reinstalling an APK**.
- The experimental fixture APK was not installed. Orphan test files were removed after verification. **Runtime mount, asset registry, visual spawn, dependency closure in the game = NOT TESTED.**
- The Pak is an *engineering fixture* generated from previously cooked assets, not a completed distribution format for multiple registered production characters. Provenance remains unauthorised until Gate 11 accepts it. It contains the known older QA assets and is not a facial/gameplay validation.

Future steady state: one base APK with stable runtime and a user-facing trusted content installer; add/update a small versioned character/world Pak rather than rebuilding the entire APK. A new Android cook still occurs on any changed original `.uasset`/interchange files; Unreal C++/native functionality changes may require an APK update. Storage is versioned by character ID with hashes, closure/dependencies, managed install, and opt-in loading. On-device free media space at check: **19 GB**; do not preload all future packs.

## Why playable APK was not produced today

Actual dedicated UE5.8.3 `AOGWorldPrototypeCharacter`-derived playable character Blueprint, custom GameMode, and cloned starting-world map were authored/compiled/saved in prior isolated commits, with 19 original-detail mesh components and exact skeleton idle animations. They are real Unreal assets but **not yet a demonstrated controllable phone character**. The proposed action-state C++ adapter is **staged uncompiled** outside runtime Source. Correct locomotion clips and facial material device validation remain outstanding.

After workstation restart, an actual **incremental** Android playable-world cook was attempted with memory-limited parameters `-iterate`, `-CookMapsOnly`, `-MaxParallelShaderJobs=1`. Engine process started but was safely terminated after ~93 seconds when free RAM fell below 850 MB: guard status `STOPPED_LOW_RAM`. Prior retry was blocked before startup at 1433 MB free. Host has 8 GB total RAM; mobile device CPU offload for morph evaluation passed earlier but cannot substitute for Windows Unreal cooking. **No new playable APK exists, so no in-place update has been applied.** Do not relax RAM guard and freeze desktop, change pagefile without consent, or degrade source art.

Recommended build host: 16 GB RAM minimum to attempt this scale; **32 GB preferable** for regular UE5 mobile cooks, shader compilation and multiple environments. Those are practical capacity targets, not asserted measurements for this exact project.

## Content production does not need to wait

`Scripts/Gate12Koikatsu/stage_new_koikatsu_character.ps1` provides one-command **preproduction source intake** for real Koikatsu GLB exports. It validates stable IDs, preserves the original byte-for-byte source and its SHA256, runs the tested lossless per-primitive converter, stages all geometry, UV/material/texture/morph references, and generates an explicit engineering-only intake receipt.

Actual fixture test (not a claim about arbitrary Koikatsu exporters):
- Same immutable original source GLB `ad06955e6f725c63c6ee014828877a9a8fa3a19479ca29936c58c96ed6d012e0`.
- `KKFixturePreProduction0002` created under isolated Saved QA, 6 meshes, 25 primitives, 4,611 morph references, one skin. Source file archived unchanged. Executed with exit 0 in **3.83 seconds**, no Unreal editor, no Android cook.
- The underlying generic converter previously proved SHA256 parity for all 25 generated GLBs relative to validated imports.
- Different multi-skin exports require their own validation; none is declared compatible without testing. No downloaded character content was added.

Production workflow now: create original characters in Koikatsu; export/save the genuine character card plus intended game-quality GLB, textures and optional animations; assign stable ID; run source intake. This grows the content corpus immediately, even if UE native import, phone animation and Gate 11 rights/provenance reviews are pending.

Example (after exporting a new original to an actual GLB):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Scripts\Gate12Koikatsu\stage_new_koikatsu_character.ps1" -SourceGlb "D:\path\to\export.glb" -CharacterId "KK_Original001"
```

**No private phone backups, experimental APKs or unpublished imported source GLBs should be added to a public repository.**

## Concrete next Gate 12 acceptance

1. Prepare Android cooked *playable* map and associated rig/material content on a sufficiently capable UE 5.8.3 build host; verify correct face alpha/toon shader and native morph material flag persists after cold reload.
2. Build a one-time signed base update containing user-facing trusted content installer / character registry-selector and real input-bound character pawn. Test **in-place**, preserve SHA-verified phone saves.
3. Mount one test bundle using the **native trusted installer with a test-only authorized receipt**, not manual DB manipulation, and validate device character spawn, touch movement, idle/walk/run/jump, face expressions, collisions and UE renderer FPS.
4. Only production-register with Gate 11 after actual source asset provenance/art review and evidence. Do not merge experimental fixtures into Foundation or promote placeholder/test status.

**Gate statuses:** source intake PASS; cooked Pak build/list PASS; device transport/hash PASS; same-ID/cert update feasibility PASS; phone playable runtime NOT_TESTED; native pack trusted install/mount NOT_TESTED; mobile facial shaders after fix NOT_TESTED; no new APK issued. These are separate evidence-gated results.
