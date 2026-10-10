"""UE 5.8.3 native material usage fix for skeletal meshes with genuine imported morph targets.
No geometry, texture pixels, morph weights, or metadata is discarded.
"""
import unreal,json,pathlib,traceback,time
R=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu")
meta=json.loads((R/"facial_stress_authoring_report.json").read_text(encoding="utf8"))
out={"status":"STARTED","processed":[],"fixed":[]}
def write(): (R/"morph_material_usage_fix_report.json").write_text(json.dumps(out,indent=2,default=str),encoding="utf8")
write()
try:
 if "OfflineGame_Gate12_Koikatsu_20261010" not in str(unreal.Paths.get_project_file_path()):raise RuntimeError("Not isolated UE test project")
 if meta["status"]!="PASS" or len(meta["sequences"])!=19:raise RuntimeError("Stress QA not ready")
 flag=unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS
 seen={}
 for part in meta["sequences"]:
  mesh=unreal.load_asset(part["mesh"])
  if not isinstance(mesh,unreal.SkeletalMesh):raise RuntimeError("Missing genuine imported skeletal mesh")
  morphs=mesh.get_editor_property("morph_targets")
  if not morphs:continue
  for slot in mesh.get_editor_property("materials"):
   material_interface=slot.get_editor_property("material_interface")
   if not material_interface:raise RuntimeError("Missing material in morphed mesh "+part["mesh"])
   path=material_interface.get_path_name()
   if path in seen:continue
   seen[path]=True
   mat=material_interface
   if isinstance(mat,unreal.MaterialInstance):
    mat=mat.get_base_material()
   if not isinstance(mat,unreal.Material):raise RuntimeError("Unsupported morph material "+path)
   before=bool(unreal.MaterialEditingLibrary.has_material_usage(mat,flag))
   item={"slot_path":path,"native_material":mat.get_path_name(),"morphs":len(morphs),"before":before}
   out["processed"].append(item);write()
   if not before:
    unreal.MaterialEditingLibrary.set_base_material_usage(mat,flag,True)
    if not bool(unreal.MaterialEditingLibrary.has_material_usage(mat,flag)):
     raise RuntimeError("Material morph-usage flag did not persist in memory "+path)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat):
     raise RuntimeError("Unable to save updated material "+path)
    out["fixed"].append(path)
   item["after"]=bool(unreal.MaterialEditingLibrary.has_material_usage(mat,flag))
   if not item["after"]:raise RuntimeError("Morph usage still false "+path)
   write()
 out["total_materials_with_morphed_meshes"]=len(out["processed"])
 out["status"]="PASS"
except Exception as e:out["status"]="FAIL";out["error"]=str(e);out["trace"]=traceback.format_exc()[-1700:]
finally:
 out["finished_utc"]=time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime());write()
 unreal.log("GATE12_MORPH_MATERIAL_USAGE_"+out["status"])
