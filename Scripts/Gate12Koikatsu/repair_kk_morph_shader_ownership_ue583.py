"""UE 5.8.3 isolated fixture: duplicate importer-owned shader parent chain into /Game.

Run ONLY in authorized high-RAM UE host with G12_MATERIAL_REPAIR_APPROVED=1.
Existing material instances keep their texture/scalar/vector parameters and source art.
A separate cold-editor process MUST validate persistence; no Android PASS here.
"""
import hashlib
import importlib.util
import json
import os
import socket
import pathlib
import re
import shutil
import subprocess
import time
import traceback
import unreal

ROOT = pathlib.Path(os.environ.get("G12_GATE12_WORKTREE",
                           r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010")).resolve()
SOURCE = ROOT / "Saved/Gate12Koikatsu/morph_material_usage_fix_report.json"
DEST = ROOT / "Saved/Gate12Koikatsu/owned_morph_shader_repair_report.json"
OWNED = "/Game/Experimental/Gate12Koikatsu/MaterialShaderOverrides"
FIXTURE = "/Game/Experimental/Gate12Koikatsu/"
BASE_EXPECTED = "/InterchangeAssets/gltf/"
FLAGS = unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

receipt = {"status": "STARTED", "ue_verification": "NOT_TESTED",
           "android_shader_visuals": "NOT_TESTED", "parent_copies": [],
           "materials_reparented": [], "backup_dir": "", "errors": []}


def flush():
    DEST.parent.mkdir(parents=True, exist_ok=True)
    DEST.write_text(json.dumps(receipt, indent=2, default=str) + "\n", encoding="utf-8")


def require(cond, message):
    if not cond:
        raise RuntimeError(message)


def sha(p):
    h = hashlib.sha256()
    with p.open("rb") as fh:
        for block in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def branch():
    return subprocess.check_output(["git", "-C", str(ROOT), "branch",
                                    "--show-current"], text=True).strip()


def package_file(object_name):
    pkg = object_name.split(".", 1)[0]
    require(pkg.startswith(FIXTURE), "Outside isolated experimental package: " + object_name)
    path = ROOT / "Content" / (pkg[len("/Game/"):] + ".uasset")
    require(path.is_file(), "Missing on-disk imported material " + str(path))
    return path


def short_name(asset):
    return re.sub(r"[^A-Za-z0-9_]", "_", asset.get_name())[:55]


def local_name(asset):
    unique = hashlib.sha256(asset.get_path_name().encode("utf8")).hexdigest()[:12]
    prefix = "M_" if isinstance(asset, unreal.Material) else "MI_"
    return f"{OWNED}/{prefix}G12_{short_name(asset)}_{unique}"


def material_properties(instance):
    # The values reside on each imported material instance; changing its parent
    # must not overwrite those imported parameter arrays.
    return {key: str(instance.get_editor_property(key))
            for key in ("scalar_parameter_values", "vector_parameter_values",
                        "texture_parameter_values")}


clone_cache = {}


def clone_external_chain(source):
    source_path = source.get_path_name()
    if source_path.startswith(OWNED + "/"):
        return source
    require(source_path.startswith(BASE_EXPECTED),
            "Unreviewed shader parent origin: " + source_path)
    if source_path in clone_cache:
        return clone_cache[source_path]
    require(isinstance(source, (unreal.Material, unreal.MaterialInstanceConstant)),
            "Unknown material parent type: " + source_path)
    dest_path = local_name(source)
    if EAL.does_asset_exist(dest_path):
        existing = unreal.load_asset(dest_path)
        require(isinstance(existing, type(source)),
                "Unexpected existing local shader class: " + dest_path)
        if isinstance(existing, unreal.Material):
            require(bool(MEL.has_material_usage(existing, FLAGS)),
                    "Existing project-owned shader lacks persisted morph usage")
        else:
            source_parent = source.get_editor_property("parent")
            require(source_parent is not None, "External shader chain has no base")
            expected_parent = clone_external_chain(source_parent)
            require(existing.get_editor_property("parent").get_path_name() ==
                    expected_parent.get_path_name(), "Existing local parent chain differs")
        clone_cache[source_path] = existing
        receipt.setdefault("parent_reused", []).append({
            "original":source_path,"existing":existing.get_path_name()})
        flush()
        return existing

    if isinstance(source, unreal.MaterialInstanceConstant):
        source_parent = source.get_editor_property("parent")
        require(source_parent is not None, "Material instance has no parent: " + source_path)
        new_parent = clone_external_chain(source_parent)
    else:
        new_parent = None

    copied = EAL.duplicate_asset(source_path, dest_path)
    require(copied is not None, "Duplicate material parent failed: " + source_path)
    if isinstance(source, unreal.Material):
        require(isinstance(copied, unreal.Material), "Cloned material is not UMaterial")
        MEL.set_base_material_usage(copied, FLAGS, True)
        require(bool(MEL.has_material_usage(copied, FLAGS)),
                "Morph usage missing on cloned material: " + dest_path)
        # Shader permutations are compiled separately by Android cooking.
        # This step verifies only persisted native material usage and ownership.
    else:
        require(isinstance(copied, unreal.MaterialInstanceConstant),
                "Cloned shader instance wrong type: " + dest_path)
        MEL.set_material_instance_parent(copied, new_parent)
        MEL.update_material_instance(copied)
        require(copied.get_editor_property("parent").get_path_name() ==
                new_parent.get_path_name(), "Cloned shader-parent mismatch")
    require(EAL.save_loaded_asset(copied), "Native cloned shader save failed: " + dest_path)
    clone_cache[source_path] = copied
    receipt["parent_copies"].append({"original": source_path, "new": copied.get_path_name()})
    flush()
    return copied


flush()
changed = []
try:
    require(os.environ.get("G12_MATERIAL_REPAIR_APPROVED") == "1",
            "Explicit G12_MATERIAL_REPAIR_APPROVED=1 is required")
    require(os.environ.get("G12_GUARDED_LAUNCH") == "1",
            "Requires guarded UE launch via the resource-monitored wrapper")
    require("OfflineGame_Gate12_Koikatsu_20261010" in
            str(unreal.Paths.get_project_file_path()), "Wrong Unreal project")
    require(ROOT.is_dir() and branch() ==
            "production/gate12-koikatsu-ue583-fixture-20261010", "Wrong branch")
    host = socket.gethostname().upper()
    # Single-PC workflow: hardware capacity is not a blanket exclusion.
    # Native mutation requires enough FREE physical RAM; an external watchdog
    # still terminates the editor before the measured 850 MiB safety floor.
    preflight = ROOT / "Scripts/Gate12Koikatsu/preflight_kk_native_shader_host.py"
    require(preflight.is_file(), "Missing fail-closed physical-memory guard module")
    spec = importlib.util.spec_from_file_location("_gate12_host_memory_guard", str(preflight))
    require(spec is not None and spec.loader is not None, "Cannot load physical-memory guard")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    total_bytes, free_bytes = module.physical_memory()
    receipt["host_preflight"] = {
        "hostname": host, "installed_gib": round(total_bytes / module.GIB, 3),
        "available_gib": round(free_bytes / module.GIB, 3)}
    require(free_bytes >= 600 * module.MIB,
            "Insufficient current physical RAM margin before native material editing")
    flush()
    rows = json.loads(SOURCE.read_text(encoding="utf-8-sig"))["processed"]
    require(len(rows) == 15, "Unexpected original morph material count; stop and re-audit")
    scope = os.environ.get("G12_MATERIAL_SCOPE", "face")
    require(scope in ("face", "remaining", "all"), "Invalid material repair scope")
    if scope == "face":
        rows = [r for r in rows if r["slot_path"].endswith("/KK_cf_m_face_00.KK_cf_m_face_00")]
        require(len(rows) == 1, "Face material not uniquely located")
    if scope == "remaining":
        rows = [r for r in rows if not r["slot_path"].endswith("/KK_cf_m_face_00.KK_cf_m_face_00")]
        require(len(rows) == 14, "Unexpected original residual material count")
    receipt["scope"] = scope
    receipt["expected_materials"] = len(rows)
    receipt["materials_already_owned_verified"] = []
    entries = []
    for row in rows:
        material = unreal.load_asset(row["slot_path"])
        require(isinstance(material, unreal.MaterialInstanceConstant),
                "Expected imported material instance: " + row["slot_path"])
        parent = material.get_editor_property("parent")
        require(parent is not None, "Material instance has no shader parent")
        if parent.get_path_name().startswith(OWNED + "/"):
            base=parent.get_base_material() if isinstance(parent,unreal.MaterialInstance) else parent
            require(isinstance(base,unreal.Material) and bool(MEL.has_material_usage(base,FLAGS)),
                    "Previously owned shader lost morph usage: " + row["slot_path"])
            existing_path=package_file(row["slot_path"])
            receipt["materials_already_owned_verified"].append({
                "slot_path":row["slot_path"],"new_parent":parent.get_path_name(),
                "after_sha256":sha(existing_path),"already_persisted":True})
            continue
        require(parent.get_path_name().startswith(BASE_EXPECTED),
                "Material references unexpected external shader: " + row["slot_path"])
        entries.append((row["slot_path"], material, parent,
                        package_file(row["slot_path"]), material_properties(material)))
    stamp = time.strftime("%Y%m%d_%H%M%S", time.gmtime())
    backup = ROOT / "Saved/Gate12Koikatsu/MaterialRepairBackups" / stamp
    require(not backup.exists(), "Backup directory already exists")
    backup.mkdir(parents=True)
    receipt["backup_dir"] = str(backup)
    for obj_path, mat, original_parent, old_file, values in entries:
        shutil.copy2(old_file, backup / (old_file.stem + "_" +
                     hashlib.sha256(obj_path.encode()).hexdigest()[:10] + ".uasset"))
    flush()
    for obj_path, mat, original_parent, old_file, values in entries:
        require(module.physical_memory()[1] >= 550 * module.MIB,
                "Available physical RAM too low to continue native material editing")
        before_sha = sha(old_file)
        owned_parent = clone_external_chain(original_parent)
        MEL.set_material_instance_parent(mat, owned_parent)
        changed.append((mat, original_parent))
        MEL.update_material_instance(mat)
        require(mat.get_editor_property("parent").get_path_name() ==
                owned_parent.get_path_name(), "Leaf parent did not switch: " + obj_path)
        require(material_properties(mat) == values,
                "Imported material parameter values changed: " + obj_path)
        require(EAL.save_loaded_asset(mat), "Leaf material save failed: " + obj_path)
        after_sha = sha(old_file)
        require(after_sha != before_sha,
                "No on-disk material change after new shader parent: " + obj_path)
        receipt["materials_reparented"].append({
            "slot_path": obj_path, "previous_parent": original_parent.get_path_name(),
            "new_parent": owned_parent.get_path_name(),
            "before_sha256": before_sha, "after_sha256": after_sha,
            "parameter_arrays_preserved": True})
        flush()
    receipt["status"] = "PASS_SAVED_REPARENT_PENDING_COLD_RELOAD"
except Exception as e:
    receipt["status"] = "FAIL_REPAIR_ATTEMPTED_ROLLBACK"
    receipt["errors"].append(str(e))
    receipt["trace"] = traceback.format_exc()[-2100:]
    for mat, old_parent in reversed(changed):
        try:
            MEL.set_material_instance_parent(mat, old_parent)
            MEL.update_material_instance(mat)
            EAL.save_loaded_asset(mat)
        except Exception as rollback_exc:
            receipt["errors"].append("ROLLBACK_ERROR: " + str(rollback_exc))
finally:
    receipt["timestamp_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    flush()
    unreal.log("G12_KK_MORPH_OWNERSHIP_" + receipt["status"])
