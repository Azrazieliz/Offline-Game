"""Gate12 create actual UE AOGWorldPrototypeCharacter-derived playable character + GameMode test assets.
Engine 5.8.3, isolated test content, full source mesh components, no native rebuild.
Do not claim animation-state switching: only actual movement pawn with native looping idle.
"""
import unreal,json,pathlib,traceback,time
R=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu")
BASE=json.loads((R/"rest_pose_sway_authored_report.json").read_text(encoding="utf8"))
DIR="/Game/Experimental/Gate12Koikatsu/PlayableQA"
PLAYER=DIR+"/BP_KoikatsuFixturePlayable"
GM=DIR+"/BP_KoikatsuFixtureWorldMode"
WORLD=DIR+"/Maps/L_KoikatsuPlayable_UE583"
SOURCE_MAP="/Game/Maps/StartingWorld"
o={"status":"STARTED","player":PLAYER,"game_mode":GM,"world":WORLD,"parts":[],"run_type":"ENGINEERING_FIXTURE"}
def save(): (R/"playable_blueprint_authoring_report.json").write_text(json.dumps(o,indent=2,default=str),encoding="utf8")
save()
try:
 if BASE["status"]!="PASS" or len(BASE["parts"])!=19:raise RuntimeError("Missing already-verified original native skeleton data")
 if "OfflineGame_Gate12_Koikatsu_20261010" not in str(unreal.Paths.get_project_file_path()):raise RuntimeError("Wrong project")
 if any(unreal.EditorAssetLibrary.does_asset_exist(x) for x in (PLAYER,GM,WORLD)):
  raise RuntimeError("Refusing to overwrite previous playable assets")
 char=unreal.load_class(None,"/Script/OfflineGame.OGWorldPrototypeCharacter")
 game=unreal.load_class(None,"/Script/OfflineGame.OGWorldPresentationGameMode")
 if not char or not game:raise RuntimeError("Frozen project's real movement classes unavailable")
 o["native_player_parent"]=str(char);o["native_gm_parent"]=str(game);save()
 bp=unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(PLAYER,char)
 if not isinstance(bp,unreal.Blueprint):raise RuntimeError("Could not create player blueprint")
 subsystem=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
 handles=subsystem.k2_gather_subobject_data_for_blueprint(bp)
 if not handles:raise RuntimeError("Blueprint component root missing")
 lib=unreal.SubobjectDataBlueprintFunctionLibrary
 root=handles[0]
 o["inherited_subobjects"]=[]
 for handle in handles:
  try:
   data=subsystem.k2_find_subobject_data_from_handle(handle)
   obj=lib.get_object(data)
   if obj:
    o["inherited_subobjects"].append({"name":obj.get_name(),"type":obj.get_class().get_name()})
    if obj.get_name().lower() in ("mesh","character_mesh0","prototypebody"):
     # Hide the frozen diagnostic presentation only in this test subclass; keep its anim for internal logic.
     if isinstance(obj,unreal.SkeletalMeshComponent):
      obj.set_editor_property("hidden_in_game",True)
      o["diagnostic_proxy_hidden"]=True
  except Exception as ex:o.setdefault("inherited_probe_warnings",[]).append(str(ex)[:100])
 save()
 for part in BASE["parts"]:
  mesh=unreal.load_asset(part["mesh"])
  clip=unreal.load_asset(part["sequence"])
  if not isinstance(mesh,unreal.SkeletalMesh) or not isinstance(clip,unreal.AnimSequence):
   raise RuntimeError("Real source mesh / exact-skeleton clip absent")
  if mesh.get_editor_property("skeleton").get_path_name()!=clip.get_editor_property("skeleton").get_path_name():
   raise RuntimeError("Refuse cross-skeleton animation")
  label="KK_Full_M%02dP%02d"%(part["mesh_index"],part["primitive_index"])
  new,why=subsystem.add_new_subobject(params=unreal.AddNewSubobjectParams(
    parent_handle=root,new_class=unreal.SkeletalMeshComponent,blueprint_context=bp))
  if not new:raise RuntimeError("Cannot add native skeletal mesh component "+label+":"+str(why))
  subsystem.rename_subobject(handle=new,new_name=unreal.Text(label))
  data=subsystem.k2_find_subobject_data_from_handle(new)
  comp=lib.get_object(data)
  if not isinstance(comp,unreal.SkeletalMeshComponent):raise RuntimeError("Subobject template not skeletal "+label)
  comp.set_skeletal_mesh_asset(mesh)
  # Capsule center from frozen native pawn is z96; imported GLB mesh origin stays ground zero.
  comp.set_editor_property("relative_location",unreal.Vector(0,0,-96))
  comp.set_editor_property("relative_rotation",unreal.Rotator(pitch=0,yaw=0,roll=0))
  comp.set_editor_property("relative_scale3d",unreal.Vector(1,1,1))
  comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
  comp.set_editor_property("cast_shadow",True)
  comp.override_animation_data(anim_to_play=clip,is_looping=True,is_playing=True,position=0,play_rate=1)
  if part["mesh_index"] in (0,1,2):comp.set_editor_property("hidden_in_game",True)
  o["parts"].append({"component":label,"mesh":mesh.get_path_name(),"skeleton":part["skeleton"],
    "idle_clip":clip.get_path_name(),"source_asset_fidelity":"FULL","disabled_optional_effect":part["mesh_index"] in (0,1,2)})
  save()
 if len(o["parts"])!=19:raise RuntimeError("Incomplete blueprint")
 if not unreal.BlueprintEditorLibrary.compile_blueprint(bp):
  raise RuntimeError("Player Blueprint compilation failed")
 if not unreal.EditorAssetLibrary.save_loaded_asset(bp):raise RuntimeError("Could not save player BP")
 o["player_blueprint_saved"]=True;save()
 gm=unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(GM,game)
 if not isinstance(gm,unreal.Blueprint):raise RuntimeError("Game mode Blueprint missing")
 player_cls=unreal.EditorAssetLibrary.load_blueprint_class(PLAYER)
 gm_cls=unreal.EditorAssetLibrary.load_blueprint_class(GM)
 if not player_cls or not gm_cls:raise RuntimeError("Cannot load generated BP class")
 gm_cdo=unreal.get_default_object(gm_cls)
 gm_cdo.set_editor_property("default_pawn_class",player_cls)
 if not unreal.BlueprintEditorLibrary.compile_blueprint(gm):raise RuntimeError("GameMode blueprint compile failed")
 if not unreal.EditorAssetLibrary.save_loaded_asset(gm):raise RuntimeError("GameMode save failed")
 o["default_pawn_class"]=str(gm_cdo.get_editor_property("default_pawn_class"));save()
 if not unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP,WORLD):
  raise RuntimeError("Could not clone frozen world into isolated QA level")
 world=unreal.EditorLoadingAndSavingUtils.load_map(WORLD)
 settings=world.get_world_settings()
 settings.set_editor_property("default_game_mode",gm_cls)
 if not unreal.EditorLoadingAndSavingUtils.save_map(world,WORLD):
  raise RuntimeError("QA map did not save")
 o["actual_playable_map_saved"]=True
 o["status"]="PASS_BLUEPRINT_ASSEMBLED_PENDING_COOK_AND_ANDROID"
except Exception as e:
 o["status"]="FAIL";o["error"]=str(e);o["trace"]=traceback.format_exc()[-2500:]
finally:
 o["utc"]=time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime());save()
 unreal.log("G12_GAMEPLAY_BLUEPRINT_"+o["status"])
