# Gate 12 P0 — Frozen Unreal content-contract compatibility

Baseline: 3f54f07cacbf193e160d0be15a7be5a7bc730c3b (foundation-v1-freeze-2026-10-08).
Work branch: production/gate12-p0-content-contract-20261008.
Scope: SOURCE-ONLY P0. No frozen Foundation code/config modified; no user world DB opened.

## Verified engine interfaces

| Concern | Frozen code | Compatibility conclusion |
| --- | --- | --- |
| Content ID | Source/OfflineGame/Public/Content/OGContentId.h | Lowercase namespace:key; restricted ASCII punctuation, not persistent runtime entity UUID |
| Package manifest | Public/Content/OGContentManifest.h and Private/Content/OGContentManifest.cpp | FOGContentPackageManifest holds PackageId, Version, Dependencies, CharacterIdentities and CharacterVersions only; validates IDs and character maturity; not a JSON importer |
| Package lifecycle | Public/Runtime/OGPackageReportManagementRecords.h and Private/Runtime/OGPackageManagerService.cpp | FOGContentPackageRecord and FOGPackageDependencyRecord live in canonical IOGWorldStore; registration deactivates; activation requires installed, validated package and active satisfactory dependency versions |
| Physical optional pak | Public/Runtime/OGLocallyInstalledPackageProvider.h and Public/Runtime/OGOptionalPackageHost.h | Independent trusted native installation authority plus actual byte verification is mandatory; imported DB fields or JSON cannot establish mount trust |
| Optional presentation | Public/Runtime/OGOptionalAssetRuntime.h and Private/Runtime/OGOptionalAssetRuntime.cpp | Optional asset needs valid FSoftObjectPath and dependency/residency; missing media can fall back without changing canonical world state |
| Ruler UI contract | Private/UI/OGUiViewModelService.cpp and Public/UI/OGUiViewModelService.h | BuildRulerShell projects Home, Characters, Gacha, Territory, Records from canonical state; does not itself create UMG widgets |

**Gate 11-to-native mismatch:** production manifest provenance, import path, pipeline status, hashes, measured bytes and asset list are not FOGContentPackageManifest fields. They must remain Gate 11 evidence or be explicitly translated by an importer; do not cast untrusted artifact JSON to native package records or pretend ManifestJson is an installation receipt. The historical CONTENT_ARTIFACT_MANIFEST_TEMPLATE.json v2 was referenced in the handoff but not supplied here. Exact schema-v2 conformance remains unverified.

## Actual authored sample

Content/Data/Production/Gate12RulerShellContractSmoke.json provides a minimal, non-diagnostic source-data example using **the frozen ruler-shell navigation contract**, not invented story, characters, visuals or balance.

- Embedded/source-only package DTOs: ui:shared_navigation_contract v1 and ui:ruler_shell_navigation v1, where the second depends on the first at minimum version 1.
- Authored content ID: ui:ruler_shell.primary_destinations; the five destinations match frozen BuildRulerShell.
- Both CharacterIdentities and CharacterVersions are intentionally empty. No new manifestation or persistent world state is authored.
- Optional media is null. Safe omission preserves the existing canonical ViewModel, rather than pointing to a non-existent asset.
- Sample state is designing; there is no actual Unreal import, package installation, native activation, physical mount or signed-off Gate 10 visual.

Scripts/validate_gate12_content_contract.py validates the static native DTO field shape, ID format, dependency references/versions/cycles, expected destination set, optional media absence, and rejected installer-trust escalation fields. It has seven built-in negative mutations and can cross-check the C++ function.

### Run from a real checked-out post-freeze worktree

    py -3 Scripts/validate_gate12_content_contract.py --self-test --check-runtime-source Source/OfflineGame/Private/UI/OGUiViewModelService.cpp --emit-adapter Saved/Gate12/RulerShellNativeDtoMap.json

The optional emitted adapter is a planned DTO mapping. It must NEVER be used as an installation receipt or directly written into the canonical save.

### Next native smoke when development machine is online

In Unreal 5.8.3, run an isolated temporary-database automation test: instantiate native package manifests, validate with FOGContentManifestValidator, register two embedded FOGContentPackageRecords, add FOGPackageDependencyRecord via SetDependency, activate dependency before child, exercise absent optional visual fallback, read back unchanged package/identity state, and remove only test temp DB. This is a **future executable instruction**, NOT evidence of a test that has run.

### Evidence and limits

- GitHub ancestry comparison verified old phase-g2-runtime-harness tip 7668ea1... is precisely one ancestor commit behind 3f54f07..., making direct post-freeze branch creation safe without reset.
- Fetched actual freeze interface/header/implementation files before authoring.
- Read-only JavaScript shape check parsed the committed source JSON and rejected five altered specimens. This did not execute the committed Python script or engine.
- The registered Windows development laptop was offline, so there was no engine compile, Unreal automation, cook, on-device Android launch, measured bytes, performance or user-save test.
- Source-only documentation/data/tool changes do not warrant repeating the 294-action/139-test Foundation freeze suite. Source/import changes require proportional tests later.

## Gate handoff and authority

Gate 11 must accept the exact namespace, dependency and packaging schema; Gate 10 must provide real approved UMG source, Gate 02 region source, Gates 04/07/08 mechanics source for next stages. Do not fabricate engine assets or mark missing media integrated. Gate 12 may subsequently author deterministic Editor import, content glue and targeted tests, while preserving trusted native package provenance.

The provisional machine-readable evidence record is docs/production/gate12/GATE11_P0_ARTIFACT_HANDOFF.json. Its schema is a clearly labeled proposal, not the absent historical v2 template.

Gate 00 approvals needed for P0 source review: **none**. Promotion to imported/integrated requires approved input assets/manifests and online native validation. Keep this branch separate from frozen Foundation pending review.


## Additional frozen World / Character presentation integration check

- Source/OfflineGame/Private/Runtime/OGCanonicalCharacterPresentation.cpp derives visual state from canonical entity injury/restoration, current outfit, presentation variants, equipped-item condition and exact Manifestation projection. Its optional asset loader retains current valid rig/materials if the authored media cannot load. A different body requires compatible skeleton/rig family/animation; rejected parts must not overwrite the retained body. Privacy presentation is a different binding, not a new character or world state.
- Source/OfflineGame/Private/Runtime/OGCharacterVisualResolverProvider.cpp consumes validated package ManifestJson only for specifically authored character_visual_bindings rows selected by exact character Identity, Version, active form and entity scope; ambiguous matches reject and preserve the current body. This is **a specialized visual metadata consumer**, not a generic Gate 11 JSON import, not physical pak authority, and not justification for invented Body/Asset paths.
- Source/OfflineGame/Private/World/OGStartingRegionPresentation.cpp is still a Foundation procedural presentation/scaffolding layer with generated terrain/instances, reproducible seed and persistent canonical locations. Its debug fixtures require -FoundationFixtures in non-shipping builds. A high-quality Gate 02 region must come as real accepted source/interchange before this gate can claim final .umap/PCG/mesh integration.
- These character/world discoveries do not change the source-only RulerShell navigation sample. They constrain later importer validation: reject mismatched rig/animation/selector assets, honor active form/outfit/injury and never convert temporary map presentation into a competing world save.
