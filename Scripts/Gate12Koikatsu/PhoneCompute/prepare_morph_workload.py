"""Prepare precise source GLB accessor metadata for ARM64 phone CPU morph evaluation.
No source bytes changed. Real sparse + dense POSITION morph deltas are evaluated
by the native worker on Android in 61 distinct frames.
"""
import argparse,hashlib,json,pathlib,struct
parser=argparse.ArgumentParser()
parser.add_argument("--glb",required=True,type=pathlib.Path)
parser.add_argument("--out",required=True,type=pathlib.Path)
args=parser.parse_args()
source=args.glb.resolve(strict=True)
data=source.read_bytes()
assert data[:4]==b"glTF"
_,ver,total=struct.unpack_from("<4sII",data,0)
assert ver==2 and total==len(data)
json_len,json_type=struct.unpack_from("<II",data,12)
assert json_type==0x4E4F534A
doc=json.loads(data[20:20+json_len])
bin_offset=20+json_len+8
assert struct.unpack_from("<I",data,20+json_len+4)[0]==0x004E4942
access=doc["accessors"];views=doc["bufferViews"]
def addr(a):
 v=views[a["bufferView"]]
 return bin_offset+v.get("byteOffset",0)+a.get("byteOffset",0)
def stride(a):
 return views[a["bufferView"]].get("byteStride",12)
records=[];baseinfo=[];morph=0
for mesh_id,m in enumerate(doc["meshes"]):
 for prim_id,p in enumerate(m["primitives"]):
  ac=access[p["attributes"]["POSITION"]]
  assert ac["componentType"]==5126 and ac["type"]=="VEC3" and "bufferView" in ac
  if ac.get("sparse"): raise RuntimeError("Sparse BASE POSITION not supported")
  group=len(baseinfo)
  records.append(f"B {group} {ac['count']} {addr(ac)} {stride(ac)}")
  baseinfo.append({"mesh":mesh_id,"primitive":prim_id,"vertices":ac["count"],"targets":len(p.get("targets",[]))})
  for target in p.get("targets",[]):
   if "POSITION" not in target: raise RuntimeError("Target with no position delta")
   a=access[target["POSITION"]]
   assert a["componentType"]==5126 and a["type"]=="VEC3"
   assert a["count"]==ac["count"]
   if "sparse" in a:
    sparse=a["sparse"];indices=sparse["indices"];values=sparse["values"]
    iv=views[indices["bufferView"]];vv=views[values["bufferView"]]
    it=indices["componentType"]
    if it not in (5121,5123,5125):raise RuntimeError("Unknown sparse index type")
    records.append(f"M {group} 1 {a['count']} {sparse['count']} {it} {bin_offset+iv.get('byteOffset',0)+indices.get('byteOffset',0)} {bin_offset+vv.get('byteOffset',0)+values.get('byteOffset',0)} 0 0")
   else:
    records.append(f"M {group} 0 {a['count']} 0 0 0 0 {addr(a)} {stride(a)}")
   morph+=1
out=args.out.resolve()
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(f"KKMORPH1 {len(baseinfo)} {morph} 61 {len(data)}\n"+"\n".join(records)+"\n",encoding="ascii")
summary={"status":"PREPARED_FOR_REAL_ANDROID_CPU","glb_sha256":hashlib.sha256(data).hexdigest(),
"base_primitive_count":len(baseinfo),"morph_targets":morph,"frames":61,
"source_bytes":len(data),"metadata_sha256":hashlib.sha256(out.read_bytes()).hexdigest(),"groups":baseinfo}
out.with_suffix(".json").write_text(json.dumps(summary,indent=2),encoding="utf8")
print("PHONE_WORKLOAD_READY",summary["base_primitive_count"],summary["morph_targets"],summary["frames"],summary["glb_sha256"])
