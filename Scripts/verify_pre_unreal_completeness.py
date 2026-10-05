#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Source" / "OfflineGame"
ERRORS: list[str] = []


def fail(message: str) -> None:
    ERRORS.append(message)


def read(rel: str) -> str:
    path = ROOT / rel
    if not path.exists():
        fail(f"missing required file: {rel}")
        return ""
    return path.read_text(encoding="utf-8")


# 1) No unresolved implementation placeholders or superseded production fields.
for path in sorted(SOURCE.rglob("*")):
    if path.suffix not in {".h", ".cpp"}:
        continue
    text = path.read_text(encoding="utf-8")
    rel = path.relative_to(ROOT).as_posix()
    for token in ("TODO", "FIXME", "NotImplemented"):
        if re.search(rf"\b{re.escape(token)}\b", text, flags=re.I):
            fail(f"{rel}: unresolved implementation placeholder {token}")
    for token in ("DuplicateAcquisitionCount", "bSexualContentEligible"):
        if token in text:
            fail(f"{rel}: superseded production field reintroduced: {token}")

# 2) Schema/document authority must describe the implemented baseline.
matrix = read("docs/RECONCILIATION_MIGRATION_MATRIX.md")
data_model = read("docs/DATA_MODEL.md")
implementation_order = read("docs/IMPLEMENTATION_ORDER.md")
vertical_slice = read("docs/VERTICAL_SLICE.md")

if "Current persistent schema: **version 14**." not in matrix:
    fail("RECONCILIATION_MIGRATION_MATRIX.md does not identify schema 14 as current")
if "authoritative persistent schema is **version 14**" not in data_model:
    fail("DATA_MODEL.md does not identify schema 14 as authoritative")
if "canonical-adult eligibility metadata" in implementation_order:
    fail("IMPLEMENTATION_ORDER.md still contains obsolete adult eligibility-gate wording")
if "harness authored against the pre-detailing architecture" in implementation_order:
    fail("IMPLEMENTATION_ORDER.md still labels Vertical Slice 0 as pre-detailing")
if "STATUS: IMPLEMENTED RECONCILED CONTRACT." not in vertical_slice:
    fail("VERTICAL_SLICE.md does not identify the reconciled implementation as current")

# 3) Frozen UI/view-model contract coverage.
models = read("Source/OfflineGame/Public/UI/OGUiViewModels.h")
service_h = read("Source/OfflineGame/Public/UI/OGUiViewModelService.h")
service_cpp = (
    read("Source/OfflineGame/Private/UI/OGUiViewModelService.cpp")
    + "\n"
    + read("Source/OfflineGame/Private/UI/OGUiViewModelService.Advanced.cpp")
)

required_model_markers = {
    "roster search/filter/sort/density": "FOGRosterQuery",
    "side-by-side Manifestation comparison": "FOGManifestationComparisonViewModel",
    "skill/progression subset": "FOGManifestationDetailViewModel",
    "wardrobe/skin compatibility": "FOGWardrobeViewModel",
    "adult utility": "FOGAdultUtilityViewModel",
    "equipment comparison": "FOGEquipmentComparisonViewModel",
    "Known/Experiment crafting": "FOGCraftingViewModel",
    "gacha details": "FOGGachaDetailsViewModel",
    "gacha history filtering": "FOGGachaHistoryFilter",
    "Chronicle filtering": "FOGChronicleFilter",
    "Intelligence filtering": "FOGIntelligenceFilter",
    "Codex searchable categories": "FOGCodexViewModel",
    "principal target/status UI": "FOGWorldTargetViewModel",
    "turn timeline": "FOGTurnTimelineEntryViewModel",
    "three-field battle recap": "FOGTurnBattleRecapEntryViewModel",
    "backup manager": "FOGBackupManagerViewModel",
    "package/storage manager": "FOGPackageStorageViewModel",
    "knowledge-gated risk": "FOGQualitativeRiskViewModel",
    "Project UI": "FOGProjectViewModel",
    "Dispatch UI": "FOGDispatchViewModel",
    "War UI": "FOGWarViewModel",
}
for label, marker in required_model_markers.items():
    if marker not in models:
        fail(f"UI contract missing {label}: {marker}")

required_service_methods = [
    "BuildRosterWithQuery",
    "BuildManifestationDetail",
    "BuildManifestationComparison",
    "BuildWardrobe",
    "BuildAdultUtility",
    "BuildCraftingShell",
    "BuildEquipmentComparison",
    "BuildGachaDetails",
    "BuildGachaHistoryFiltered",
    "SetActiveTerritoryOverlay",
    "BuildChronicleFiltered",
    "BuildIntelligenceFiltered",
    "BuildCodex",
    "BuildWorldTarget",
    "BuildTurnBattlePresentation",
    "BuildBackupManager",
    "BuildPackageStorage",
    "ProjectRisk",
    "BuildProject",
    "BuildDispatch",
    "BuildWar",
]
for method in required_service_methods:
    if method not in service_h:
        fail(f"UI service declaration missing: {method}")
    if f"FOGUiViewModelService::{method}" not in service_cpp:
        fail(f"UI service implementation missing: {method}")

if "Columns = 3" not in models:
    fail("roster default density is not three columns")
if "Query.Columns != 2" not in service_cpp or "Query.Columns != 3" not in service_cpp:
    fail("roster 2/3-column density validation is missing")
if "bInternalFormulaBreakdownVisible = false" not in models:
    fail("player-facing character/equipment models do not explicitly hide formula decomposition")
