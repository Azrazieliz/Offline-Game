"""Native Unreal editor script; set required morph-target shader usage, no texture resizing."""
import unreal,os,pathlib,json,traceback
if "KK_BLUEPRINT_DESCRIPTOR" not in os.environ:
 raise RuntimeError("KK_BLUEPRINT_DESCRIPTOR must point to validated native descriptor")
D=pathlib.Path(os.environ["KK_BLUEPRINT_DESCRIPTOR"]).resolve(strict=True)
m=json.loads(D.read_text(encoding="utf8"))
r={"status":"STARTED","character_id":m.get("character_id"),"materials":[]}
out=D.parent/(m["character_id"].replace("-","_")+"_native_morph_shader_audit.json")
def save():out.write_text(json.dumps(r,indent=2,default=str),encoding="utf8")
save()
try:
 if m.get("status")!="PASS" or m.get("schema_version")!="KK_UNREAL_CHARACTER_DESCRIPTOR_1":
  raise RuntimeError("Native descriptor not accepted")
 if "OfflineGame_Gate12_Koikatsu_20261010" not in str(unreal.Paths.get_project_file_path()):
  raise RuntimeError("Wrong isolated project")
 seen=set();flag=unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS
 for part in m["parts"]:
  mesh=unreal.load_asset(part["mesh"])
  if not isinstance(mesh,unreal.SkeletalMesh):raise RuntimeError("Missing actual native mesh")
  if mesh.get_editor_property("skeleton").get_path_name()!=part["skeleton"]:
   raise RuntimeError("Skeleton mismatch")
  if not mesh.get_editor_property("morph_targets"):continue
  for slot in mesh.get_editor_property("materials"):
   interface=slot.get_editor_property("material_interface")
   if not interface:raise RuntimeError("Missing material in morph-driven component")
   mat=interface.get_base_material() if isinstance(interface,unreal.MaterialInstance) else interface
   path=mat.get_path_name()
   if path in seen:continue
   seen.add(path)
   if not isinstance(mat,unreal.Material):raise RuntimeError("Unsupported morph material")
   before=bool(unreal.MaterialEditingLibrary.has_material_usage(mat,flag))
   if not before:
    unreal.MaterialEditingLibrary.set_base_material_usage(mat,flag,True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat):
     raise RuntimeError("Material save unsuccessful: "+path)
   after=bool(unreal.MaterialEditingLibrary.has_material_usage(mat,flag))
   if not after:raise RuntimeError("Morph flag not enabled: "+path)
   r["materials"].append({"path":path,"before":before,"after":after})
   save()
 r["status"]="PASS_NATIVE_SAVED_REQUIRES_ANDROID_RECOOK"
except Exception as e:r["status"]="FAIL";r["error"]=str(e);r["trace"]=traceback.format_exc()[-1200:]
finally:
 save();unreal.log("G12_KK_GENERIC_MORPH_USAGE_"+r["status"])
