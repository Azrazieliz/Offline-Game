"""Lossless KK export-quality audit. No image edits, no resizing, no UE import.
Examines glTF source materials/embedded texture dimensions/alpha for UE suitability.
"""
import argparse,struct,json,pathlib,io,hashlib,sys
from PIL import Image
ap=argparse.ArgumentParser()
ap.add_argument("--source-glb",type=pathlib.Path,required=True)
ap.add_argument("--output-json",type=pathlib.Path,required=True)
x=ap.parse_args()
src=x.source_glb.resolve(strict=True)
raw=src.read_bytes()
if raw[:4]!=b"glTF":raise ValueError("Requires glTF 2.0 binary")
pos=12;parts={}
while pos<len(raw):
 size,typ=struct.unpack_from("<II",raw,pos);pos+=8
 parts[typ]=raw[pos:pos+size];pos+=size
doc=json.loads(parts[0x4e4f534a]);blob=parts[0x4e4942]
images=[];problems=[]
for n,im in enumerate(doc.get("images",[])):
 if "uri" in im:raise ValueError("External image URI not supported")
 view=doc["bufferViews"][im["bufferView"]];offset=view.get("byteOffset",0)
 payload=blob[offset:offset+view["byteLength"]]
 with Image.open(io.BytesIO(payload)) as bitmap:
  rgb=bitmap.convert("RGBA");a=rgb.getchannel("A");h=a.histogram()
  tot=sum(h);fraction_mid=sum(h[1:255])/tot
  img={"index":n,"name":im.get("name"),"width":bitmap.width,"height":bitmap.height,
    "format":bitmap.format,"sha256":hashlib.sha256(payload).hexdigest(),
    "alpha_opaque_fraction":round(h[255]/tot,6),"alpha_transparent_fraction":round(h[0]/tot,6),
    "alpha_gradient_fraction":round(fraction_mid,6),
    "likely_cutout_mask":h[0]>0 and fraction_mid <.01,
    "original_encoded_bytes":len(payload)}
  del rgb,a
 images.append(img)
textures=doc.get("textures",[]);materials=[]
for i,mat in enumerate(doc.get("materials",[])):
 pbr=mat.get("pbrMetallicRoughness",{})
 base=pbr.get("baseColorTexture")
 im=None
 if base:
  ti=base["index"]
  if ti<len(textures):
   image_index=textures[ti].get("source")
   if image_index is not None and image_index<len(images):im=images[image_index]
 entry={"index":i,"name":mat.get("name"),"alpha_mode":mat.get("alphaMode","OPAQUE"),
 "alpha_cutoff":mat.get("alphaCutoff"),"double_sided":mat.get("doubleSided",False),
 "base_image_index":im["index"] if im else None,
 "base_image_name":im["name"] if im else None,
 "base_image_pixels":(im["width"]*im["height"]) if im else None,
 "base_color_factor":pbr.get("baseColorFactor",[1,1,1,1]),
 "metallic_factor":pbr.get("metallicFactor",1),"roughness_factor":pbr.get("roughnessFactor",1),
 "normal_map_present":"normalTexture" in mat,
 "alpha_gradient_fraction":im["alpha_gradient_fraction"] if im else None,
 "cutout_candidate_not_automatic":bool(im and im["likely_cutout_mask"] and mat.get("alphaMode")=="BLEND")}
 materials.append(entry)
 if entry["cutout_candidate_not_automatic"]:
  problems.append("Binary-alpha translucent material "+str(entry["name"])+": masked shader may improve depth sorting, requires visual QA")
 if im and im["width"]>=4096 and im["height"]>=4096:
  pass
report={"status":"AUDIT_SOURCE_ONLY","source_path":str(src),"source_sha256":hashlib.sha256(raw).hexdigest(),
  "mesh_count":len(doc.get("meshes",[])),"primitive_count":sum(len(m["primitives"]) for m in doc.get("meshes",[])),
  "materials_count":len(materials),"images_count":len(images),
  "images":images,"materials":materials,"quality_observations":problems,
  "limits":["Source PBR material factors do not reproduce every Koikatsu custom shader/toon parameter in Unreal.",
   "Shader usage flags and morph shaders require UE native cook/device validation.",
   "Full texture fidelity is preserved; no offline resizing or quality lowering performed.",
   "Do not automatically convert translucent alpha-gradient hair or other soft materials to MASKED.",
   "Only engine/runtime measurements can establish resulting visual quality and FPS."]}
dst=x.output_json.resolve();dst.parent.mkdir(parents=True,exist_ok=True)
dst.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf8")
print("QUALITY_AUDIT",report["status"],"IMAGES",len(images),"MATERIALS",len(materials),
      "CUTOUT_CANDIDATES",len(problems),"SOURCE_SHA",report["source_sha256"])
