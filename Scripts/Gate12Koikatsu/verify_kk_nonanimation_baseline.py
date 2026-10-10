"""Fail closed on any drift of the Gate12 v1 non-animation conversion reference.

This checks immutable pipeline *reference files*, not successful importing of
arbitrary new characters, Android runtime acceptance, or animation quality.
Source, frozen Foundation and user saves are not written or changed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import subprocess
import sys
import time


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git(root: pathlib.Path, *params: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(root), *params], text=True, stderr=subprocess.DEVNULL
    ).strip()


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--repo-root", type=pathlib.Path,
                   default=pathlib.Path(__file__).resolve().parents[2])
    p.add_argument("--manifest", type=pathlib.Path, default=None)
    p.add_argument("--skip-machine-foundation-check", action="store_true",
                   help="For read-only evaluation off the user's development machine")
    args = p.parse_args()
    root = args.repo_root.resolve(strict=True)
    manifest_file = (args.manifest or root / "docs/production/gate12"
                     / "koikatsu_nonanimation_contract_v1.json").resolve(strict=True)
    result = {
        "contract": "G12-KK-NONANIM-1.0.0",
        "status": "FAIL",
        "verified_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "checked": [],
        "errors": [],
        "generic_new_character_import": "NOT_TESTED",
        "animation_fidelity": "OPEN_UNAPPROVED",
        "gate11_artifact_ready": False,
    }
    try:
        m = json.loads(manifest_file.read_text(encoding="utf-8-sig"))
        if m.get("schema") != "GATE12_KOIKATSU_NONANIMATION_LOCK_1" or m.get(
            "freeze_id"
        ) != result["contract"]:
            raise ValueError("Unexpected non-animation freeze schema/version")
        if not re.fullmatch(r"[0-9a-f]{40}", m.get("baseline_commit", "")):
            raise ValueError("Invalid source baseline hash")
        if not m.get("locked_files") or len(m["locked_files"]) < 6:
            raise ValueError("Insufficient byte-locked conversion files")
        result["reference_commit"] = m["baseline_commit"]
        result["branch"] = git(root, "branch", "--show-current")
        if result["branch"] != m["isolated_branch"]:
            result["errors"].append("Verifier must execute in the authorized Gate12 worktree")
        if git(root, "cat-file", "-t", m["baseline_commit"]) != "commit":
            result["errors"].append("The validated baseline Git commit is missing")

        seen = set()
        for item in m["locked_files"]:
            rel = item["path"]
            expected = item["sha256"]
            category = item["evidence_scope"]
            if not isinstance(rel, str) or "\\" in rel or rel.startswith("/") or (
                ".." in pathlib.PurePosixPath(rel).parts
            ):
                raise ValueError("Unsafe content-lock file path")
            if rel in seen:
                raise ValueError("Duplicated content-lock identity")
            seen.add(rel)
            if not re.fullmatch(r"[0-9a-f]{64}", expected):
                raise ValueError("Malformed expected SHA256 for " + rel)
            path = (root / rel).resolve()
            if not path.is_relative_to(root) or not path.is_file():
                result["errors"].append("Absent authoritative file: " + rel)
                result["checked"].append({"path": rel, "status": "MISSING"})
                continue
            current = sha256(path)
            good = current == expected
            result["checked"].append({
                "path": rel, "status": "PASS" if good else "SHA_DRIFT",
                "evidence_scope": category, "sha256": current,
            })
            if not good:
                result["errors"].append("Frozen file has changed: " + rel)

        if not args.skip_machine_foundation_check:
            foundation = pathlib.Path(m["foundation_worktree"])
            if not foundation.is_dir():
                result["errors"].append("Missing frozen Foundation worktree")
            elif git(foundation, "rev-parse", "HEAD") != m["foundation_sha"]:
                result["errors"].append("Frozen Foundation commit drift")
            elif git(foundation, "status", "--porcelain"):
                result["errors"].append("Frozen Foundation worktree not clean")
            else:
                result["foundation"] = "PASS_UNMODIFIED"
            save = foundation / "Saved/OfflineGame/WorldState.db"
            if not save.is_file() or sha256(save) != m["foundation_save_sha256"]:
                result["errors"].append("Original saved-world hash differs")
            else:
                result["original_game_save"] = "PASS_UNMODIFIED"

        result["status"] = (
            "PASS_NONANIMATION_BASELINE_BYTE_LOCK_ONLY"
            if not result["errors"] else "FAIL_NONANIMATION_BASELINE_DRIFT"
        )
    except Exception as exc:
        result["errors"].append(str(exc))
        result["status"] = "FAIL_NONANIMATION_BASELINE_VALIDATION"
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0 if result["status"].startswith("PASS_") else 2


if __name__ == "__main__":
    sys.exit(main())
