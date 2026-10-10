"""Read-only, no-Unreal-process audit of the saved Koikatsu material dependency boundary.

This is NOT a shader compilation test, a cold-reload test or an Android visual test.
Only targets the isolated Gate12 worktree; never edits any .uasset or raw source.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import sys

EXPECTED_BRANCH = "production/gate12-koikatsu-ue583-fixture-20261010"
EXPECTED_RECOVERY = "aff5f4fdeed6a4f11d7e5438ace6208f93e00603"
GATE12_FOLDER = "OfflineGame_Gate12_Koikatsu_20261010"
REF_PATTERN = re.compile(rb"/InterchangeAssets/[A-Za-z0-9_./-]+")


def sha256_file(p: pathlib.Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as fh:
        for block in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def audit(root: pathlib.Path, report: pathlib.Path) -> dict:
    if root.name != GATE12_FOLDER or not (root / "OfflineGame.uproject").is_file():
        raise ValueError("Refusing non-isolated Gate 12 Unreal project")
    ledger = json.loads(report.read_text(encoding="utf-8-sig"))
    rows = ledger.get("processed", [])
    if len(rows) != 15:
        raise ValueError(f"Expected 15 documented morph material slots; found {len(rows)}")
    output = {
        "status": "PASS_STATIC_ASSET_INVENTORY_ONLY",
        "scope": "NO_UNREAL_LOAD_NO_NATIVE_SHADER_COMPILATION",
        "expected_recovery_commit": EXPECTED_RECOVERY,
        "expected_branch": EXPECTED_BRANCH,
        "slots": [],
        "external_parent_material_slots": 0,
        "local_material_sha256_count": 0,
        "native_cold_reload": "NOT_TESTED",
        "native_material_shader_persistence": "NOT_TESTED",
        "android_face_eyes_hair": "NOT_TESTED",
    }
    seen = set()
    for row in rows:
        object_path = row["slot_path"]
        pkg_path = object_path.split(".")[0]
        if not pkg_path.startswith("/Game/Experimental/Gate12Koikatsu/"):
            raise ValueError("Outside experimental content root: " + object_path)
        if object_path in seen:
            raise ValueError("Duplicate slot in report: " + object_path)
        seen.add(object_path)
        package = (root / "Content" / (pkg_path[len("/Game/"):] + ".uasset")).resolve()
        if not package.is_relative_to((root / "Content").resolve()) or not package.is_file():
            raise FileNotFoundError(str(package))
        data = package.read_bytes()
        parents = sorted({m.decode("ascii") for m in REF_PATTERN.findall(data)})
        external = any("/InterchangeAssets/gltf/" in ref for ref in parents)
        output["slots"].append({
            "material_slot": object_path,
            "asset_file_relative": package.relative_to(root).as_posix(),
            "asset_bytes": len(data),
            "asset_sha256": sha256_file(package),
            "external_import_shader_references": parents,
            "needs_cold_native_shader_proof": True,
            "previous_in_session_flag": row.get("after"),
        })
        output["external_parent_material_slots"] += int(external)
    output["local_material_sha256_count"] = len(output["slots"])
    output["finding"] = (
        "All saved slot assets have been hashed. Imported material-instance parent refs "
        "are not proof that UE 5.8.3 serialized MorphTargets usage in the base shader. "
        "Cold reload AND Android visual evidence remain mandatory."
    )
    return output


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project", type=pathlib.Path, required=True)
    parser.add_argument("--report", type=pathlib.Path)
    parser.add_argument("--output", type=pathlib.Path)
    a = parser.parse_args()
    root = a.project.resolve(strict=True)
    report = a.report or root / "Saved/Gate12Koikatsu/morph_material_usage_fix_report.json"
    dest = a.output or root / "Saved/Gate12Koikatsu/material_parent_ownership_static_audit.json"
    try:
        result = audit(root, report)
    except Exception as exc:
        result = {"status": "FAIL_STATIC_INVENTORY", "error": str(exc)}
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "status": result["status"],
        "slot_count": len(result.get("slots", [])),
        "external_parent_slots": result.get("external_parent_material_slots"),
        "report": str(dest),
        "error": result.get("error"),
    }))
    return 0 if result["status"].startswith("PASS") else 2


if __name__ == "__main__":
    sys.exit(main())
