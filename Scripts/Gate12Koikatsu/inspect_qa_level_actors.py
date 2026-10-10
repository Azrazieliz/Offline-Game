import unreal,os,json,traceback
R=r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu"
MAP="/Game/Experimental/Gate12Koikatsu/CommandletQA/Maps/L_Koikatsu_SplitNative_QA"
res={"project":str(unreal.Paths.get_project_file_path()),"map":MAP,"status":"STARTED","actors":[]}
try:
 world=unreal.EditorLoadingAndSavingUtils.load_map(MAP)
 if not world:raise Exception("map not loaded")
 res["world"]=world.get_path_name()
 actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
 for a in actors:
  c=a.get_class().get_name()
  if any(x in c for x in ("SkeletalMeshActor","CameraActor","PlayerStart","Light","WorldSettings","DefaultPawn")):
   v={"class":c,"name":a.get_name(),"label":a.get_actor_label(),"location":str(a.get_actor_location()),"rotation":str(a.get_actor_rotation()),"scale":str(a.get_actor_scale3d())}
   if isinstance(a,unreal.CameraActor):
    for prop in ("auto_activate_for_player",):
     try:v[prop]=str(a.get_editor_property(prop))
     except Exception as e:v[prop+"_err"]=str(e)
    try:v["fov"]=a.get_camera_component().field_of_view
    except Exception as e:v["fov_err"]=str(e)
   if isinstance(a,unreal.SkeletalMeshActor):
    comp=a.skeletal_mesh_component
    sk=comp.get_skeletal_mesh_asset()
    v["asset"]=sk.get_path_name() if sk else None
    try:
     center,ext=a.get_actor_bounds(False)
     v["bounds_center"]=str(center);v["bounds_extent"]=str(ext)
    except Exception as e:v["bounds_error"]=str(e)
   res["actors"].append(v)
 res["total_actors"]=len(actors)
 res["status"]="COMPLETE"
except Exception as e:
 res["status"]="ERROR";res["error"]=str(e);res["trace"]=traceback.format_exc()[-1600:]
finally:
 with open(os.path.join(R,"inspect_qa_level_actors_report.json"),"w",encoding="utf8") as f:json.dump(res,f,indent=2,default=str)
 unreal.log("G12_CAMERA_LEVEL_INSPECT_"+res["status"])
