#!/usr/bin/env python3
"""Native UE5.8 import plan from 3 ORIGINAL hash-proven Koikatsu cards.
Refuses all pre-2026-10-31 unpaired candidate meshes. Preserves exact parts."""
import json,hashlib,pathlib,sys
R=pathlib.Path(r"D:\UnrealProjects\G12_LuminousNativeImport_20261031")
BASE=R/"AuthenticClothedOriginalCard3D"
ORIGINAL=pathlib.Path(r"D:\UnrealProjects\Gate12_WholeCharacter_Build_20261031\Assets\NoWorld\Luminous\KoikatsuOriginalCards")
REQUIRED={"Awakened":("AWAKENED_CLOTHED","LUMINOUS_AWAKENED_CLOTHED_KK.png","e6b53e82c0d87ecdbd0ff8bf510ce93a1d7e557b436382271fcfed6451897bd5"),
 "Berserker":("BERSERKER_CLOTHED","LUMINOUS_BERSERKER_CLOTHED_KK.png","820307ce971382af9b620fa8610af60918ad01cc7267994ba9ac5cde96070862"),
 "Grand":("GRAND_CLOTHED","LUMINOUS_GRAND_CONVERGENCE_CLOTHED_KK.png","26c48e1c03c154f9f39a0b06afaade53423c11dbd079604e430162eb4cae5353")}
def sha(path):
 h=hashlib.sha256()
 with path.open("rb") as f:
  for data in iter(lambda:f.read(1024*1024),b""):h.update(data)
 return h.hexdigest()
allgroups=[];source_reports={}
for form,(state,card,expected) in REQUIRED.items():
 folder=BASE/form
 source=folder/"ORIGINAL_APPROVED_CARD_FULL_3D_BLENDER_PROOF.json"
 assert source.is_file(),f"{form}: original-source Blender conversion missing"
 record=json.loads(source.read_text("utf8"))
 assert record["form"]==form and record["original_card_sha256"]==expected and record["true_card_to_export_api_verified"]
 assert record["status"]=="PASS_EXACT_ORIGINAL_KOIKATSU_CARD_SIX_SOURCED_GLBS_PENDING_NATIVE_UE",record["status"]
 assert sha(ORIGINAL/card)==expected
 proof=R/"Saved/CorrectKoikatsuExports"/("Gate12_Card_"+state+"_Original_API_Export.json")
 p=json.loads(proof.read_text("utf8"))
 assert p["status"]=="EXPORTED_NATIVE_EXACT_ORIGINAL_CARD" and p["source_card_sha256"]==expected
 assert sha(pathlib.Path(p["pmx_path"]))==record["native_pmx_sha256"]==p["pmx_sha256"]
 assets=record["native_unreal_part_sources"]
 assert len(assets)==record["source_geometry"]["mesh_parts"] and len(assets)>=3
 ids=set()
 for asset in assets:
  n=asset["part"];assert n not in ids;ids.add(n)
  path=pathlib.Path(asset["path"])
  assert path.is_file() and sha(path)==asset["source_glb_sha256"]
  assert path.parent.resolve()==folder.resolve() and "TrueOriginalPart" in path.name
  name=f"ExactCard_{form}_{n:02d}"
  destination=f"/Game/NoWorld/Characters/Luminous/{form}/ExactOriginalClothed/Part_{n:02d}"
  allgroups.append({"GroupName":name,"Filenames":[str(path)],"DestinationPath":destination,"bReplaceExisting":False,"bSkipReadOnly":True})
 source_reports[form]={"original_card_hash":expected,"native_pmx_hash":p["pmx_sha256"],
    "source_geometry":record["source_geometry"],"parts":len(assets)}
out=R/"Saved"/"G12_AllGenuineOriginalCardNativeImportPlan.json"
out.write_text(json.dumps({"ImportGroups":allgroups},indent=2)+"\n","utf8")
print("EXACT_CARD_NATIVE_UE_SOURCE_VALIDATED",json.dumps(source_reports),flush=True)
print("REAL_EXACT_CARD_IMPORT_GROUPS",len(allgroups),str(out),flush=True)
