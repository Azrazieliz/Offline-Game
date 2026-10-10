"""Generate an explicitly fixture-only Unreal player descriptor from accepted native per-part animation evidence."""
import argparse,pathlib,json,re,hashlib
p=argparse.ArgumentParser()
p.add_argument("--character-id",required=True)
p.add_argument("--native-report",required=True,type=pathlib.Path)
p.add_argument("--source-shard-manifest",required=True,type=pathlib.Path)
p.add_argument("--output-descriptor",required=True,type=pathlib.Path)
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
 parts.append({"mesh_index":ix,"primitive_index":pr,"mesh":mesh,"skeleton":skeleton,
  "sequence":seq,"visible":ix not in (0,1,2),
  "motion_map":{"idle":seq,"walk":None,"run":None,"airborne":None,"dodge":None},
  "facial_curve_names":[]})
data={"schema_version":"KK_UNREAL_CHARACTER_DESCRIPTOR_1",
      "status":"PASS","character_id":a.character_id,
      "source_glb_sha256":s["original_glb_sha256"],
      "source_primitive_count":s["full_primitive_count"],
      "source_morph_reference_count":s["full_morph_links"],
      "native_part_count":len(parts),"parts":parts,
      "asset_status":"ENGINEERING_FIXTURE_NOT_ARTIFACT_READY",
      "validated_motion_states":["idle_basic_sway"],
      "unvalidated_motion_states":["walk","run","airborne","dodge"],
      "requires_gate11_provenance":True}
out=a.output_descriptor.resolve();out.parent.mkdir(parents=True,exist_ok=True)
if out.exists():raise FileExistsError("Refusing overwrite "+str(out))
out.write_text(json.dumps(data,indent=2,ensure_ascii=False),encoding="utf8")
print("DESCRIPTOR_PASS",data["character_id"],"NATIVE_PARTS",data["native_part_count"],
      "SOURCE_SHA256",data["source_glb_sha256"])
