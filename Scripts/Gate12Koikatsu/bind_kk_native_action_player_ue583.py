"""Cold-editor native reparent of existing Koikatsu playable QA Blueprint.

Never duplicates the 19 full-resolution mesh components, GameMode, or starting
world. Requires actual compiled /Script/OfflineGame native class. One isolated
experimental fixture only, with on-disk backup and proof after saving.
"""
import hashlib
import json
import pathlib
import shutil
import time
import traceback
import unreal

ROOT=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010")
R=ROOT/"Saved/Gate12Koikatsu"
P="/Game/Experimental/Gate12Koikatsu/PlayableQA/BP_KoikatsuFixturePlayable"
NATIVE="/Script/OfflineGame.OGKoikatsuPlayableCharacter"
PLAYER_FILE=ROOT/"Content/Experimental/Gate12Koikatsu/PlayableQA/BP_KoikatsuFixturePlayable.uasset"
REPORT=R/"native_action_blueprint_bind_report.json"
result={"status":"STARTED","player_bp":P,"new_native_parent":NATIVE,
        "component_count":None,"android_playability":"NOT_TESTED"}
def save_report():
    REPORT.write_text(json.dumps(result,indent=2,default=str)+"\n",encoding="utf-8")
def require(v,msg):
    if not v:raise RuntimeError(msg)
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

save_report()
try:
    require("OfflineGame_Gate12_Koikatsu_20261010" in str(unreal.Paths.get_project_file_path()),
            "Wrong experimental project")
    native=unreal.load_class(None,NATIVE)
    require(native is not None,"Not compiled: offline-game native subclass missing")
    bp=unreal.load_asset(P)
    require(isinstance(bp,unreal.Blueprint),"Existing playable Blueprint missing")
    parent=bp.get_editor_property("parent_class")
    require(parent is not None,"Blueprint has no parent")
    result["previous_parent"]=parent.get_path_name()
    require(parent.get_path_name() in (NATIVE,
            "/Script/OfflineGame.OGWorldPrototypeCharacter"),
            "Unexpected creative/production BP superclass: "+parent.get_path_name())
    subsystem=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib=unreal.SubobjectDataBlueprintFunctionLibrary
    def components():
        rows=[]
        for h in subsystem.k2_gather_subobject_data_for_blueprint(bp):
            obj=lib.get_object(subsystem.k2_find_subobject_data_from_handle(h))
            if obj and obj.get_name().startswith("KK_Full_M"):
                rows.append((obj.get_name(),obj))
        return rows
    before=components()
    expected=json.loads((R/"playable_blueprint_authoring_report.json").read_text(encoding="utf-8"))["parts"]
    expected_set={x["component"] for x in expected}
    require(len(before)==len(expected)==19,"Expected 19 existing UE Blueprint mesh components")
    require(set(n for n,_ in before)==expected_set,"Unexpected Blueprint component identifiers")
    require(PLAYER_FILE.is_file(),"Missing native serialized Blueprint file")
    old_hash=sha(PLAYER_FILE)
    result["before_sha256"]=old_hash
    backup=R/"NativeActionClips/BackupBeforeNativeReparent"
    backup.mkdir(parents=True,exist_ok=True)
    destination=backup/(old_hash[:16]+"_BP_KoikatsuFixturePlayable.uasset")
    if not destination.exists():shutil.copy2(PLAYER_FILE,destination)
    require(sha(destination)==old_hash,"Backup hash mismatch")
    result["backup_file"]=str(destination)
    if parent.get_path_name()!=NATIVE:
        unreal.BlueprintEditorLibrary.reparent_blueprint(bp,native)
    reparented=bp.get_editor_property("parent_class")
    require(reparented and reparented.get_path_name()==NATIVE,"Native reparent failed")
    after=components()
    require(len(after)==19 and set(n for n,_ in after)==expected_set,
            "Imported Blueprint mesh components lost during reparent")
    for name,part in after:
        require(isinstance(part,unreal.SkeletalMeshComponent),
                "Unexpected non-skeletal component: "+name)
        require(part.get_skeletal_mesh_asset() is not None,
                "Source native mesh lost: "+name)
    require(bool(unreal.BlueprintEditorLibrary.compile_blueprint(bp)),"Native-reparented BP did not compile")
    require(unreal.EditorAssetLibrary.save_loaded_asset(bp),"Blueprint failed to save")
    result["after_sha256"]=sha(PLAYER_FILE)
    result["component_count"]=len(after)
    result["parent_persisted_in_current_editor"]=True
    result["status"]="PASS_BP_REPARENT_SERIALIZED_REQUIRES_COLD_RELOAD_ANDROID"
except Exception as exc:
    result["status"]="FAIL_NATIVE_BP_BIND"
    result["error"]=str(exc)
    result["trace"]=traceback.format_exc()[-1900:]
finally:
    result["time_utc"]=time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())
    save_report()
    unreal.log("G12_NATIVE_BP_REPARENT_"+result["status"])
