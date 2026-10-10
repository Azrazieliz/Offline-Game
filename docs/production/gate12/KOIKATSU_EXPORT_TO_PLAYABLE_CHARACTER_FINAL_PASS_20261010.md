# Gate 12 — Koikatsu export to Unreal 5.8.3 playable character, final-pass checkpoint

**Date:** 2026-10-10. **Scope:** isolated experimental engineering fixture G01-KK-PIPELINE-TEST-0002. Not Gate 11 ARTIFACT_READY, not fully DEVICE_VALIDATED, and not production content. Frozen Foundation and PR #16 are untouched.

## Independent test matrix

| Test | Result | Grounded evidence |
|---|---|---|
| Actual GLB source and original image fidelity | PASS | 29,572,372 bytes, SHA256 ad06955e6f725c63c6ee014828877a9a8fa3a19479ca29936c58c96ed6d012e0 |
| Reusable export converter on fixture source | PASS | 6 meshes, 25 primitives, 4,611 morph references, 20 original embedded textures |
| Comparison with previous independently verified source sharder | PASS | All 25 new GLB shard SHA256 values identical |
| Generic source import PLAN without UE mutation | PASS | 25 hash-verified jobs for a character-ID-isolated content path |
| Generic new-character UE import execution | NOT TESTED | No new real character provided or native import run |
| Existing exact-native skeleton bone motion on physical S26 Ultra | PASS for basic 2s loop | Previous 12s video and source asset branch commit 4325720 |
| Larger native skeleton motion on physical S26 Ultra | PASS for sampled limited motion | 15s video, 25.111% silhouette difference between opposite poses; only 0.644% after one 2s loop |
| Real UE AOGWorldPrototypeCharacter-derived Blueprint character assembly | PASS native authoring | 19 actual full-resolution parts, 19 exact idle animation bindings, Blueprint compiled/saved |
| Dedicated GameMode and copied starting world | PASS native authoring | Saved the world's GameMode override and Blueprint default pawn class |
| Hiding frozen diagnostic character behind real imported meshes | PASS native authoring | Precisely one inherited CharacterMesh0 component hidden, 19 imported visual parts preserved |
| Full world on phone / touch-directed avatar movement | NOT TESTED | Playable-world Android cook aborted safely due low workstation RAM before completion |
| Native morph usage flag check | PASS in same editor session, PERSISTENCE NOT PROVEN | 15 materials inspected, all flagged after, one toggled in memory but saved file has identical Git blob SHA |
| Correct facial expression appearance on phone AFTER material fix | NOT TESTED | Corrected build could not finish Android cook |
| Facial expression appearance on PRIOR device diagnostic | FAIL | Real S26 close-up shows grey face; missing MorphTargets shader-usage warnings in UE runtime log |
| Walk/run/jump/dodge state-controlled animation clips | NOT IMPLEMENTED | Existing Blueprint still uses fixed idle sway; state adapter is SOURCE ONLY and not compiled |
| Stable phone FPS and comprehensive clothed collision | NOT TESTED | Limited prior 60s non-crash / memory / thermal checks are not FPS or clipping acceptance |

## How future Koikatsu exports should be handled

**Step 1 — source:** retain immutable original GLB, use build_kk_export_generic.py with --source-glb, --character-id, --output-root. Run --inspect-only first. Strict one-skin glTF 2.0 source contract; other rigs are deliberately rejected rather than silently changed. Full-byte source hash, all used textures, material IDs and morphs are independently checked. No downsampling of source texture details for laptop RAM.

**Step 2 — staging:** run import_kk_character_guarded.ps1 -Manifest PATH without -Execute to verify all shard hash/provenance, generate the staged UE paths and evidence-only plan. Its -Execute path requires a specifically isolated fixture branch, verifies Frozen Foundation and has per-shard start-RAM / live-RAM / timeout stops, and refuses package overwrite. Tested plan-only for 25/25; new-character actual native importer remains untested.

**Step 3 — real UE asset validation:** UE 5.8.3 import must be audited for mesh bounds, bone hierarchy, reference pose, correct skeleton for each segment, all material/texture slots and actual morph deltas before accepting a native descriptor. Never use another Koikatsu character's original local bone transforms just because skeleton names match.

