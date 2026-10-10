"""Strict static audit of all 171 new full-fidelity native Unreal animation assets.

Does not open UE, validate phone rendering, or assert real gameplay controls.
"""
import collections
import hashlib
import json
import pathlib
import sys
from kk_native_action_profiles import ACTIONS

ROOT=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010")
R=ROOT/"Saved/Gate12Koikatsu"
ASSETS=ROOT/"Content/Experimental/Gate12Koikatsu/Animation/NativeActions"
REPORT=R/"NativeActionClips/171_native_action_assets_static_integrity.json"

def hash_file(path):
    h=hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda:file.read(1024*1024),b""):
            h.update(block)
    return h.hexdigest()

result={"status":"STARTED","native_cold_reload":"NOT_TESTED",
        "android_actions":"NOT_TESTED","native_gameplay_binding":"NOT_TESTED",
        "per_part":[]}
try:
    bp=json.loads((R/"playable_blueprint_authoring_report.json").read_text(encoding="utf-8-sig"))
    assert bp["status"]=="PASS_BLUEPRINT_ASSEMBLED_PENDING_COOK_AND_ANDROID"
    assert len(bp["parts"])==19
    assert len(set(p["component"] for p in bp["parts"]))==19
    all_paths=[]
    digests=[]
    for i,expected_part in enumerate(bp["parts"]):
        receipt=R/"NativeActionClips"/("native_action_part_%02d_ALL.json"%i)
        detail=json.loads(receipt.read_text(encoding="utf-8"))
        assert detail["status"]=="PASS_NATIVE_ACTION_SEQUENCE_ASSET_AUTHORING"
        assert detail["component"]==expected_part["component"]
        assert detail["native_skeleton"]==expected_part["skeleton"]
        assert len(detail["action_assets"])==len(ACTIONS)
        assert {a["action"] for a in detail["action_assets"]}=={a.name for a in ACTIONS}
        part_summaries=[]
        for a in detail["action_assets"]:
            expected=next(x for x in ACTIONS if x.name==a["action"])
            assert a["skeleton"]==expected_part["skeleton"]
            assert a["mesh"]==expected_part["mesh"]
            assert a["root_animation_tracks"]==0
            assert a["frames"]==expected.frames+1
            assert len(a["bone_tracks"])>=6
            assert a["looping_at_runtime"]==expected.looping
            assert abs(a["duration_sec"]-expected.frames/30)<0.07
            package=a["sequence"].split(".",1)[0]
            assert package.startswith("/Game/Experimental/Gate12Koikatsu/Animation/NativeActions/")
            file=ROOT/"Content"/(package[len("/Game/"):] + ".uasset")
            assert file.is_file() and file.stat().st_size>=1024
            assert file.read_bytes()[:4]==bytes.fromhex("c1832a9e"),"Not Unreal package"
            all_paths.append(file)
            digest=hash_file(file)
            digests.append((file.name,digest))
            part_summaries.append({"action":a["action"],"sha256":digest,"bytes":file.stat().st_size,
                                   "tracks":len(a["bone_tracks"])})
        result["per_part"].append({"index":i,"component":expected_part["component"],
                                    "actions":part_summaries})
    assert len(set(all_paths))==171,"Reused output path"
    on_disk=set(ASSETS.glob("*.uasset"))
    assert on_disk==set(all_paths), "Unexpected/missing native action packages"
    result["total_parts"]=19
    result["total_native_assets"]=171
    result["total_bytes"]=sum(f.stat().st_size for f in all_paths)
    result["per_action_counts"]=dict(collections.Counter(a["action"] for x in result["per_part"] for a in x["actions"]))
    result["aggregate_sha256"]=hashlib.sha256("".join(k+v for k,v in sorted(digests)).encode()).hexdigest()
    result["status"]="PASS_STATIC_171_REAL_UE_UASSET_PACKAGES"
except Exception as exc:
    result["status"]="FAIL_STATIC_INTEGRITY"
    result["error"]=str(exc)
finally:
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({k:v for k,v in result.items() if k!="per_part"},indent=2))
sys.exit(0 if result["status"].startswith("PASS") else 2)
