"""Finalize existing actual playable Blueprint: hide only native tutorial proxy; no source detail reduction."""
import unreal,pathlib,json,traceback
R=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu")
BP="/Game/Experimental/Gate12Koikatsu/PlayableQA/BP_KoikatsuFixturePlayable"
o={"status":"STARTED","bp":BP}
try:
 if "OfflineGame_Gate12_Koikatsu_20261010" not in str(unreal.Paths.get_project_file_path()):raise RuntimeError("Wrong project")
 bp=unreal.load_asset(BP)
 if not isinstance(bp,unreal.Blueprint):raise RuntimeError("No native playable BP")
 s=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
 handles=s.k2_gather_subobject_data_for_blueprint(bp)
 lib=unreal.SubobjectDataBlueprintFunctionLibrary
 names=[];hidden=0;parts=0
 for h in handles:
  data=s.k2_find_subobject_data_from_handle(h);obj=lib.get_object(data)
  if not obj:continue
  n=obj.get_name();names.append(n)
  if n=="CharacterMesh0":
   if not isinstance(obj,unreal.SkeletalMeshComponent):raise RuntimeError("Prototype mesh not skeletal")
   obj.set_editor_property("hidden_in_game",True)
   hidden+=1
  if n.startswith("KK_Full_M"):parts+=1
 o["native_prototype_hidden"]=hidden;o["imported_part_count"]=parts;o["names"]=names
 if hidden!=1 or parts!=19:raise RuntimeError("Expected 1 diagnostic body and 19 full imports")
 if not unreal.BlueprintEditorLibrary.compile_blueprint(bp):raise RuntimeError("Blueprint compilation failed")
 if not unreal.EditorAssetLibrary.save_loaded_asset(bp):raise RuntimeError("Blueprint save failed")
 o["status"]="PASS_BLUEPRINT_COMPONENTS_SAVED"
except Exception as e:o["status"]="FAIL";o["error"]=str(e);o["trace"]=traceback.format_exc()[-1000:]
finally:
 (R/"playable_blueprint_visual_finalize_report.json").write_text(json.dumps(o,indent=2),encoding="utf8")
 unreal.log("G12_KK_PLAYABLE_VISUALS_"+o["status"])