**Step 4 — character descriptor and blueprint:** build_playable_descriptor.py creates a safe per-character KK_UNREAL_CHARACTER_DESCRIPTOR_1 referencing only exact native assets. The generic author_kk_playable_from_descriptor.py creates a new Blueprint subclass of the existing native mobile-input/movement character and game mode; source-only portability is not yet tested on a second export. The fixture-specific author_playable_character_blueprints.py and finalize_playable_kk_visuals.py ARE natively validated and produced real serialized blueprint assets.

**Step 5 — expression material safety:** use apply_kk_morph_material_flags_from_descriptor.py on the accepted native descriptor. Base UE materials on morph-capable parts must have MATUSAGE_MORPH_TARGETS explicitly enabled, saved and recooked. Native in-session flag check PASS; cold-reload persistence NOT VERIFIED; mobile post-correction FAIL/PASS unknown.

**Step 6 — actual action animations and gameplay:** playable test Blueprint currently has real ACharacter movement inheritance, but the visible mesh components still loop idle sway. NativeGameplayCandidate/OGKoikatsuPlayableCharacter.h and .cpp contain an explicitly UNCOMPILED source-only adapter that can switch exact-skeleton idle/walk/run/airborne/dodge animation clips according to actual frozen game movement, while exposing a dynamic SetExpressionMorph control. This is NOT installed in the active Source tree. Walking/running/jumping clips must be authored and approved for every exact source skeleton; no fake placeholder locomotion acceptance. On physical Android the actual action-state transitions must be observed before game-ready designation.

## Actual Unreal gameplay work done

Existing frozen source has AOGWorldPrototypeCharacter and AOGWorldPresentationGameMode: player movement, sprint, dodge, climb, flight/swim, touch controls and combat action hooks already exist. In the isolated worktree this pass actually created and saved:

- /Game/Experimental/Gate12Koikatsu/PlayableQA/BP_KoikatsuFixturePlayable
- /Game/Experimental/Gate12Koikatsu/PlayableQA/BP_KoikatsuFixtureWorldMode
- /Game/Experimental/Gate12Koikatsu/PlayableQA/Maps/L_KoikatsuPlayable_UE583

The player's inherited CharacterMesh0 tutorial proxy is hidden, and 19 full imported UE skeletal mesh segments are real component templates on the Blueprint. This is neither a video nor a sprite. However Android cook of this real-player world was stopped by free RAM guard when workstation available RAM dropped to 422 MB on the 8GB laptop; no playable-game Android result can be asserted.

## Visual-quality opportunities and remaining problems

A real visual audit script (audit_kk_visual_quality.py) validated the immutable GLB and catalogued 25 PBR materials and 20 original textures including a 4096x4096 face image. Only 1 source material is a candidate for cutout alpha conversion; no visual material conversion was automatically forced. Texture-resolution preservation alone does NOT reproduce Koikatsu's custom face/eye/skin/toon shader. The visible grey face in the earlier failed close-up must be fixed in real UE materials and independently validated on phone. Future guarded, art-reviewed improvements can include toon/skin shading matching reference, correct sRGB/linear maps, alpha masking only for near-binary cutout, two-sided hair where genuinely needed, neutral scene illumination/exposure and mobile shadow settings, while preserving original source details.

The native material initially reported as changed has identical on-disk Git hash to HEAD after editor save (6e8803d2198057db38057fe9420bac7dce11957d), so this in-memory flag operation cannot be called a persisted material fix without cold-reload verification. The face rendering cannot be called a device-quality PASS until Android recook succeeds. Android ASTC cooker for corrected facial scene stopped by memory guard at 364/649 packages; playable starting-world cooker stopped even earlier during startup. Do not disable the memory guard, downscale the sources or silently change Foundation.

## Preservation and status

Original frozen Foundation SHA: 3f54f07cacbf193e160d0be15a7be5a7bc730c3b. Original save WorldState.db SHA256: 4DBB961D45823763EBCE7E16DC62B1102B32B8FFD46003AB61AB05F0EF83554A. Original phone app package com.azrazieliz.OfflineGame retained, and last experimentally validated basic sway APK retained separately. PR #16 remains draft and unmerged. Source-only adapter is deliberately outside active runtime Source/OfflineGame.

**Conclusion:** source preservation/generalized sharding, native rig/idle rendering and native Blueprint assembly are materially improved and independently supported. The exact claim that every future Koikatsu export is one-click game-ready would currently be false. Finish facial shader device validation and live controlled player + state animation Android tests on a UE 5.8.3 host with sufficient free physical memory, then reopen the gate.
