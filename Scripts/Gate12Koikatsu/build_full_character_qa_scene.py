"""Create scene from complete original-quality Koikatsu assets only.
Uses all 25 source primitives (14 visible combined actors, 5 optional hidden).
No reduced mesh/texture/morph fixtures allowed.
UE 5.8.3 Python commandlet: -run=pythonscript -script=...
"""
import unreal,json,pathlib,time,traceback
R=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu")
meta=json.loads((R/"full_fidelity_primitive_shards/manifest_full_fidelity_primitives.json").read_text(encoding="utf8"))
acceptance=json.loads((R/"full_fidelity_native_reconciled_acceptance.json").read_text(encoding="utf8"))
if acceptance["status"]!="PASS":raise RuntimeError("Full-fidelity native acceptance not passed")
MAP="/Game/Experimental/Gate12Koikatsu/FullFidelityQA/Maps/L_KK_CompleteSource_UE583"
report={"status":"STARTED","map":MAP,"source_sha256":meta["original_glb_sha256"],"actors":[],"started":time.time()}
def save():
 (R/"full_character_scene_report.json").write_text(json.dumps(report,indent=2,default=str),encoding="utf8")
save()
try:
 project=str(unreal.Paths.get_project_file_path())
 if "OfflineGame_Gate12_Koikatsu_20261010" not in project:raise RuntimeError("Wrong project!")
 assets={}
 for entry in meta["shards"]:
  mesh=int(entry["mesh_index"]);prim=int(entry["primitive_index"])
  key=(mesh,None if mesh in (4,5) else prim)
  if key in assets:continue
  folder=("/Game/Experimental/Gate12Koikatsu/FullFidelity/Mesh%02d"%mesh
          if mesh in (4,5) else
          "/Game/Experimental/Gate12Koikatsu/FullPrimitives/Mesh%02dPrim%02d"%(mesh,prim))
  sk=[]
  for path in unreal.EditorAssetLibrary.list_assets(folder,recursive=True,include_folder=False):
   a=unreal.load_asset(path)
   if isinstance(a,unreal.SkeletalMesh):sk.append(a)
  if len(sk)!=1:raise RuntimeError("Expected precisely 1 real full-quality skeletal mesh under "+folder+"; found "+str(len(sk)))
  assets[key]=sk[0]
 if len(assets)!=19:raise RuntimeError("Expected 19 unique full-quality meshes, found "+str(len(assets)))
 editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
 if not editor.new_level(MAP):raise RuntimeError("Unable to create isolated QA level")
 actorsub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 visible=0;hidden=0
 for (mesh,prim),skeletal in sorted(assets.items(),key=lambda k:(k[0][0],-1 if k[0][1] is None else k[0][1])):
  actor=actorsub.spawn_actor_from_class(unreal.SkeletalMeshActor,
      unreal.Vector(0,0,0),unreal.Rotator(pitch=0,yaw=0,roll=0))
  if not actor:raise RuntimeError("Could not spawn skeletal mesh")
  actor.set_actor_label("G12_FullKK_Mesh%02d"%mesh+("Whole" if prim is None else "_Prim%02d"%prim))
  actor.skeletal_mesh_component.set_skeletal_mesh_asset(skeletal)
  if mesh in (0,1,2):actor.set_actor_hidden_in_game(True);hidden+=1
  else:visible+=1
  origin,extent=actor.get_actor_bounds(False)
  skeleton=skeletal.get_editor_property("skeleton")
  report["actors"].append({"label":actor.get_actor_label(),"source_mesh_index":mesh,"source_primitive_index":prim,
       "full_quality_asset":skeletal.get_path_name(),"hidden_optional_effect":mesh in (0,1,2),
       "bones":len(skeleton.get_editor_property("bone_tree")),"morphs":len(skeletal.get_editor_property("morph_targets")),
       "material_slots":len(skeletal.get_editor_property("materials")),
       "world_bbox_height_cm":extent.z*2})
 camera_position=unreal.Vector(0,235,100)
 camera_rotation=unreal.Rotator(pitch=0,yaw=-90,roll=0)
 camera=actorsub.spawn_actor_from_class(unreal.CameraActor,camera_position,camera_rotation)
 camera.set_actor_label("G12_Full_Fidelity_Camera")
 camera.set_editor_property("auto_activate_for_player",unreal.AutoReceiveInput.PLAYER0)
 start=actorsub.spawn_actor_from_class(unreal.PlayerStart,camera_position,camera_rotation)
 start.set_actor_label("G12_Full_Fidelity_PlayerStart")
 light=actorsub.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,400),unreal.Rotator(pitch=-42,yaw=28,roll=0))
 light.set_actor_label("G12_Neutral_Key_DirectionalLight")
 for i,pos in enumerate((unreal.Vector(0,150,270),unreal.Vector(0,-170,170))):
  l=actorsub.spawn_actor_from_class(unreal.PointLight,pos,unreal.Rotator(pitch=0,yaw=0,roll=0))
  l.set_actor_label("G12_Neutral_Character_Light_"+str(i))
 report["camera"]={"position":str(camera.get_actor_location()),"rotation":str(camera.get_actor_rotation())}
 report["visible_real_meshes"]=visible;report["hidden_optional_meshes"]=hidden
 report["save_success"]=bool(unreal.EditorLoadingAndSavingUtils.save_map(unreal.EditorLevelLibrary.get_editor_world(),MAP))
 # save_map of a just-loaded map can be risky if new_level world was not yet serialized.
 if not report["save_success"]:
  report["save_success"]=bool(editor.save_current_level())
 if not report["save_success"]:raise RuntimeError("Unable to serialize full-fidelity scene")
 report["status"]="PASS"
except Exception as e:
 report["status"]="FAIL";report["error"]=str(e);report["trace"]=traceback.format_exc()[-2200:]
finally:
 report["finished"]=time.time();save();unreal.log("G12_FULL_CHARACTER_SCENE_"+report["status"])
