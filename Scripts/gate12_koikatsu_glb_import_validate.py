"""Gate12 experimental, GLB-first Koikatsu import acceptance (isolated worktree ONLY).
DO NOT run while the computer is in active use: opening UE needs several GB RAM.
Not a production character. No packaging, no Android install, no user save IO.
"""
import hashlib, json, os, traceback, time
import unreal

WORKTREE = "OfflineGame_Gate12_Koikatsu_20261010"
ROOT = r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu"
GLB = r"D:\Tools\GameAssetPipeline\KoikatsuParty\characters\G01-KK-PIPELINE-TEST-0002\conversion\character_unreal_skeleton.glb"
SHA = "ad06955e6f725c63c6ee014828877a9a8fa3a19479ca29936c58c96ed6d012e0"
DEST = "/Game/Experimental/Gate12Koikatsu/GLBRecovered_20261010"
MAP = "/Game/Experimental/Gate12Koikatsu/Maps/L_GLB_Recovery_Acceptance"
report_path = os.path.join(ROOT, "glb_recovery_ue583_report.json")
report = {"input": GLB, "expected_sha256": SHA, "destination": DEST,
          "experimental_only": True, "gate11_artifact_ready": False, "device_validated": False,
          "status": "STARTED", "assertions": {}, "started": time.time(), "errors": []}

def save():
    os.makedirs(ROOT, exist_ok=True)
    with open(report_path, "w", encoding="utf8") as f:
        json.dump(report, f, indent=2, default=str)

def check(name, accepted, facts):
    report["assertions"][name] = {"status": "PASS" if accepted else "FAIL", "observed": facts}
    save()
    return accepted

save()
try:
    project = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    if WORKTREE.lower() not in project.lower():
        raise RuntimeError("REFUSING import outside isolated Gate 12 test worktree: " + project)
    check("isolated_project_path", True, project)
    if not os.path.isfile(GLB):
        raise FileNotFoundError(GLB)
    with open(GLB, "rb") as f:
        measured = hashlib.file_digest(f, "sha256").hexdigest()
    if not check("immutable_source_sha256", measured.lower() == SHA, measured):
        raise RuntimeError("Input hash mismatch. Aborting.")
    # UE 5.8 Interchange GenericMeshPipeline defaults bImportMorphTargets=true.
    # Do NOT attach FbxImportUI options to a GLB task: doing so uses the wrong pipeline.
    if unreal.EditorAssetLibrary.does_directory_exist(DEST):
        raise RuntimeError("Recovery output already exists; refusing overwrite")
    task = unreal.AssetImportTask()
    task.filename = GLB
    task.destination_path = DEST
    task.automated = True
    task.async_ = False
    task.save = True
    task.replace_existing = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    assets = []
    mesh = None
    blend_rows = []
    for path in unreal.EditorAssetLibrary.list_assets(DEST, recursive=True, include_folder=False):
        a = unreal.EditorAssetLibrary.load_asset(path)
        kind = a.get_class().get_name() if a else "UNLOADED"
        assets.append({"path": str(path), "class": kind})
        if isinstance(a, unreal.SkeletalMesh):
            mesh = a
        if a and kind in ("Material", "MaterialInstanceConstant"):
            try:
                blend_rows.append({"path": str(path), "mode": str(a.get_blend_mode())})
            except Exception as e:
                blend_rows.append({"path": str(path), "mode": "UNREADABLE: " + str(e)})
    report["asset_counts"] = {k: sum(a["class"] == k for a in assets)
                              for k in sorted(set(a["class"] for a in assets))}
    report["asset_paths"] = assets
    check("skeletal_mesh_import", mesh is not None, report["asset_counts"])
    if mesh is None:
        raise RuntimeError("glTF interchange produced no SkeletalMesh")
    skeleton = mesh.get_editor_property("skeleton")
    nbones = len(skeleton.get_editor_property("bone_tree")) if skeleton else 0
    morphs = len(mesh.get_editor_property("morph_targets"))
    check("skeletal_bone_count", 100 <= nbones <= 150, nbones)
    check("facial_morph_preservation", morphs > 0, morphs)
    translucent = [a for a in blend_rows if any(s in a["mode"].upper() for s in ("TRANSLUCENT", "MASKED"))]
    report["material_modes"] = blend_rows
    check("material_alpha_pipeline", len(translucent) > 0,
          {"transparent_or_masked": len(translucent), "total": len(blend_rows)})

    # Measure real UE bounds to detect the old FBX's 14,369 cm phantom extent.
    if nbones > 0:
        editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if not editor.new_level(MAP):
            raise RuntimeError("Cannot create isolated GLB acceptance level")
        a = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkeletalMeshActor,
                                      unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        a.set_actor_label("Gate12_Recovery_GLB_Skeletal_QA")
        a.skeletal_mesh_component.set_skeletal_mesh_asset(mesh)
        origin, extent = a.get_actor_bounds(False)
        ext = {"x": extent.x, "y": extent.y, "z": extent.z}
        check("rig_bounds_plausible_cm", (0 < extent.z * 2 < 250 and
                   extent.x * 2 < 500 and extent.y * 2 < 500),
                   {"extent_cm": ext, "origin_cm": str(origin)})
        check("qa_level_saved", bool(editor.save_current_level()), MAP)
    report["status"] = "PASS" if all(v["status"] == "PASS" for v in report["assertions"].values()) else "FAIL"
except Exception as e:
    report["status"] = "FAIL"
    report["errors"].append(str(e))
    report["trace"] = traceback.format_exc()
finally:
    report["finished"] = time.time()
    save()
    unreal.log("GATE12_GLB_RECOVERY_" + report["status"])
