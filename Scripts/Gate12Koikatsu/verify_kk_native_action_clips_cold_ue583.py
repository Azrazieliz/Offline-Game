"""Cold UE5.8.3 editor read-only validation for all saved Koikatsu exact-rig action clips.

This is not a phone playback/fidelity claim. Loads only existing skeletal
assets and animations, one at a time, without authoring, mutating or saving.
"""
import json
import pathlib
import time
import traceback
import unreal

ROOT=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010")
SOURCE=ROOT/"Saved/Gate12Koikatsu/NativeActionClips/171_native_action_assets_static_integrity.json"
DEST=ROOT/"Saved/Gate12Koikatsu/NativeActionClips/171_native_action_clips_cold_unreal_ue583.json"
out={"status":"STARTED","verified":0,"errors":[],"per_part":[],
     "android_runtime":"NOT_TESTED","mobile_render":"NOT_TESTED"}
def save():
    DEST.write_text(json.dumps(out,indent=2,default=str)+"\n",encoding="utf8")
def require(pred,msg):
    if not pred:raise RuntimeError(msg)
save()
try:
    require("OfflineGame_Gate12_Koikatsu_20261010" in
            str(unreal.Paths.get_project_file_path()),"Wrong isolated Unreal project")
    inventory=json.loads(SOURCE.read_text(encoding="utf-8"))
    require(inventory["status"]=="PASS_STATIC_171_REAL_UE_UASSET_PACKAGES",
            "Input static inventory not accepted")
    require(len(inventory["per_part"])==19,"Expected 19 exact rig parts")
    expected_actions={"Walk","Run","Jump","Fall","Land","TurnLeft",
                      "TurnRight","Dodge","Action"}
    for part in inventory["per_part"]:
        group=part["component"]
        require(len(part["actions"])==9,"Missing action for "+group)
        import re
        ids=re.search(r"M(\d+)P(\d+)$",group)
        require(ids,"Malformed native part "+group)
        m,p=map(int,ids.groups())
        if m in (4,5):
            mesh_dir=f"/Game/Experimental/Gate12Koikatsu/FullFidelity/Mesh{m:02d}"
        else:
            mesh_dir=f"/Game/Experimental/Gate12Koikatsu/FullPrimitives/Mesh{m:02d}Prim{p:02d}"
        rec={"part":group,"actions":[]}
        names=set()
        for item in part["actions"]:
            a=item["action"]
            require(a in expected_actions and a not in names,"Bad action "+str(a))
            names.add(a)
            asset_name=f"AN_KK_{a}_M{m:02d}_P{p:02d}"
            seqpath=f"/Game/Experimental/Gate12Koikatsu/Animation/NativeActions/{asset_name}"
            seq=unreal.load_asset(seqpath)
            require(isinstance(seq,unreal.AnimSequence),"Not cold-loadable AnimSequence: "+seqpath)
            sk=seq.get_editor_property("skeleton")
            require(sk is not None,"Missing native skeleton: "+seqpath)
            model=seq.get_editor_property("controller").get_model_interface()
            tracks=[str(n) for n in model.get_bone_track_names()]
            require(len(tracks)>=6,"Too few tracks "+seqpath)
            require(not (set(tracks)&{"Center","pelvis","Pelvis"}),
                    "Root track would move source rig "+seqpath)
            require(abs(seq.get_play_length()-
                        next(x for x in (1.0,0.8,0.6) if abs(seq.get_play_length()-x)<0.06))<0.07,
                    "Unusual animation length "+seqpath)
            require(mesh_dir in sk.get_path_name(),
                    "Skeleton path incompatible with imported mesh "+seqpath)
            rec["actions"].append({"name":a,"asset":seqpath,
                "skeleton":sk.get_path_name(),"tracks":len(tracks),
                "duration":seq.get_play_length()})
            out["verified"]+=1
            if out["verified"]%9==0:save()
        require(names==expected_actions,"Native action closure incomplete "+group)
        out["per_part"].append(rec);save()
    require(out["verified"]==171,"UE cold load count not 171")
    out["status"]="PASS_COLD_UNREAL_171_EXACT_SKELETON_CLIPS"
except Exception as exc:
    out["status"]="FAIL_UNREAL_CLIP_COLD_AUDIT"
    out["errors"].append(str(exc))
    out["trace"]=traceback.format_exc()[-1600:]
finally:
    out["utc"]=time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())
    save()
    unreal.log("GATE12_NATIVE_ACTION_CLIP_COLD_"+out["status"])
