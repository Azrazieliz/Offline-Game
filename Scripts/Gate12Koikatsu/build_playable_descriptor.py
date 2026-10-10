"""Generate an explicitly fixture-only Unreal player descriptor from accepted native per-part animation evidence."""
import argparse,pathlib,json,re,hashlib
p=argparse.ArgumentParser()
p.add_argument("--character-id",required=True)
p.add_argument("--native-report",required=True,type=pathlib.Path)
p.add_argument("--source-shard-manifest",required=True,type=pathlib.Path)
p.add_argument("--output-descriptor",required=True,type=pathlib.Path)
p.add_argument("--native-action-cold-report",type=pathlib.Path,
               help="Optional independent 171-action real Unreal cold-load receipt")
a=p.parse_args()
if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_-]{2,63}",a.character_id):raise ValueError("Unsafe ID")
n=json.loads(a.native_report.read_text(encoding="utf8"))
s=json.loads(a.source_shard_manifest.read_text(encoding="utf8"))
if n.get("status")!="PASS" or any(x.get("status")!="PASS" for x in n.get("parts",[])):
 raise ValueError("Native exact skeleton animation evidence is not PASS")
if s.get("status")!="SOURCE_LOSSLESS_SHARDS_VERIFIED":
 raise ValueError("Source interchange not hash verified")
if n.get("source_sha256")!=s.get("original_glb_sha256"):
 raise ValueError("Native assets and source interchange do not share source SHA")
if n.get("unique_native_sequences")!=len(n.get("parts",[])):
 raise ValueError("Incomplete per-skeleton animation coverage")
action_by_component={}
if a.native_action_cold_report:
 cold=json.loads(a.native_action_cold_report.read_text(encoding="utf8"))
 if cold.get("status")!="PASS_COLD_UNREAL_171_EXACT_SKELETON_CLIPS":
  raise ValueError("Unreal action clips not independently cold validated")
 for group in cold.get("per_part",[]):
  name=group.get("part")
  if not isinstance(name,str) or name in action_by_component:
   raise ValueError("Duplicated or invalid imported component identity")
  actions={}
  for clip in group.get("actions",[]):
   key=clip.get("name")
   if key in actions or not key or not str(clip.get("asset","")).startswith("/Game/"):
    raise ValueError("Duplicate, non-native or unsafe UE action clip")
   actions[key]=clip
  if set(actions)!={"Walk","Run","Jump","Fall","Land","TurnLeft","TurnRight","Dodge","Action"}:
   raise ValueError("Incomplete native action coverage for "+name)
  action_by_component[name]=actions
 if len(action_by_component)!=len(n["parts"]):
  raise ValueError("Independent UE cold action count mismatches original native parts")
seen=set();parts=[]
for part in n["parts"]:
 mesh=part["mesh"];seq=part["sequence"];skeleton=part["skeleton"]
 if not all(str(x).startswith("/Game/") for x in (mesh,seq,skeleton)):
  raise ValueError("Source must contain exact /Game paths, never arbitrary disk paths")
 if mesh in seen:raise ValueError("Duplicate mesh")
 seen.add(mesh)
 if part.get("root_animation_tracks")!=0 or not part.get("tracks_verified"):
  raise ValueError("Wrong skeleton root or animation tracks")
 ix=int(part["mesh_index"]);pr=int(part["primitive_index"])
 identity="KK_Full_M%02dP%02d"%(ix,pr)
 motion={"idle":seq,"walk":None,"run":None,"jump":None,"fall":None,"land":None,
         "turn_left":None,"turn_right":None,"dodge":None,"action":None}
 if action_by_component:
  clips=action_by_component.get(identity)
  if not clips or any(c["skeleton"]!=skeleton for c in clips.values()):
   raise ValueError("Independent UE action skeleton mismatch for "+identity)
  for key,action_name in (("walk","Walk"),("run","Run"),("jump","Jump"),
                          ("fall","Fall"),("land","Land"),("turn_left","TurnLeft"),
                          ("turn_right","TurnRight"),("dodge","Dodge"),("action","Action")):
   motion[key]=clips[action_name]["asset"]
 parts.append({"mesh_index":ix,"primitive_index":pr,"mesh":mesh,"skeleton":skeleton,
  "sequence":seq,"visible":ix not in (0,1,2),
  "motion_map":motion,"facial_curve_names":[]})
data={"schema_version":"KK_UNREAL_CHARACTER_DESCRIPTOR_1",
      "status":"PASS","character_id":a.character_id,
      "source_glb_sha256":s["original_glb_sha256"],
      "source_primitive_count":s["full_primitive_count"],
      "source_morph_reference_count":s["full_morph_links"],
      "native_part_count":len(parts),"parts":parts,
      "asset_status":"ENGINEERING_FIXTURE_NOT_ARTIFACT_READY",
      "native_cold_validated_motion_states":list(parts[0]["motion_map"]) if action_by_component else ["idle"],
      "android_validated_motion_states":[],
      "motion_evidence_scope":"171_NATIVE_COLD_RELOADED_NOT_ANDROID" if action_by_component else "REST_SAFE_ONLY",
      "validated_motion_states":["idle_basic_sway"],
      "unvalidated_motion_states":["walk","run","jump","fall","land","turn_left","turn_right","dodge","action"],
      "requires_gate11_provenance":True}
out=a.output_descriptor.resolve();out.parent.mkdir(parents=True,exist_ok=True)
if out.exists():raise FileExistsError("Refusing overwrite "+str(out))
out.write_text(json.dumps(data,indent=2,ensure_ascii=False),encoding="utf8")
print("DESCRIPTOR_PASS",data["character_id"],"NATIVE_PARTS",data["native_part_count"],
      "SOURCE_SHA256",data["source_glb_sha256"])
