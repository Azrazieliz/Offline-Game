"""UE5.8.3 post-freeze v2 provisional gait and swim author: create ONE native part's genuine distinct in-place action clips.

Consumes existing 19-part blueprint receipt, not a recopy/import of source GLB.
The source rig reference pose is the sole rest pose; zero root tracks or offsets.
G12_KK_PART_INDEX=0..18, G12_KK_ACTION=Walk|Run|...|ALL.
Run batches sequentially with independent live RAM/commit monitoring.
"""
import json
import math
import os
import pathlib
import sys
import time
import traceback
import unreal

SCRIPT_DIR=pathlib.Path(__file__).resolve().parent
sys.path.insert(0,str(SCRIPT_DIR))
from kk_native_motion_v2_profiles import ACTIONS, AXES, ROOT_NAMES, action, angles_degrees, validate
ROOT=pathlib.Path(os.environ.get("G12_GATE12_WORKTREE",
                r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010"))
REPORT_DIR=ROOT/"Saved/Gate12Koikatsu/NativeMotionV2/NativeActionClips"
BLUPRINT=ROOT/"Saved/Gate12Koikatsu/playable_blueprint_authoring_report.json"
OUT="/Game/Experimental/Gate12Koikatsu/Animation/NativeQualityV2"
index=int(os.environ.get("G12_KK_PART_INDEX","0"))
requested=os.environ.get("G12_KK_ACTION","ALL")
status={"status":"STARTED","part_index":index,"requested_action":requested,
        "action_assets":[],"fidelity":"ORIGINAL_NATIVE_SKELETON_REFERENCE_POSE",
        "not_android_runtime_validated":True}

def receipt():
    REPORT_DIR.mkdir(parents=True,exist_ok=True)
    (REPORT_DIR/("native_action_part_%02d_%s.json"%(index,requested))).write_text(
        json.dumps(status,indent=2,default=str)+"\n",encoding="utf-8")

def require(test,message):
    if not test:raise RuntimeError(message)

def times(a,b):
    return (a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
            a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
            a[3]*b[2]-a[0]*b[1]+a[1]*b[2]-a[0]*b[3],
            a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2])

def quat(v):
    s=math.sqrt(sum(x*x for x in v))
    return unreal.Quat(*(x/s for x in v))

def posedelta(axis,degrees):
    theta=math.radians(degrees)/2
    return (axis[0]*math.sin(theta),axis[1]*math.sin(theta),
            axis[2]*math.sin(theta),math.cos(theta))

def write_sequence(sk,mesh,part,desc):
    name="AN_KK_V2_%s_M%02d_P%02d"%(desc.name,int(part["mesh_index"]),
                                 int(part["primitive_index"]))
    dest=OUT+"/"+name
    # Resume safely after a partial batch without rewriting any existing .uasset.
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        existing=unreal.load_asset(dest)
        require(isinstance(existing,unreal.AnimSequence), "Existing asset is not AnimSequence")
        require(existing.get_editor_property("skeleton").get_path_name()==sk.get_path_name(),
                "Existing clip has a foreign skeleton "+dest)
        require(abs(existing.get_play_length()-desc.frames/30)<0.07,
                "Existing clip has wrong duration "+dest)
        stored=set(str(n) for n in existing.get_editor_property("controller").get_model_interface().get_bone_track_names())
        require(len(stored)>=6 and not stored.intersection(ROOT_NAMES),
                "Existing clip has invalid native/rest tracks")
        return {"action":desc.name,"sequence":existing.get_path_name(),
                "skeleton":sk.get_path_name(),"mesh":mesh.get_path_name(),
                "frames":desc.frames+1,"duration_sec":existing.get_play_length(),
                "looping_at_runtime":desc.looping,"bone_tracks":sorted(stored),
                "root_animation_tracks":0,"saved_native_asset":True,
                "disposition":"EXISTING_ASSET_VERIFIED_NOT_OVERWRITTEN"}
    factory=unreal.AnimSequenceFactory()
    factory.set_editor_property("target_skeleton",sk)
    seq=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name,OUT,unreal.AnimSequence,factory)
    require(isinstance(seq,unreal.AnimSequence),"AnimSequence factory failed "+dest)
    require(seq.get_editor_property("skeleton").get_path_name()==sk.get_path_name(),
            "Unexpected created native skeleton")
    pose=sk.get_reference_pose()
    bones=set(str(n) for n in pose.get_bone_names())
    c=seq.get_editor_property("controller")
    c.set_frame_rate(unreal.FrameRate(numerator=30,denominator=1),
                     should_transact=False)
    c.set_number_of_frames(unreal.FrameNumber(value=desc.frames),
                           should_transact=False)
    tracks=[]
    c.open_bracket("Koikatsu character state "+desc.name,False)
    try:
        for bone,axis in AXES.items():
            if bone not in bones:continue
            ref=pose.get_bone_pose(unreal.Name(bone),unreal.AnimPoseSpaces.LOCAL)
            base=(ref.rotation.x,ref.rotation.y,ref.rotation.z,ref.rotation.w)
            trans=[];rots=[];scales=[]
            for frame in range(desc.frames+1):
                d=angles_degrees(desc.name,frame).get(bone,0.0)
                trans.append(unreal.Vector(ref.translation.x,ref.translation.y,ref.translation.z))
                rots.append(quat(times(base,posedelta(axis,d))))
                scales.append(unreal.Vector(ref.scale3d.x,ref.scale3d.y,ref.scale3d.z))
            require(bool(c.add_bone_track(unreal.Name(bone),False)),
                    "Could not add original rig bone "+bone)
            require(bool(c.set_bone_track_keys(unreal.Name(bone),trans,rots,scales,False)),
                    "Could not key original rig bone "+bone)
            tracks.append(bone)
    finally:
        c.close_bracket(False)
    require(len(tracks)>=6,"Not enough native skeleton motion tracks")
    require(not ROOT_NAMES.intersection(tracks),"Root pose must never be animated")
    expected=set(tracks)
    actual=set(str(n) for n in c.get_model_interface().get_bone_track_names())
    require(expected==actual,"UE track model mismatch "+str(actual.symmetric_difference(expected)))
    require(abs(seq.get_play_length()-desc.frames/30)<0.07,"Unexpected UE duration")
    require(unreal.EditorAssetLibrary.save_loaded_asset(seq),"Unreal asset save failed")
    return {"action":desc.name,"sequence":seq.get_path_name(),
            "skeleton":sk.get_path_name(),"mesh":mesh.get_path_name(),
            "frames":desc.frames+1,"duration_sec":seq.get_play_length(),
            "looping_at_runtime":desc.looping,"bone_tracks":tracks,
            "root_animation_tracks":0,"saved_native_asset":True}