if "bRelationshipGatePresent = false" not in models:
    fail("adult utility does not explicitly guarantee no relationship access gate")
if "bFastPrivacySfwToggleAvailable = true" not in models:
    fail("adult utility lacks fast Privacy/SFW presentation control")
if "bPersistentMmoOverlay = false" not in models:
    fail("World HUD no-MMO-overlay contract is missing")
if 'FName(TEXT("upper_right"))' not in models:
    fail("World HUD upper-right companion placement is missing")
if 'FName(TEXT("known"))' not in service_cpp or 'FName(TEXT("experiment"))' not in service_cpp:
    fail("Known/Experiment crafting modes are not implemented")
if "1.0f" not in service_cpp or "2.0f" not in service_cpp or "3.0f" not in service_cpp:
    fail("turn-battle 1x/2x/3x speed options are incomplete")
if "DirectDamage" not in models or "DotDamage" not in models or "TotalHealing" not in models:
    fail("battle recap does not expose exactly the three frozen player-facing categories")
for forbidden in ("Shielding", "DamagePrevented", "DamageTaken", "CritAnalytics"):
    if forbidden in models:
        fail(f"normal battle recap leaked forbidden analytics: {forbidden}")
if "AuthoredBandThresholdBps" not in service_h:
    fail("risk bands are not explicitly tuning/content-owned")
if "Clamped < 2500" in service_cpp or "Clamped < 5000" in service_cpp or "Clamped < 7500" in service_cpp:
    fail("UI layer hard-codes risk-band tuning thresholds")
if 'TEXT("Exceed")' not in read("Source/OfflineGame/Private/UI/OGUiViewModelService.cpp"):
    fail("combat large-number overflow does not expose Exceed")
if 'TEXT("Unknown")' not in read("Source/OfflineGame/Private/UI/OGUiViewModelService.cpp"):
    fail("knowledge-hidden large numbers do not expose Unknown")

# 4) Persistence/query support required by the completed UI projections.
world_store = read("Source/OfflineGame/Public/Persistence/OGWorldStore.h")
sqlite_h = read("Source/OfflineGame/Private/Persistence/OGSQLiteWorldStore.h")
sqlite_items = read("Source/OfflineGame/Private/Persistence/OGSQLiteWorldStore.Items.cpp")
for marker in (
    "ListWorldEvents",
    "ListKnowledgeFactsByOwner",
    "ListEquipmentProficienciesByOwner",
    "ListProjectPhases",
    "ListProjectAssignments",
    "ListDispatchObjectives",
    "ListDispatchConstraints",
    "ListWarFronts",
    "ListWarObjectives",
    "ListWarOrders",
):
    if marker not in world_store:
        fail(f"public persistence query required by UI is missing: {marker}")
if "ListEquipmentProficienciesByOwner" not in sqlite_h or "FOGSQLiteWorldStore::ListEquipmentProficienciesByOwner" not in sqlite_items:
    fail("SQLite proficiency-list persistence parity is incomplete")

# 5) Explicit non-Unreal automation coverage must exist for every audited surface.
ui_tests = read("Source/OfflineGame/Private/Tests/OGPreUnrealUiCompletenessTests.cpp")
for marker in (
    "CharacterRosterDetailWardrobeAdultEquipment",
    "GachaRecordsTerritoryWorldHudTurnBattle",
    "RecoveryPackageProjectDispatchWarRisk",
):
    if marker not in ui_tests:
        fail(f"pre-Unreal completeness automation coverage missing: {marker}")

# 6) Recovery/package and canonical-world safety regressions audited earlier remain present.
required_regression_markers = {
    "Source/OfflineGame/Private/Tests/OGPackageReportRecoveryTests.cpp": [
        "snapshot",
        "dependency",
        "Clear World",
    ],
    "Source/OfflineGame/Private/Tests/OGGachaTests.cpp": [
        "ticket",
    ],
    "Source/OfflineGame/Private/Tests/OGItemKnowledgeCharacterTests.cpp": [
        "transfer",
    ],
    "Source/OfflineGame/Private/Tests/OGSovereigntyGachaAccessTests.cpp": [
        "anchor",
    ],
}
for rel, markers in required_regression_markers.items():
    corpus = read(rel).lower()
    for marker in markers:
        if marker.lower() not in corpus:
            fail(f"{rel}: expected audited regression coverage marker missing: {marker}")

# 7) Source declaration/definition parity for the UI projection service.
declared = set(re.findall(r"\b(?:bool|static\s+bool|static\s+FOG\w+|FOG\w+)\s+(Build\w+|ProjectRisk|SetActiveTerritoryOverlay|CharacterLastUsedContextId)\s*\(", service_h))
defined = set(re.findall(r"FOGUiViewModelService::(Build\w+|ProjectRisk|SetActiveTerritoryOverlay|CharacterLastUsedContextId)\s*\(", service_cpp))
for name in sorted(declared - defined):
    fail(f"FOGUiViewModelService declaration has no definition: {name}")
for name in sorted(defined - declared):
    fail(f"FOGUiViewModelService definition has no declaration: {name}")

print("Pre-Unreal completeness audit:")
if ERRORS:
    for error in ERRORS:
        print(f"ERROR: {error}")
    print(f"FAILED with {len(ERRORS)} error(s).")
    sys.exit(1)

print("PASSED: reconciled source/docs/UI/recovery/package contracts are statically complete.")
