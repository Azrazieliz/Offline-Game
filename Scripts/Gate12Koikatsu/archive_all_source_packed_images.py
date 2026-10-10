"""Blender background script: archive ALL original KKBP packed image file bytes.
No image downscaling, recoding, re-saving or modification. Source .blend read-only.
Use: blender -b unreal_skeleton_prepared.blend -t 1 --python this_script.py
"""
import bpy,os,re,json,hashlib,pathlib,traceback
ROOT=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu")
DEST=ROOT/"original_KKBP_all_packed_images"
DEST.mkdir(parents=True,exist_ok=True)
report={"status":"STARTED","source_blend":bpy.data.filepath,"images":[],"materials":[]}
try:
 for idx,image in enumerate(bpy.data.images):
    item={"index":idx,"name":image.name,"width":image.size[0],"height":image.size[1],"channels":image.channels,
          "source_path":image.filepath,"packed":bool(image.packed_file),"file_format":image.file_format}
    if image.packed_file:
        data=bytes(image.packed_file.data)
        suffix=pathlib.Path(image.filepath).suffix
        if not suffix:suffix=".png" if data[:8]==b"\x89PNG\r\n\x1a\n" else ".binary"
        safe=re.sub(r'[^A-Za-z0-9_.-]+','_',pathlib.Path(image.name).stem)[:80]
        target=DEST/("%02d_"%idx+safe+suffix)
        if target.exists():
            if hashlib.sha256(target.read_bytes()).hexdigest()!=hashlib.sha256(data).hexdigest():
                raise RuntimeError("Refusing overwrite of non-identical archived source "+str(target))
        else:target.write_bytes(data)
        item.update({"archive":str(target),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest(),
                     "byte_identical":target.read_bytes()==data})
    report["images"].append(item)
 for m in bpy.data.materials:
    nodes=[]
    if m.use_nodes:
        for n in m.node_tree.nodes:
            if n.type=="TEX_IMAGE":
                nodes.append({"node":n.name,"image":n.image.name if n.image else None})
    report["materials"].append({"name":m.name,"surface_method":getattr(m,"surface_render_method","UNSPECIFIED"),
                                "texture_nodes":nodes})
 report["mesh_slot_counts"]={o.name:len(o.material_slots) for o in bpy.data.objects if o.type=="MESH"}
 report["source_mesh_count"]=len(report["mesh_slot_counts"])
 report["source_material_count"]=len(report["materials"])
 report["image_count"]=len(report["images"])
 report["packed_count"]=sum(int(x["packed"]) for x in report["images"])
 report["all_packed_byte_identical"]=all(x.get("byte_identical",True) for x in report["images"] if x["packed"])
 report["status"]="PASS" if report["all_packed_byte_identical"] else "FAIL"
except Exception as e:
 report["status"]="FAIL";report["error"]=str(e);report["trace"]=traceback.format_exc()
finally:
 (DEST/"archived_source_manifest.json").write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf8")
 print("GATE12_ARCHIVE_STATUS="+report["status"]+" IMAGES="+str(len(report["images"]))+
       " PACKED="+str(sum(int(x.get("packed",False)) for x in report["images"]))+
       " MATERIALS="+str(len(report["materials"])),flush=True)