receipt()
try:
    require("OfflineGame_Gate12_Koikatsu_20261010" in str(unreal.Paths.get_project_file_path()),
            "Wrong project")
    require(validate()["action_count"]==4,"Pure action definitions do not validate")
    report=json.loads(BLUPRINT.read_text(encoding="utf-8-sig"))
    parts=report["parts"]
    require(len(parts)==19 and 0<=index<len(parts),"Not exact 19-part fixture descriptor")
    part=parts[index]
    require(report["status"]=="PASS_BLUEPRINT_ASSEMBLED_PENDING_COOK_AND_ANDROID",
            "Unvalidated Blueprint fixture receipt")
    ids=part["component"]
    import re
    match=re.search(r"M(\d+)P(\d+)$",ids)
    require(match is not None,"Component lacks original primitive identity")
    part["mesh_index"]=int(match.group(1))
    part["primitive_index"]=int(match.group(2))
    mesh=unreal.load_asset(part["mesh"])
    require(isinstance(mesh,unreal.SkeletalMesh),"Native part mesh not loaded")
    sk=mesh.get_editor_property("skeleton")
    require(sk.get_path_name()==part["skeleton"],"Native per-part skeleton mismatch")
    status["component"]=ids
    status["native_mesh"]=mesh.get_path_name()
    status["native_skeleton"]=sk.get_path_name()
    jobs=ACTIONS if requested=="ALL" else [action(requested)]
    for desc in jobs:
        status["action_assets"].append(write_sequence(sk,mesh,part,desc))
        receipt()
    status["status"]="PASS_NATIVE_QUALITY_V2_SEQUENCE_ASSET_AUTHORING"
except Exception as exc:
    status["status"]="FAIL_NATIVE_ACTION_AUTHORING"
    status["error"]=str(exc)
    status["trace"]=traceback.format_exc()[-2500:]
finally:
    status["timestamp_utc"]=time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())
    receipt()
    unreal.log("GATE12_INCREMENTAL_NATIVE_ACTIONS_"+status["status"])
