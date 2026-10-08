# Foundation v1 — repository cleanup and GitHub baseline audit

Date: 2026-10-08. Branch: `content/environments-worlds`.
Pre-cleanup HEAD: `7668ea19268c5c3ea1cc7d023d984b0f188851e5`.

## Pre-mutation inventory

`git status --short --untracked-files=all` reported **53 tracked modifications** and **108 untracked files**; no tracked deletions. The branch matched `origin/content/environments-worlds` at the pre-cleanup HEAD. No Foundation source or config was reset, reverted or discarded.

## Classification: all 53 tracked modifications — keep

- `Config/DefaultEngine.ini` (1): authoritative engine/runtime/Android project settings.
- `OfflineGame.uproject` (1): project/plugin configuration.
- `Scripts/verify_unreal_cpp.py` (1): validation tooling.
- `Source/OfflineGame/**` (48): Foundation build rules, implementation, interfaces and C++ regression/automation tests.
- `docs/CONTRADICTION_SUPERSESSION_AUDIT.md`, `docs/G2_RUNTIME_HARNESS.md` (2): authored design and verification documentation.

## Classification: all 108 untracked files

**Keep and commit (99):**

- `Build/Android/src/com/epicgames/unreal/VolumeReceiver.java` (1): authored/required Android media-volume bridge. Referenced by `Source/OfflineGame/OfflineGame_APL.xml`; must not be ignored or deleted.
- `Config/Android/AndroidEngine.ini`, `Config/Android/AndroidGame.ini`, `Config/DefaultGame.ini`, `Config/DefaultInput.ini`, `Config/Foundation/CharacterVisualDiagnostic.json` (5): Android startup/package fixes, input, diagnostic binding and cooking settings.
- `Content/Data/FoundationClockPolicy.json`, `Content/Foundation/Materials/M_FoundationWaterSurface.uasset`, `Content/Maps/StartingWorld.umap` (3): authored data and native Unreal assets.
- `FOUNDATION_FREEZE.lock` (1): freeze marker.
- `Scripts/Editor/create_starting_world.py`, `Scripts/Editor/patch_starting_world.py` (2): authored editor map construction/patch tooling.
- `Source/OfflineGame/OfflineGame_APL.xml` (1): authored Android UPL and backup/volume/resizability integration.
- All new `Source/OfflineGame/Private/**` and `Source/OfflineGame/Public/**` C++ headers/implementations/tests (83): authoritative Foundation and diagnostic integration source.
- `docs/FOUNDATION_CLEANUP_HANDOFF_2026-10-08.md`, `docs/FOUNDATION_FREEZE_V1.md`, `docs/MATURE_CONTENT_V041_PRODUCTION_FREEZE.md` (3): authored freeze/handoff/production documentation.

**Generated; exclude from version control (9):**

- `Build/Android/FileOpenOrder/CookerOpenOrder.log` and `EditorOpenOrder.log` (2): Unreal file-order trace logs.
- `Build/Android_ASTC/FileOpenOrder/CookerOpenOrder.log` and `EditorOpenOrder.log` (2): alternate-target file-order trace logs.
- `Build/Android/src/com/azrazieliz/OfflineGame/AlarmReceiver.java`, `DownloaderActivity.java`, `OBBData.java`, `OBBDownloaderService.java` (4): Unreal Android expansion/download template or generated package-specific helper code; `OBBData` embeds a build-dependent expansion size.
- `Build/Android/src/com/epicgames/unreal/DownloadShim.java` (1): Unreal package-specific downloader shim.

The `.gitignore` changes target only these generated paths, not the surrounding `Build/Android` source directory. Generated copies remain on disk. Existing ignored Unreal outputs in `Saved/`, `Intermediate/`, `Binaries/`, `DerivedDataCache/`, APK/OBB/AAB archives and local databases remain untracked and untouched; this cleanup is about publishing a reproducible Git source baseline, not deleting device/build evidence.

## Baseline policy

No gameplay behavior, Unreal asset, Android bridge logic, or runtime configuration was changed in this cleanup. Only explicit ignore rules, this audit, and whitespace-only fixes in six newly added source/document files were applied on top of the validated Foundation worktree. Lightweight gates suffice; the previous 294-action build and 139-test device freeze evidence is documented by `FOUNDATION_FREEZE.lock` and `docs/FOUNDATION_FREEZE_V1.md`.

A Foundation change after the baseline requires an evidence-backed defect or unavoidable compatibility/content integration issue and proportionate regression validation. The next stage is game content, UX/presentation and world/character production, not renewed Foundation engineering.
