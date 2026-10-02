#!/usr/bin/env python3

from __future__ import annotations

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    print(f"ERROR: {message}")
    raise SystemExit(1)


def validate_uproject() -> None:
    path = ROOT / "OfflineGame.uproject"
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("EngineAssociation") != "5.8":
        fail("OfflineGame.uproject must remain pinned to Unreal Engine 5.8 until deliberately migrated.")

    plugins = {item.get("Name"): item for item in data.get("Plugins", [])}
    if not plugins.get("SQLiteCore", {}).get("Enabled"):
        fail("SQLiteCore plugin must be enabled.")


def validate_migrations() -> None:
    migration_dir = ROOT / "Database" / "Migrations"
    migrations = sorted(migration_dir.glob("*.sql"))
    if not migrations:
        fail("At least one database migration is required.")

    versions = []
    for migration in migrations:
        match = re.fullmatch(r"(\d{4})_[a-z0-9_]+\.sql", migration.name)
        if not match:
            fail(f"Invalid migration filename: {migration.name}")
        versions.append(int(match.group(1)))

    expected = list(range(1, len(versions) + 1))
    if versions != expected:
        fail(f"Migration sequence must be contiguous. Found {versions}, expected {expected}.")


def validate_required_files() -> None:
    required = [
        "README.md",
        "docs/ARCHITECTURE.md",
        "docs/DATA_MODEL.md",
        "docs/VERTICAL_SLICE.md",
        "Source/OfflineGame/Public/Persistence/OGWorldStore.h",
    ]
    for relative in required:
        if not (ROOT / relative).exists():
            fail(f"Missing required file: {relative}")


def main() -> int:
    validate_required_files()
    validate_uproject()
    validate_migrations()
    print("Repository validation passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
