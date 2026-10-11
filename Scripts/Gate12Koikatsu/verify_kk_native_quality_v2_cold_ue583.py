"""Independent NEW UE5.8.3 process verification: 19 exact rigs x 4 V2 clips.

Not animation-quality approval or phone/render validation. Read-only test.
"""
import json, os, pathlib, re, sys, time, traceback
import unreal
ROOT=pathlib.Path(os.environ.get("G12_GATE12_WORKTREE",r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010"))
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
from kk_native_motion_v2_profiles import ACTIONS,ROOT_NAMES,validate
REPORT=ROOT/"Saved/Gate12Koikatsu/NativeMotionV2/quality_v2_cold_unreal.json"
BLUEPRINT=ROOT/"Saved/Gate12Koikatsu/playable_blueprint_authoring_report.json"
ASSET_DIR="/Game/Experimental/Gate12Koikatsu/Animation/NativeQualityV2"
result={"status":"STARTED","time_utc":time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime()),"total_expected":76,
        "parts":[],"cold_unreal_process":True,"phone_validated":False,"final_animation_approved":False}
def write():
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(result,indent=2,default=str)+"\n",encoding="utf8")
def must(test,msg):
    if not test:raise RuntimeError(msg)
write()
try:
    must("OfflineGame_Gate12_Koikatsu_20261010" in str(unreal.Paths.get_project_file_path()),"Wrong UE project")
    must(validate()["status"]=="PASS_STATIC_V2_BOUNDED_LOOP_SEAMS","Loop-bound statics failed")
    base=json.loads(BLUEPRINT.read_text(encoding="utf-8-sig"))
    must(base["status"]=="PASS_BLUEPRINT_ASSEMBLED_PENDING_COOK_AND_ANDROID","Reference BP not validated")
    must(len(base["parts"])==19,"Expected exactly 19 saved native BP parts")
    seen=set()
    for part in base["parts"]:
        name=part["component"]
        x=re.search(r"M(\d+)P(\d+)$",name)
        must(x is not None,"Bad physical primitive identity: "+name)
        m,p=(int(z) for z in x.groups())
        mesh=unreal.load_asset(part["mesh"])
        must(isinstance(mesh,unreal.SkeletalMesh),"Missing exact saved mesh")
        skeleton=mesh.get_editor_property("skeleton")
        must(skeleton.get_path_name()==part["skeleton"],"Saved source skeleton mismatch")
        ref=skeleton.get_reference_pose()
        bone_names=set(str(b) for b in ref.get_bone_names())
        batch={"name":name,"skeleton":part["skeleton"],"mesh":part["mesh"],"clips":[]}
        for a in ACTIONS:
            k=f"AN_KK_V2_{a.name}_M{m:02d}_P{p:02d}"
            path=f"{ASSET_DIR}/{k}"
            must(path not in seen,"Duplicate v2 source object: "+path)
            seen.add(path)
            asset=unreal.load_asset(path)
            must(isinstance(asset,unreal.AnimSequence),"Missing native UE AnimSequence: "+path)
            must(asset.get_editor_property("skeleton").get_path_name()==part["skeleton"],
                 "Foreign skeleton in action: "+path)
            duration=asset.get_play_length()
            must(abs(duration-a.frames/30)<0.07,"Wrong clip duration: "+path)
            controller=asset.get_editor_property("controller")
            tracks=set(str(q) for q in controller.get_model_interface().get_bone_track_names())
            must(len(tracks)>=6 and not tracks.intersection(ROOT_NAMES),"Missing motion or root displacement: "+path)
            must(tracks.issubset(bone_names),"Clip references unknown source bone: "+path)
            batch["clips"].append({"state":a.name,"asset":path,
                                   "seconds":duration,"tracks":len(tracks),
                                   "no_root_tracks":True,"exact_skeleton":True})
        result["parts"].append(batch)
        write()
    must(len(seen)==76 and len(result["parts"])==19,"Incomplete cold loaded v2 set")
    result["native_animation_asset_count"]=len(seen)
    result["status"]="PASS_COLD_UNREAL_19_PARTS_76_EXACT_SKELETON_V2_MOTION_PRE_ANDROID"
except Exception as e:
    result["status"]="FAIL_COLD_UNREAL_V2_MOTION"
    result["error"]=str(e)
    result["traceback"]=traceback.format_exc()[-2700:]
finally:
    write()
    unreal.log("GATE12_QUALITY_V2_COLD_RESULT="+result["status"])
