"""UE5.8.3 read-only, one face material native probe, isolated Gate 12 project."""
import json, pathlib, traceback, time, unreal
r=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu\native_face_probe_low_ram.json")
result={"status":"STARTED","operation":"READ_ONLY_ONE_MATERIAL","time_utc":time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())}
try:
    project=str(unreal.Paths.get_project_file_path())
    if "OfflineGame_Gate12_Koikatsu_20261010" not in project:raise RuntimeError("Wrong project")
    path="/Game/Experimental/Gate12Koikatsu/FullPrimitives/Mesh03Prim00/Mesh_03_Prim_00/Materials/KK_cf_m_face_00"
    mat=unreal.load_asset(path)
    if not isinstance(mat,unreal.MaterialInstanceConstant):raise RuntimeError("Original face material instance not loaded")
    parent=mat.get_editor_property("parent")
    result["material"]=mat.get_path_name()
    result["immediate_parent"]=parent.get_path_name() if parent else None
    chain=[];visited=set();cur=parent
    while isinstance(cur,unreal.MaterialInstance):
        name=cur.get_path_name()
        if name in visited:raise RuntimeError("Material parent cycle")
        visited.add(name);chain.append(name);cur=cur.get_editor_property("parent")
    if not isinstance(cur,unreal.Material):raise RuntimeError("No real UMaterial root")
    usage=unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS
    result["base"]=cur.get_path_name()
    result["morph_flag"]=bool(unreal.MaterialEditingLibrary.has_material_usage(cur,usage))
    result["chain"]=chain
    result["editor_material_instance"]=type(mat).__name__
    result["status"]="PASS_NATIVE_COLD_LOAD_READ_ONLY"
except Exception as exc:
    result["status"]="FAIL_NATIVE_READ_ONLY";result["error"]=str(exc);result["trace"]=traceback.format_exc()[-1500:]
finally:
    r.parent.mkdir(parents=True,exist_ok=True);r.write_text(json.dumps(result,indent=2),encoding="utf-8")
    unreal.log("G12_LOW_RAM_FACE_PROBE_"+result["status"])
