"""Read-only independent UE 5.8.3 COLD EDITOR process shader persistence verifier.

Run in a fresh UnrealEditor-Cmd process AFTER repair_kk_morph_shader_ownership_ue583.py
has exited. This script does not save any assets and NEVER certifies phone visuals.
"""
import hashlib
import json
import os
import pathlib
import time
import traceback
import unreal

ROOT = pathlib.Path(os.environ.get("G12_GATE12_WORKTREE",
                           r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010")).resolve()
SOURCE = ROOT / "Saved/Gate12Koikatsu/morph_material_usage_fix_report.json"
PREVIOUS = ROOT / "Saved/Gate12Koikatsu/owned_morph_shader_repair_report.json"
DEST = ROOT / "Saved/Gate12Koikatsu/owned_morph_shader_cold_reload_report.json"
OWNED = "/Game/Experimental/Gate12Koikatsu/MaterialShaderOverrides/"
USAGE = unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS
ML = unreal.MaterialEditingLibrary
result = {"status": "STARTED", "mode": "COLD_EDITOR_READ_ONLY",
          "materials": [], "android_visuals": "NOT_TESTED",
          "mobile_shader_compile": "NOT_TESTED"}


def require(ok, why):
    if not ok:
        raise RuntimeError(why)


def digest(p):
    h = hashlib.sha256()
    with p.open("rb") as fh:
        for block in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


try:
    require("OfflineGame_Gate12_Koikatsu_20261010" in
            str(unreal.Paths.get_project_file_path()), "Wrong isolated project")
    previous = json.loads(PREVIOUS.read_text(encoding="utf8"))
    require(previous["status"] == "PASS_SAVED_REPARENT_PENDING_COLD_RELOAD",
            "No previously saved native material repair to verify")
    rows = json.loads(SOURCE.read_text(encoding="utf-8-sig"))["processed"]
    require(len(rows) == 15, "Unexpected base material inventory")
    scope = previous.get("scope", "all")
    require(scope in ("face", "all"), "Invalid repair scope")
    if scope == "face":
        rows = [r for r in rows if r["slot_path"].endswith("/KK_cf_m_face_00.KK_cf_m_face_00")]
    expected = {x["slot_path"]: x for x in previous["materials_reparented"]}
    require(len(expected) == len(rows) == previous.get("expected_materials", len(rows)),
            "Unmatched repaired material slot count")
    for row in rows:
        path = row["slot_path"]
        expected_row = expected[path]
        imported = unreal.load_asset(path)
        require(isinstance(imported, unreal.MaterialInstanceConstant),
                "Missing saved local instance: " + path)
        parent = imported.get_editor_property("parent")
        require(parent is not None and
                parent.get_path_name() == expected_row["new_parent"],
                "Cold reload parent not persisted: " + path)
        current = parent
        parents = []
        visited = set()
        while isinstance(current, unreal.MaterialInstance):
            path_name = current.get_path_name()
            require(path_name not in visited, "Cyclic shader chain " + path_name)
            visited.add(path_name)
            require(path_name.startswith(OWNED),
                    "Found external shader parent after save: " + path_name)
            parents.append(path_name)
            current = current.get_editor_property("parent")
            require(current is not None, "Broken material instance chain")
        require(isinstance(current, unreal.Material), "No saved UMaterial root")
        root_name = current.get_path_name()
        require(root_name.startswith(OWNED), "Shader root not project-owned " + root_name)
        require(bool(ML.has_material_usage(current, USAGE)),
                "MATUSAGE_MORPH_TARGETS cold reload false: " + root_name)
        pkg = path.split(".", 1)[0]
        local = ROOT / "Content" / (pkg[len("/Game/"):] + ".uasset")
        require(local.is_file(), "No .uasset on disk: " + path)
        leaf_sha = digest(local)
        require(leaf_sha == expected_row["after_sha256"],
                "Material bytes changed since repair or were not saved: " + path)
        base_pkg = root_name.split(".", 1)[0]
        base_file = ROOT / "Content" / (base_pkg[len("/Game/"):] + ".uasset")
        require(base_file.is_file(), "Native shader base not on disk " + root_name)
        result["materials"].append({
            "slot_path": path, "parent_chain": parents, "base_material": root_name,
            "morph_usage_cold_reload": True, "leaf_sha256": leaf_sha,
            "shader_base_sha256": digest(base_file)})
    result["status"] = "PASS_PERSISTED_MATERIAL_PARENT_AND_MORPH_FLAG_ONLY"
except Exception as exc:
    result["status"] = "FAIL_COLD_RELOAD_PERSISTENCE"
    result["error"] = str(exc)
    result["trace"] = traceback.format_exc()[-2400:]
finally:
    result["timestamp_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    DEST.write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
    unreal.log("G12_KK_MORPH_COLD_RELOAD_" + result["status"])
