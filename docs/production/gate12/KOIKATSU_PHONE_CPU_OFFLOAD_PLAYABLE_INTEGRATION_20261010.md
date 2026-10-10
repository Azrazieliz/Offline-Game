# Gate 12 — Physical S26 Ultra CPU offload, generic converter and playable QA asset

Date: 2026-10-10. All work is isolated on production/gate12-koikatsu-ue583-fixture-20261010. Not Gate 11 ARTIFACT_READY, not overall DEVICE_VALIDATED, and not approved to merge into the frozen Foundation.

## Physical phone CPU execution: PASS

The Galaxy S26 Ultra (Android 16, arm64-v8a, ADB R3GYC0LSC7K) executed a compiled native Android NDK binary on the physical phone, not remotely on the laptop or as a video. This preserves the device-only target preference and avoids Unreal Editor RAM pressure for compatible geometry calculations.

Source:
- Scripts/Gate12Koikatsu/PhoneCompute/prepare_morph_workload.py
- Scripts/Gate12Koikatsu/PhoneCompute/kk_morph_validate_arm64.cpp
- Scripts/Gate12Koikatsu/PhoneCompute/run_on_galaxy_s26.ps1

The wrapper accepts -SourceGlb and -CharacterId. It checks the isolated branch and frozen Foundation, indexes original source POSITION and sparse/dense morph accessors on the PC without heavy blending, compiles a small ARM64 executable using Android NDK, uses ADB to transfer it with the GLB, checks the GLB SHA256 on phone, executes all morph calculations on phone, captures JSON evidence, and deletes only its own temporary phone directory.

Actual existing fixture:
- Original GLB SHA256: ad06955e6f725c63c6ee014828877a9a8fa3a19479ca29936c58c96ed6d012e0
- Device original-GLB SHA256: exact match.
- 25 source primitives, 4,611 real morph POSITION accessors, 61 evaluated animation frames.
- 12,740,399 sparse/dense weighted morph vertex updates; 371,646 changed vertex samples.
- Max weighted vertex displacement in original GLB coordinate units: 0.0353559.
- Invalid/nonfinite vertices: zero.
- Final independently repeated phone computation: 22 ms. End-to-end preparation, compilation, upload, verification, computation and cleanup: 3.25 seconds. These timing metrics refer only to this fixture and computation.
- Wrapper exit: 0; status PASS_PHONE_NATIVE_CPU.
- Phone temporary work directory removed and checked.

Usage (from isolated project root):
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Scripts/Gate12Koikatsu/PhoneCompute/run_on_galaxy_s26.ps1 -SourceGlb D:/path/to/new_character.glb -CharacterId KK_Character001

Phone evidence: docs/production/gate12/evidence/koikatsu-phone-cpu-20261010/phone_compute_report.json and phone_compute_stdout.txt. Raw GLB is NOT committed.

## Reusable source conversion: PASS on original fixture

Scripts/Gate12Koikatsu/build_kk_export_generic.py takes --source-glb, --character-id, --output-root, optional --expected-sha256 and --inspect-only. It replaces original fixture-only source-path and hardcoded SHA requirements. It preserves original mesh/primitive topology, morph references, full embedded texture bytes, image resolution, and material references. For the existing source fixture it recreated 25/25 shards bitwise equal to the validated earlier exports (SHA256 comparisons); 6 meshes, 25 primitive slots, 4,611 source morph references, 20 embedded images. Supports exactly one glTF skin as previously tested; multiple skin export needs deliberate support, not guessed conversion.

Scripts/Gate12Koikatsu/import_kk_character_guarded.ps1 validates every shard hash and derives safe character-id-specific Unreal import staging paths. Its dry-run passed all 25 entries; no new-character real UE import from this generic wrapper has occurred. The actual native UE import process still requires the Windows UE5.8.3 editor.

## Real UE playable Blueprint assembly: PASS for asset authoring only

Unreal Engine 5.8.3 created, compiled and saved:
- /Game/Experimental/Gate12Koikatsu/PlayableQA/BP_KoikatsuFixturePlayable
- /Game/Experimental/Gate12Koikatsu/PlayableQA/BP_KoikatsuFixtureWorldMode
- /Game/Experimental/Gate12Koikatsu/PlayableQA/Maps/L_KoikatsuPlayable_UE583

The Blueprint derives from the actual existing AOGWorldPrototypeCharacter, reusing the game movement, mobile controls, sprint/jump/traversal and action hook infrastructure. It holds 19 full-quality skeletal mesh components, each assigned its exact native skeleton idle clip; the inherited engine tutorial body is hidden in this experimental Blueprint. The test GameMode sets this Blueprint as default player pawn in a clone of the starting world. Unreal asset authoring PASS does not prove gameplay control on Android.

The actual playable Android cook was attempted but stopped by the existing RAM safety guard after free RAM fell to 422 MB during the engine startup/asset-registry phase. No runnable playable-world APK was created. It is NOT an Android gameplay PASS.

An additional gameplay-state-aware proposed C++ adapter is staged ONLY outside the active Source tree under Scripts/Gate12Koikatsu/NativeGameplayCandidate. It includes idle/walk/run/airborne/dodge selection based on the existing CharacterMovement and action state and direct morph-set controls. It has NOT been compiled, activated or tested, and actual appropriate movement clips are not authored yet. It is a future engineering candidate, not demonstrated game operation.

## Facial material issue and visual quality

Unreal native material-usage fix script Scripts/Gate12Koikatsu/fix_morph_target_material_usage.py ran under the guarded editor and returned PASS: 15 materials associated with native morph meshes inspected, 1 lacking morph shader usage fixed, all 15 verified true. Corrected material visual appearance was NOT revalidated on Android: Android ASTC recook was stopped under laptop RAM safety guard.

Full-resolution original face/skin/hair/clothing textures and original materials remain the proper quality baseline. Potential additional quality improvements should be compared on target phone, including correct alpha/transparency, facial and eye material blend/texture color space, hair transparency sorting, normal directions and tangents, lighting and mobile shadow settings. There is NO justification for dropping polygons, expressions or textures solely to suit laptop RAM. No unmeasured visual improvements are claimed.

The phone can execute CPU geometry/morph analysis, preprocessing and future native tooling; it cannot replace Windows Unreal Engine asset importing, 5.8.3 CookCommandlet, shaders or packaging. A real Android APK and physical runtime proof still require a successful compatible Unreal host build.

## Final evidence-gated status

- Real physical Android ARM64 CPU compute: PASS.
- Generic source GLB sharding with exact fixture comparison: PASS.
- Guarded import planner: PASS dry-run, native import NOT_TESTED for new exports.
- Real Unreal playable Blueprint asset authoring: PASS.
- Native material metadata fix: PASS; facial device visuals NOT_TESTED.
- Playable Android cook/install/control: BLOCKED by laptop RAM guard.
- Runtime locomotion animation switching: NOT_TESTED.
- Source licensing, Gate11 acceptance, full DEVICE_VALIDATED: NOT AUTHORIZED.

Preservation: frozen Foundation commit 3f54f07cacbf193e160d0be15a7be5a7bc730c3b and saves were not modified; P0 PR #16 remains draft and unmerged. The known-good native-rest-safe QA level is the default experimental map; original Android game remains installed and separate from the test fixture.
