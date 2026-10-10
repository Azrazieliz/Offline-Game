"""Non-destructive full-quality UE map binds exact per-skeleton rest-safe clips."""
import unreal,json,pathlib,traceback
R=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu")
BASE="/Game/Experimental/Gate12Koikatsu/FullFidelityQA/Maps/L_KK_CompleteSource_UE583"
DEST="/Game/Experimental/Gate12Koikatsu/FullFidelityQA/Maps/L_KK_NativeRestSafeSway_UE583"
OUT="/Game/Experimental/Gate12Koikatsu/Animation/NativeRestSafe"
created=json.loads((R/"rest_pose_sway_authored_report.json").read_text(encoding="utf8"))
r={"status":"STARTED","map":DEST,"source_map":BASE,"bound":[]}
def save():
 (R/"rest_safe_animated_map_report.json").write_text(json.dumps(r,indent=2,default=str),encoding="utf8")
save()
try:
 if created["status"]!="PASS" or len(created["parts"])!=19:raise RuntimeError("Native clips not complete")
 if "OfflineGame_Gate12_Koikatsu_20261010" not in str(unreal.Paths.get_project_file_path()):raise RuntimeError("Not isolated project")
 if unreal.EditorAssetLibrary.does_asset_exist(DEST):raise RuntimeError("Map already exists; refusing overwrite")
 if not unreal.EditorAssetLibrary.duplicate_asset(BASE,DEST):raise RuntimeError("Could not duplicate original static map")
 world=unreal.EditorLoadingAndSavingUtils.load_map(DEST)
 if not world:raise RuntimeError("Failed loading new map")
 actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SkeletalMeshActor)
 if len(actors)!=19:raise RuntimeError("Expected exactly 19 real full-quality skeletal actors")
 candidates={str(x["mesh"]):x for x in created["parts"]}
 for actor in actors:
  comp=actor.skeletal_mesh_component
  mesh=comp.get_skeletal_mesh_asset()
  if not mesh or mesh.get_path_name() not in candidates:raise RuntimeError("Unexpected actor "+actor.get_name())
  record=candidates[mesh.get_path_name()]
  seq=unreal.load_asset(record["sequence"])
  if not isinstance(seq,unreal.AnimSequence):raise RuntimeError("Missing native sequence "+record["sequence"])
  sk=mesh.get_editor_property("skeleton");target=seq.get_editor_property("skeleton")
  if sk.get_path_name()!=target.get_path_name():raise RuntimeError("Exact native skeleton mismatch")
  comp.override_animation_data(anim_to_play=seq,is_looping=True,is_playing=True,position=0.0,play_rate=1.0)
  if comp.get_animation_mode()!=unreal.AnimationMode.ANIMATION_SINGLE_NODE:raise RuntimeError("Invalid animation mode")
  r["bound"].append({"actor":actor.get_actor_label(),"mesh":mesh.get_path_name(),"sequence":seq.get_path_name(),
                     "skeleton":sk.get_path_name(),"reference_pose_preserved":record["root_animation_tracks"]==0,
                     "tracks":record["tracks"],"full_resolution_material_slots":len(mesh.get_editor_property("materials"))})
  save()
 if len(r["bound"])!=19 or not all(x["reference_pose_preserved"] for x in r["bound"]):raise RuntimeError("Incomplete bindings")
 r["save_result"]=bool(unreal.EditorLoadingAndSavingUtils.save_map(world,DEST))
 if not r["save_result"]:raise RuntimeError("Map save returned false")
 r["status"]="PASS"
except Exception as e:
 r["status"]="FAIL";r["error"]=str(e);r["trace"]=traceback.format_exc()[-1600:]
finally:save();unreal.log("G12_NATIVE_RESTSAFE_MAP_"+r["status"])
