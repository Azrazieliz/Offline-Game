import unreal,os,json,traceback
R=r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu"
MAP="/Game/Experimental/Gate12Koikatsu/CommandletQA/Maps/L_Koikatsu_SplitNative_QA"
OUT=os.path.join(R,"repair_actual_scene_camera_report.json")
o={"status":"STARTED","map":MAP,"actions":[]}
try:
 project=str(unreal.Paths.get_project_file_path())
 if "OfflineGame_Gate12_Koikatsu_20261010" not in project:raise RuntimeError("Not isolated worktree!")
 world=unreal.EditorLoadingAndSavingUtils.load_map(MAP)
 if world is None:raise RuntimeError("Map not loaded")
 cams=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor)
 if len(cams)!=1:raise RuntimeError("Expected 1 real test camera, got "+str(len(cams)))
 camera=cams[0]
 o["camera_before"]={"position":str(camera.get_actor_location()),"rotation":str(camera.get_actor_rotation())}
 position=unreal.Vector(0.0,330.0,115.0)
 rotation=unreal.Rotator(pitch=0.0,yaw=-90.0,roll=0.0)
 camera.set_actor_location(position,False,False)
 camera.set_actor_rotation(rotation,False)
 camera.set_editor_property("auto_activate_for_player",unreal.AutoReceiveInput.PLAYER0)
 o["camera_after"]={"position":str(camera.get_actor_location()),"rotation":str(camera.get_actor_rotation()),"activation":str(camera.get_editor_property("auto_activate_for_player"))}
 sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 starts=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PlayerStart)
 if len(starts)>1:raise RuntimeError("Multiple pre-existing PlayerStarts: refusing to mutate")
 if starts:
  spawn=starts[0]
  o["player_start"]="updated_existing"
 else:
  spawn=sub.spawn_actor_from_class(unreal.PlayerStart,position,rotation)
  if not spawn:raise RuntimeError("Unable to create PlayerStart")
  spawn.set_actor_label("Gate12_Koikatsu_QA_PlayerStart")
  o["player_start"]="created"
 spawn.set_actor_location(position,False,False)
 spawn.set_actor_rotation(rotation,False)
 o["player_start_properties"]={"position":str(spawn.get_actor_location()),"rotation":str(spawn.get_actor_rotation())}
 # Fix original light positional Unreal.Rotator error and ensure one usable key light.
 lights=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DirectionalLight)
 for light in lights:
  light.set_actor_rotation(unreal.Rotator(pitch=-50.0,yaw=25.0,roll=0.0),False)
  o["actions"].append({"light":light.get_name(),"rotation":str(light.get_actor_rotation())})
 # A neutral white frontal point light illuminates the existing model;
 # this is not a substitute visual or diagnostic model.
 pts=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PointLight)
 if not pts:
  l=sub.spawn_actor_from_class(unreal.PointLight,unreal.Vector(0,180,260),unreal.Rotator(pitch=0,yaw=0,roll=0))
  if l:
   l.set_actor_label("Gate12_Koikatsu_Front_KeyLight")
   pts=[l]
 o["point_lights"]=len(pts)
 saved=unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
 o["saved"]=bool(saved)
 if not saved:raise RuntimeError("save_map returned false")
 actual=camera.get_actor_rotation()
 o["rotation_values"]={"pitch":actual.pitch,"yaw":actual.yaw,"roll":actual.roll}
 o["camera_rotation_ok"]=abs(actual.pitch)<0.2 and abs(actual.yaw+90)<0.2 and abs(actual.roll)<0.2
 if not o["camera_rotation_ok"]:raise RuntimeError("Rotator not corrected: "+str(actual))
 o["status"]="PASS"
except Exception as e:
 o["status"]="FAIL";o["error"]=str(e);o["trace"]=traceback.format_exc()
finally:
 with open(OUT,"w",encoding="utf8") as f:json.dump(o,f,indent=2,default=str)
 unreal.log("G12_REPAIRED_CAMERA_"+o["status"])
