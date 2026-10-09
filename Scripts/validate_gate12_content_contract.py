#!/usr/bin/env python3
"""Gate 12 source-only content/DTO compatibility gate; never installs or mounts content."""
from __future__ import annotations

import argparse
import copy
import json
import re
import sys
from pathlib import Path

BASE = Path(__file__).resolve().parents[1]
SAMPLE = BASE / "Content/Data/Production/Gate12RulerShellContractSmoke.json"
ID = re.compile(r"^[a-z0-9_.\-/]+:[a-z0-9_.\-/]+$", re.ASCII)
FOUNDATION = "3f54f07cacbf193e160d0be15a7be5a7bc730c3b"
DTO_KEYS = {"PackageId", "Version", "Dependencies", "CharacterIdentities", "CharacterVersions"}
DEP_KEYS = {"PackageId", "MinimumVersion"}
SHELL_DESTINATIONS = ["Home", "Characters", "Gacha", "Territory", "Records"]


def validate(doc: object) -> list[str]:
    problems: list[str] = []

    def fail(msg: str) -> None:
        problems.append(msg)

    if not isinstance(doc, dict):
        return ["Root must be an object."]
    if doc.get("schema") != "gate12.embedded-contract-smoke.v1":
        fail("Unexpected schema.")
    authority = doc.get("authority")
    if not isinstance(authority, dict) or authority.get("foundation_commit") != FOUNDATION:
        fail("Foundation reference is not the verified frozen commit.")
    if not isinstance(authority, dict) or authority.get("non_diagnostic") is not True:
        fail("Sample must be explicitly non-diagnostic.")
    if doc.get("external_container") is not False:
        fail("Source-only sample may not claim an external container.")
    if doc.get("production_state") != "designing":
        fail("Smoke sample must not impersonate approved/integrated production.")
    boundaries = doc.get("boundaries")
    if not isinstance(boundaries, dict) or any(
        boundaries.get(k) is not False
        for k in ("stored_world_data", "installs_or_activates_packages", "guarantees_unreal_import")
    ) or boundaries.get("requires_gate11_approval_before_production_import") is not True:
        fail("Trust/provenance boundaries must remain explicit.")

    entries = doc.get("native_manifests")
    if not isinstance(entries, list) or len(entries) != 2:
        fail("Exactly two source-only native manifest DTO examples are required.")
        entries = []
    by_id: dict[str, dict] = {}
    for i, package in enumerate(entries):
        if not isinstance(package, dict):
            fail(f"native_manifests[{i}] is not an object.")
            continue
        if set(package) != DTO_KEYS:
            fail(f"native_manifests[{i}] fields differ from frozen FOGContentPackageManifest.")
            continue
        pid, version = package["PackageId"], package["Version"]
        if not isinstance(pid, str) or not ID.fullmatch(pid):
            fail(f"native_manifests[{i}] has invalid FOGContentId.")
            continue
        if pid in by_id:
            fail(f"Duplicate package ID {pid}.")
        if type(version) is not int or version < 1:
            fail(f"Invalid version for {pid}.")
        if package["CharacterIdentities"] != [] or package["CharacterVersions"] != []:
            fail("Smoke does not author Character Identity or Version data.")
        if not isinstance(package["Dependencies"], list):
            fail(f"{pid} dependencies must be an array.")
            continue
        seen: set[str] = set()
        for dep in package["Dependencies"]:
            if not isinstance(dep, dict) or set(dep) != DEP_KEYS:
                fail(f"{pid} dependency shape differs from FOGPackageDependency.")
                continue
            did, minimum = dep["PackageId"], dep["MinimumVersion"]
            if not isinstance(did, str) or not ID.fullmatch(did):
                fail(f"{pid} dependency has invalid ID.")
            if type(minimum) is not int or minimum < 1:
                fail(f"{pid} dependency has invalid minimum version.")
            if did in seen or did == pid:
                fail(f"{pid} duplicate or self dependency.")
            seen.add(did)
        by_id[pid] = package

    for pid, package in by_id.items():
        for dep in package["Dependencies"]:
            if not isinstance(dep, dict) or set(dep) != DEP_KEYS:
                continue
            other = by_id.get(dep["PackageId"])
            if other is None:
                fail(f"{pid} references missing dependency {dep['PackageId']}.")
            elif type(dep["MinimumVersion"]) is int and type(other["Version"]) is int and other["Version"] < dep["MinimumVersion"]:
                fail(f"{pid} dependency {dep['PackageId']} is below minimum version.")

    visiting: set[str] = set()
    done: set[str] = set()

    def dfs(pid: str) -> None:
        if pid in done:
            return
        if pid in visiting:
            fail(f"Dependency cycle at {pid}.")
            return
        visiting.add(pid)
        package = by_id[pid]
        for dep in package["Dependencies"]:
            if isinstance(dep, dict) and dep.get("PackageId") in by_id:
                dfs(dep["PackageId"])
        visiting.remove(pid)
        done.add(pid)

    for pid in by_id:
        dfs(pid)

    content = doc.get("content")
    if not isinstance(content, dict):
        fail("Missing content object.")
    else:
        if content.get("ContentId") != "ui:ruler_shell.primary_destinations":
            fail("Content ID does not match the scoped navigation projection.")
        if content.get("OwnerPackageId") != "ui:ruler_shell_navigation" or content.get("OwnerPackageId") not in by_id:
            fail("Navigation owner package is absent.")
        if content.get("ContentKind") != "ruler_shell_navigation_projection_contract":
            fail("Unknown sample content kind.")
        if content.get("PrimaryDestinations") != SHELL_DESTINATIONS:
            fail("Navigation differs from frozen BuildRulerShell projection.")
        if content.get("OptionalVisualAsset") is not None:
            fail("Absent visual asset must remain null, never a fake path.")
        if content.get("MissingOptionalAssetPolicy") != "preserve_canonical_view_model":
            fail("Optional media omission must preserve canonical view model.")
    return problems


