"""Cold UE5.8.3 verification of saved Koikatsu playable BP and GameMode.

Read-only: no world launch, touch test, cook or package installation.
"""
import unreal,json,pathlib,hashlib,traceback,time
w=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010")
r=w/"Saved/Gate12Koikatsu"
name="/Game/Experimental/Gate12Koikatsu/PlayableQA/BP_KoikatsuFixturePlayable"
gmname="/Game/Experimental/Gate12Koikatsu/PlayableQA/BP_KoikatsuFixtureWorldMode"
native="/Script/OfflineGame.OGKoikatsuPlayableCharacter"
receipt=r/"playable_blueprint_authoring_report.json"
out={"status":"STARTED","components":[],"android_touch":"NOT_TESTED"}
try:
 expected=json.loads(receipt.read_text(encoding="utf8"))["parts"]
 assert len(expected)==19
 bp=unreal.load_asset(name)
 assert isinstance(bp,unreal.Blueprint)
 cls=unreal.EditorAssetLibrary.load_blueprint_class(name)
 base=unreal.load_class(None,native)
 par=unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp)
 assert par.get_path_name()==native,(par.get_path_name(),native)
 assert isinstance(unreal.get_default_object(cls),unreal.get_type_from_class(base))
 sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
 lib=unreal.SubobjectDataBlueprintFunctionLibrary
 verified={}
 for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
  obj=lib.get_object(sub.k2_find_subobject_data_from_handle(handle))
  if obj and obj.get_name().startswith("KK_Full_M"):
   public=obj.get_name().removesuffix("_GEN_VARIABLE")
   assert public not in verified
   assert isinstance(obj,unreal.SkeletalMeshComponent)
   mesh=obj.get_skeletal_mesh_asset()
   assert isinstance(mesh,unreal.SkeletalMesh)
   verified[public]={"name":public,"mesh":mesh.get_path_name(),
      "skeleton":mesh.get_editor_property("skeleton").get_path_name()}
 expected_map={item["component"]:item for item in expected}
 assert set(verified)==set(expected_map)
 for key,info in verified.items():
  assert info["mesh"]==expected_map[key]["mesh"]
  assert info["skeleton"]==expected_map[key]["skeleton"]
 out["components"]=list(verified.values())
 gm=unreal.load_asset(gmname)
 assert isinstance(gm,unreal.Blueprint)
 gmclass=unreal.EditorAssetLibrary.load_blueprint_class(gmname)
 default_pawn=unreal.get_default_object(gmclass).get_editor_property("default_pawn_class")
 out["game_mode_default_pawn"]=default_pawn.get_path_name()
 assert default_pawn.get_path_name()==cls.get_path_name()
 asset=w/"Content/Experimental/Gate12Koikatsu/PlayableQA/BP_KoikatsuFixturePlayable.uasset"
 out["saved_blueprint_sha256"]=hashlib.sha256(asset.read_bytes()).hexdigest()
 out["native_parent"]=par.get_path_name()
 out["status"]="PASS_COLD_NATIVE_BP_19_EXACT_PARTS_GAMEMODE"
except Exception as ex:
 out["status"]="FAIL_COLD_BP_NATIVE_BIND"
 out["error"]=str(ex)
 out["trace"]=traceback.format_exc()[-2300:]
finally:
 out["time_utc"]=time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())
 (r/"NativeActionClips/native_playable_19part_cold_unreal.json").write_text(json.dumps(out,indent=2),encoding="utf8")
 unreal.log("G12_NATIVE_PLAYER_COLD_"+out["status"])
