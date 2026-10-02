#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Source" / "OfflineGame"
ERRORS: list[str] = []
WARNINGS: list[str] = []

def err(path: Path | str, message: str) -> None:
    ERRORS.append(f"{path}: {message}")

def warn(path: Path | str, message: str) -> None:
    WARNINGS.append(f"{path}: {message}")

def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")

headers = sorted(SOURCE.rglob("*.h"))
cpps = sorted(SOURCE.rglob("*.cpp"))
all_cpp = headers + cpps

# Map module-local include forms (Combat/X.h, Persistence/Y.h, OfflineGame.h).
include_map: set[str] = set()
for h in headers:
    for root_name in ("Public", "Private"):
        root = SOURCE / root_name
        try:
            include_map.add(h.relative_to(root).as_posix())
        except ValueError:
            pass

# 1) UHT/reflection hygiene.
reflection_re = re.compile(r"\bU(?:CLASS|STRUCT|ENUM|INTERFACE)\s*\(")
for h in headers:
    text = read(h)
    includes = re.findall(r'^\s*#include\s+"([^"]+)"', text, flags=re.M)
    generated = [x for x in includes if x.endswith(".generated.h")]
    has_reflection = bool(reflection_re.search(text))

    if has_reflection:
        expected = h.stem + ".generated.h"
        if generated != [expected]:
            err(h.relative_to(ROOT), f"reflected header must include exactly {expected!r}; found {generated!r}")
        elif includes[-1] != expected:
            err(h.relative_to(ROOT), ".generated.h must be the final #include")
    elif generated:
        warn(h.relative_to(ROOT), "contains .generated.h but no reflected declaration was detected")

    if re.search(r"UPROPERTY\s*\([^)]*\)\s*TFunction\b", text, flags=re.S):
        err(h.relative_to(ROOT), "TFunction cannot be a UPROPERTY")

    if re.search(r"\bFName\s+\w+\s*=\s*TEXT\s*\(", text):
        err(h.relative_to(ROOT), "ambiguous FName copy-initialization from TEXT literal; use FName(TEXT(...))")

# 2) Module-local quoted includes must exist.
local_prefixes = (
    "Characters/", "Combat/", "Content/", "Core/", "Effects/", "Events/",
    "Math/", "Persistence/", "Random/", "Rules/", "Runtime/", "Skills/", "World/"
)
for path in all_cpp:
    text = read(path)
    for inc in re.findall(r'^\s*#include\s+"([^"]+)"', text, flags=re.M):
        if inc == "OfflineGame.h" or inc.startswith(local_prefixes):
            if inc not in include_map:
                err(path.relative_to(ROOT), f"module-local include does not exist: {inc}")

# 3) Persistence class declaration/definition parity + duplicate definitions.
store_header = SOURCE / "Private" / "Persistence" / "OGSQLiteWorldStore.h"
if store_header.exists():
    store_h = read(store_header)
    declared = set(re.findall(
        r"\b(?:virtual\s+)?(?:bool|void|int32|FString|const\s+FString&)\s+(\w+)\s*\(",
        store_h
    ))
    definitions: list[str] = []
    for p in sorted((SOURCE / "Private" / "Persistence").glob("OGSQLiteWorldStore*.cpp")):
        definitions.extend(re.findall(r"FOGSQLiteWorldStore::(\w+)\s*\(", read(p)))

    counts = Counter(definitions)
    for name, count in counts.items():
        if count > 1:
            err("FOGSQLiteWorldStore", f"method {name} has {count} definitions")
        if name not in declared and name not in {"FOGSQLiteWorldStore", "~FOGSQLiteWorldStore"}:
            err("FOGSQLiteWorldStore", f"definition has no matching declaration: {name}")

    # Public non-inline declarations should all be implemented, except trivial IsOpen/GetDatabasePath.
    ignore = {"IsOpen", "GetDatabasePath"}
    for name in sorted(declared - set(definitions) - ignore):
        # private helpers may still be inline/covered by simplistic regex; only flag store API-like methods.
        if name.startswith(("Upsert", "TryRead", "Set", "IsContent", "Append", "Backup", "Restore",
                            "RunIntegrity", "Checkpoint", "Open", "Close", "Begin", "Commit",
                            "Rollback", "GetSchema", "Apply", "Record", "Ensure", "Execute", "Last")):
            err("FOGSQLiteWorldStore", f"declared method has no out-of-line definition: {name}")

# 4) Migration/test schema consistency.
migration_dir = ROOT / "Database" / "Migrations"
versions = []
if migration_dir.exists():
    for p in migration_dir.glob("*.sql"):
        m = re.match(r"(\d+)_", p.name)
        if m:
            versions.append(int(m.group(1)))
if versions:
    latest = max(versions)
    test_dir = SOURCE / "Private" / "Tests"
    for p in test_dir.glob("*.cpp"):
        text = read(p)
        # Match TestEqual(... Store.GetSchemaVersion(Error), N)
        for m in re.finditer(
            r"GetSchemaVersion\s*\(\s*Error\s*\)\s*,\s*(\d+)",
            text,
            flags=re.S
        ):
            actual = int(m.group(1))
            if actual != latest:
                err(p.relative_to(ROOT), f"schema assertion expects {actual}, latest migration is {latest}")

# 5) Dependency sanity checks tied to actual source use.
build_cs = SOURCE / "OfflineGame.Build.cs"
if build_cs.exists():
    build = read(build_cs)
    corpus = "\n".join(read(p) for p in all_cpp)
    required = []
    if "FJsonObject" in corpus or "JsonSerializer" in corpus:
        required.append("Json")
    if "sqlite3_" in corpus or "sqlite/sqlite3.h" in corpus:
        required.append("SQLiteCore")
    if "UUserWidget" in corpus or "UMG" in corpus:
        required.append("UMG")
    for module in required:
        if f'"{module}"' not in build:
            err(build_cs.relative_to(ROOT), f"source uses {module} but module dependency is missing")

# 6) Guard against accidentally reintroducing known stale design notes into code.
for path in all_cpp:
    text = read(path)
    if "2 Crit Rate" in text and "1 Crit Damage" in text:
        err(path.relative_to(ROOT), "stale crit-overflow direction detected")

print(f"Unreal C++ preflight scanned {len(headers)} headers and {len(cpps)} cpp files.")
for w in WARNINGS:
    print(f"WARNING: {w}")
if ERRORS:
    for e in ERRORS:
        print(f"ERROR: {e}")
    print(f"Preflight FAILED with {len(ERRORS)} error(s).")
    sys.exit(1)

print("Preflight PASSED.")