def check_unreal_source(source: Path) -> list[str]:
    text = source.read_text(encoding="utf-8")
    start = text.find("bool FOGUiViewModelService::BuildRulerShell(")
    end = text.find("bool FOGUiViewModelService::BuildRoster(", start)
    if start < 0 or end < 0:
        return ["Unable to locate BuildRulerShell definition in supplied C++ source."]
    actual = re.findall(r"EOGRulerPrimaryDestination::([A-Za-z_]+)", text[start:end])
    return [] if actual == SHELL_DESTINATIONS else [f"Frozen C++ destinations differ: {actual!r}"]


def self_test(original: dict) -> None:
    assert not validate(original), validate(original)
    tests = [
        ("uppercase ID", lambda d: d["native_manifests"][0].update(PackageId="UI:Wrong")),
        ("missing dependency", lambda d: d["native_manifests"][1]["Dependencies"][0].update(PackageId="ui:absent")),
        ("unmet minimum", lambda d: d["native_manifests"][1]["Dependencies"][0].update(MinimumVersion=2)),
        ("cycle", lambda d: d["native_manifests"][0]["Dependencies"].append({"PackageId": "ui:ruler_shell_navigation", "MinimumVersion": 1})),
        ("fake uasset path", lambda d: d["content"].update(OptionalVisualAsset="/Game/Fake.Fake")),
        ("untrusted installation", lambda d: d["native_manifests"][0].update(bValidated=True)),
        ("altered UI contract", lambda d: d["content"].update(PrimaryDestinations=["Home"])),
    ]
    for label, mutate in tests:
        bad = copy.deepcopy(original)
        mutate(bad)
        assert validate(bad), f"Negative test accepted: {label}"
    print(f"PASS: 1 positive, {len(tests)} negative source-contract cases.")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("manifest", nargs="?", type=Path, default=SAMPLE)
    ap.add_argument("--check-runtime-source", type=Path, help="Check destinations against checked-out frozen OGUiViewModelService.cpp")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--emit-adapter", type=Path, help="Write planned native DTO mapping; does NOT register or mount packages")
    args = ap.parse_args()
    doc = json.loads(args.manifest.read_text(encoding="utf-8"))
    errors = validate(doc)
    if args.check_runtime_source:
        errors.extend(check_unreal_source(args.check_runtime_source))
    if errors:
        for err in errors:
            print("FAIL:", err, file=sys.stderr)
        return 1
    if args.self_test:
        self_test(doc)
    if args.emit_adapter:
        mapping = {
            "schema": "gate12.native_dto_source_map.v1",
            "runtime_manifest_dtos": doc["native_manifests"],
            "package_dependency_records": [
                {"PackageId": package["PackageId"], "DependencyPackageId": dep["PackageId"], "MinimumVersion": dep["MinimumVersion"]}
                for package in doc["native_manifests"] for dep in package["Dependencies"]
            ],
            "source_only": True,
            "native_registration_performed": False,
            "trusted_installation_provenance": None,
        }
        args.emit_adapter.parent.mkdir(parents=True, exist_ok=True)
        args.emit_adapter.write_bytes((json.dumps(mapping, indent=2) + "\n").encode("utf-8"))
    print("PASS: sample matches frozen content ID, DTO, dependency and safe omission rules (source-only).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
