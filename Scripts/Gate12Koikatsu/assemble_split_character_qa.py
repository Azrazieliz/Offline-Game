import unreal,os,json,traceback
R=r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu"
MAP="/Game/Experimental/Gate12Koikatsu/CommandletQA/Maps/L_Koikatsu_SplitNative_QA"
OUT=os.path.join(R,"assemble_split_character_report.json")
o={"status":"STARTED","map":MAP,"assembled_meshes":[],"engineering_fixture_only":True}
try:
 if "OfflineGame_Gate12_Koikatsu_20261010" not in str(unreal.Paths.get_project_file_path()):raise RuntimeError("Wrong worktree")
 world=unreal.EditorLoadingAndSavingUtils.load_map(MAP)
 if not world:raise RuntimeError("Missing map")
 body=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SkeletalMeshActor)
 if len(body)!=4:raise RuntimeError("Expected 4 real split skeletal actors, found "+str(len(body)))
 for a in body:
  sk=a.skeletal_mesh_component.get_skeletal_mesh_asset()
  if not sk:raise RuntimeError("No actual skeletal mesh on "+a.get_name())
  previous=a.get_actor_location()
  a.set_actor_location(unreal.Vector(0.0,0.0,0.0),False,False)
  a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0),False)
  origin,ext=a.get_actor_bounds(False)
  o["assembled_meshes"].append({"asset":sk.get_path_name(),"label":a.get_actor_label(),"location_before":str(previous),"location_after":str(a.get_actor_location()),"bounds_height_cm":2*ext.z,"bounds_center_cm":str(origin)})
 cameras=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor)
 if len(cameras)!=1:raise RuntimeError("Expected exactly one actual camera")
 cam=cameras[0]
 position=unreal.Vector(0,190,108)
 rotate=unreal.Rotator(pitch=0,yaw=-90,roll=0)
 cam.set_actor_location(position,False,False)
 cam.set_actor_rotation(rotate,False)
 cam.set_editor_property("auto_activate_for_player",unreal.AutoReceiveInput.PLAYER0)
 o["camera"]={"position":str(cam.get_actor_location()),"rotation":str(cam.get_actor_rotation())}
 starts=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PlayerStart)
 if len(starts)!=1:raise RuntimeError("Expected exactly one PlayerStart")
 start=starts[0]
 start.set_actor_location(position,False,False)
 start.set_actor_rotation(rotate,False)
 o["player_start"]={"position":str(start.get_actor_location()),"rotation":str(start.get_actor_rotation())}
 o["saved"]=bool(unreal.EditorLoadingAndSavingUtils.save_map(world,MAP))
 if not o["saved"]:raise RuntimeError("Map saving failed")
 o["status"]="PASS"
except Exception as e:
 o["status"]="FAIL";o["error"]=str(e);o["trace"]=traceback.format_exc()
finally:
 with open(OUT,"w",encoding="utf8") as f:json.dump(o,f,indent=2,default=str)
 unreal.log("G12_ASSEMBLED_REAL_KOIKATSU_QA_"+o["status"])
