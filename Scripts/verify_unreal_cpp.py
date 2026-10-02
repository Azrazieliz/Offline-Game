#!/usr/bin/env python3
from __future__ import annotations

import json
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

# 0) Raw source sanity before any Unreal-specific parsing.
valid_include = re.compile(
    r'^\s*#include\s+(?:"[^"]+"|<[^>]+>)\s*(?://.*)?$'
)
for path in all_cpp:
    text = read(path)

    if r"\n#include" in text or r"\r#include" in text:
        err(path.relative_to(ROOT), "literal escaped newline embedded before #include")

    for line_no, line in enumerate(text.splitlines(), start=1):
        if line.lstrip().startswith("#include") and not valid_include.fullmatch(line):
            err(
                path.relative_to(ROOT),
                f"malformed #include directive at line {line_no}: {line.strip()!r}",
            )

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

    reflected_body_types = len(re.findall(r"\bU(?:CLASS|STRUCT)\s*\(", text))
    generated_bodies = text.count("GENERATED_BODY()")
    if reflected_body_types != generated_bodies:
        err(
            h.relative_to(ROOT),
            f"reflected class/struct count ({reflected_body_types}) does not match GENERATED_BODY count ({generated_bodies})",
        )

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

# 5) Automation-test declaration/implementation parity.
test_dir = SOURCE / "Private" / "Tests"
for p in test_dir.glob("*.cpp"):
    text = read(p)
    declared_tests = re.findall(
        r"IMPLEMENT_SIMPLE_AUTOMATION_TEST\s*\(\s*(\w+)",
        text,
    )
    implemented_tests = set(re.findall(r"bool\s+(\w+)::RunTest\s*\(", text))

    for test_name in declared_tests:
        if test_name not in implemented_tests:
            err(p.relative_to(ROOT), f"automation test {test_name} has no RunTest implementation")

    for test_name in implemented_tests:
        if test_name not in declared_tests:
            err(p.relative_to(ROOT), f"RunTest implementation {test_name} has no automation-test declaration")

# 6) Dependency and project-descriptor sanity checks.
build_cs = SOURCE / "OfflineGame.Build.cs"
uproject_path = ROOT / "OfflineGame.uproject"
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

    if uproject_path.exists():
        try:
            descriptor = json.loads(read(uproject_path))
        except json.JSONDecodeError as exc:
            err(uproject_path.relative_to(ROOT), f"invalid JSON: {exc}")
        else:
            if descriptor.get("EngineAssociation") != "5.8":
                err(
                    uproject_path.relative_to(ROOT),
                    f"EngineAssociation must remain '5.8', found {descriptor.get('EngineAssociation')!r}",
                )

            module_names = {
                module.get("Name")
                for module in descriptor.get("Modules", [])
                if isinstance(module, dict)
            }
            if "OfflineGame" not in module_names:
                err(uproject_path.relative_to(ROOT), "OfflineGame runtime module is missing")

            if '"SQLiteCore"' in build:
                enabled_plugins = {
                    plugin.get("Name")
                    for plugin in descriptor.get("Plugins", [])
                    if isinstance(plugin, dict) and plugin.get("Enabled") is True
                }
                if "SQLiteCore" not in enabled_plugins:
                    err(
                        uproject_path.relative_to(ROOT),
                        "SQLiteCore is a Build.cs dependency but is not enabled in the project descriptor",
                    )

# 7) Guard against accidentally reintroducing known stale design/code drift.
version_ids_declared = any(
    re.search(r"\\bVersionIds\\b\\s*(?:=|;)", read(path))
    for path in headers
)
for path in all_cpp:
    text = read(path)
    if "2 Crit Rate" in text and "1 Crit Damage" in text:
        err(path.relative_to(ROOT), "stale crit-overflow direction detected")

    if not version_ids_declared and re.search(r"\\.\\s*VersionIds\\b", text):
        err(
            path.relative_to(ROOT),
            "stale VersionIds member access: Character Version points to Identity; "
            "Identity does not own a mutable Version list",
        )


# 8) G2 Android runtime baseline sanity.
android_config = ROOT / "Config" / "DefaultEngine.ini"
if not android_config.exists():
    err("Config/DefaultEngine.ini", "G2 requires an Android runtime configuration")
else:
    android = read(android_config)
    required_android_settings = {
        "PackageName=com.azrazieliz.[PROJECT]": "provisional package identity",
        "MinSDKVersion=26": "minimum install SDK",
        "TargetSDKVersion=35": "UE 5.8 target SDK",
        "Orientation=Sensor": "portrait/landscape sensor orientation",
        "bBuildForArm64=True": "ARM64 target",
        "bBuildForX8664=False": "x86_64 disabled",
        "bSupportsVulkan=True": "Vulkan mobile support",
        "bSupportsVulkanSM5=False": "experimental Vulkan SM5 disabled",
        "bUseExternalFilesDir=True": "app-specific Android storage",
    }
    for setting, purpose in required_android_settings.items():
        if setting not in android:
            err(
                android_config.relative_to(ROOT),
                f"missing G2 Android setting for {purpose}: {setting}",
            )

print(f"Unreal C++ preflight scanned {len(headers)} headers and {len(cpps)} cpp files.")
for w in WARNINGS:
    print(f"WARNING: {w}")
if ERRORS:
    for e in ERRORS:
        print(f"ERROR: {e}")
    print(f"Preflight FAILED with {len(ERRORS)} error(s).")
    sys.exit(1)

print("Preflight PASSED.")
