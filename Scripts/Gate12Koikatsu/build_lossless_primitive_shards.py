"""Non-destructive KKBP GLB sharding, full meshes / all morph targets / exact texture bytes.
Source GLB remains immutable. Six output skinned-mesh shards reconstruct the source
without downscaling any image, deleting a morph, or dropping a used material.
Only material alphaMode may be corrected from BLEND to OPAQUE when input texture
alpha channel is completely opaque. No mesh/accessor/image payload bytes are edited.
"""
import os,json,struct,hashlib,io,copy,pathlib,collections
from PIL import Image

SOURCE=pathlib.Path(r"D:\Tools\GameAssetPipeline\KoikatsuParty\characters\G01-KK-PIPELINE-TEST-0002\conversion\character_unreal_skeleton.glb")
ROOT=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu")
DEST=ROOT/"full_fidelity_primitive_shards"
DEST.mkdir(exist_ok=True,parents=True)
ORIGINAL_SHA="ad06955e6f725c63c6ee014828877a9a8fa3a19479ca29936c58c96ed6d012e0"
data=SOURCE.read_bytes()
assert hashlib.sha256(data).hexdigest()==ORIGINAL_SHA
magic,ver,actual_len=struct.unpack_from("<4sII",data,0)
assert magic==b"glTF" and ver==2 and actual_len==len(data)
pos=12;chunks={}
while pos<len(data):
    sz,ctype=struct.unpack_from("<II",data,pos);pos+=8
    chunks[ctype]=data[pos:pos+sz];pos+=sz
    assert pos%4==0
doc=json.loads(chunks[0x4e4f534a]);blob=chunks[0x4e4942]
assert len(doc["meshes"])==6 and len(doc["skins"])==1
materials=doc["materials"];texts=doc.get("textures",[]);images=doc.get("images",[])
if any(im.get("uri") for im in images):raise ValueError("external image URI not allowed")
all_mat_ids=set();image_props={}
for imidx,im in enumerate(images):
    view=doc["bufferViews"][im["bufferView"]]
    src_blob=blob[view.get("byteOffset",0):view.get("byteOffset",0)+view["byteLength"]]
    with Image.open(io.BytesIO(src_blob)) as img:
        rgba=img.convert("RGBA");lo,hi=rgba.getchannel("A").getextrema()
        image_props[imidx]={"name":im.get("name"),"width":img.width,"height":img.height,"original_sha256":hashlib.sha256(src_blob).hexdigest(),"original_encoded_bytes":len(src_blob),"alpha_min":lo,"alpha_max":hi}

def referenced_texture_slots(node):
    if isinstance(node,dict):
        for k,v in node.items():
            if k.endswith("Texture") and isinstance(v,dict) and isinstance(v.get("index"),int):yield v
            yield from referenced_texture_slots(v)
    elif isinstance(node,list):
        for item in node:yield from referenced_texture_slots(item)

def process_mesh(mesh_index,primitive_index):
    mesh=copy.deepcopy(doc["meshes"][mesh_index])
    mesh["primitives"]=[mesh["primitives"][primitive_index]]
    needed_access=set()
    used_mat=sorted(set(p["material"] for p in mesh["primitives"] if "material" in p))
    all_mat_ids.update(used_mat)
    needed_access.add(doc["skins"][0]["inverseBindMatrices"])
    for prim in mesh["primitives"]:
        if "indices" in prim:needed_access.add(prim["indices"])
        needed_access.update(prim.get("attributes",{}).values())
        for target in prim.get("targets",[]):needed_access.update(target.values())
    output=bytearray();new_views=[];view_map={}
    def append(original_bytes,orig_view=None):
        while len(output)%4:output.append(0)
        offset=len(output);output.extend(original_bytes)
        new={"buffer":0,"byteOffset":offset,"byteLength":len(original_bytes)}
        if orig_view:
            for field in ("byteStride","target"):
                if field in orig_view:new[field]=orig_view[field]
        new_views.append(new)
        return len(new_views)-1
    def remap_view(oldidx):
        if oldidx not in view_map:
            view=doc["bufferViews"][oldidx];offset=view.get("byteOffset",0)
            view_map[oldidx]=append(blob[offset:offset+view["byteLength"]],view)
        return view_map[oldidx]
    new_access=[];access_map={}
    for ix in sorted(needed_access):
        a=copy.deepcopy(doc["accessors"][ix])
        if "bufferView" in a:a["bufferView"]=remap_view(a["bufferView"])
        if "sparse" in a:
            a["sparse"]["indices"]["bufferView"]=remap_view(a["sparse"]["indices"]["bufferView"])
            a["sparse"]["values"]["bufferView"]=remap_view(a["sparse"]["values"]["bufferView"])
        access_map[ix]=len(new_access);new_access.append(a)
    for prim in mesh["primitives"]:
        if "indices" in prim:prim["indices"]=access_map[prim["indices"]]
        prim["attributes"]={k:access_map[v] for k,v in prim["attributes"].items()}
        for target in prim.get("targets",[]):
            for key,ix in list(target.items()):target[key]=access_map[ix]
    textures=[];tex_map={};new_images=[];im_map={};samplers=[];sam_map={}
    def trans_tex(old_id):
        if old_id in tex_map:return tex_map[old_id]
        t=copy.deepcopy(texts[old_id])
        oldimage=t["source"]
        if oldimage not in im_map:
            im=copy.deepcopy(images[oldimage])
            im["bufferView"]=remap_view(im["bufferView"])
            im_map[oldimage]=len(new_images)
            new_images.append(im)
        t["source"]=im_map[oldimage]
        if "sampler" in t:
            oldsam=t["sampler"]
            if oldsam not in sam_map:
                sam_map[oldsam]=len(samplers);samplers.append(copy.deepcopy(doc["samplers"][oldsam]))
            t["sampler"]=sam_map[oldsam]
        tex_map[old_id]=len(textures);textures.append(t)
        return tex_map[old_id]
    new_mats=[];mat_map={}
    alpha_changes=[]
    for mid in used_mat:
        mat=copy.deepcopy(materials[mid])
        old_slots=list(referenced_texture_slots(mat))
        base_tex=mat.get("pbrMetallicRoughness",{}).get("baseColorTexture",{}).get("index")
        if base_tex is not None:
            tex_image=texts[base_tex]["source"]
            imageinfo=image_props[tex_image]
            if mat.get("alphaMode")=="BLEND" and imageinfo["alpha_min"]==255:
                mat["alphaMode"]="OPAQUE"
                alpha_changes.append({"material":mat.get("name"),"from":"BLEND","to":"OPAQUE","reason":"exact image A channel 255 at every pixel"})
        for slot in old_slots:slot["index"]=trans_tex(slot["index"])
        mat_map[mid]=len(new_mats);new_mats.append(mat)
    for p in mesh["primitives"]:
        if "material" in p:p["material"]=mat_map[p["material"]]
    nodes=copy.deepcopy(doc["nodes"]);selected=[]
    for i,node in enumerate(nodes):
        if "mesh" in node:
            if node["mesh"]==mesh_index:
                node["mesh"]=0;selected.append(i)
            else:node.pop("mesh",None);node.pop("skin",None)
    for node in nodes:
        if "children" in node:node["children"]=[ix for ix in node["children"] if ix not in selected]
    scenes=copy.deepcopy(doc["scenes"])
    for scene in scenes:scene["nodes"]=list(dict.fromkeys(scene.get("nodes",[])+selected))
    skin=copy.deepcopy(doc["skins"][0]);skin["inverseBindMatrices"]=access_map[skin["inverseBindMatrices"]]
    shard={"asset":doc["asset"],"scene":doc.get("scene",0),"scenes":scenes,"nodes":nodes,
         "meshes":[mesh],"skins":[skin],"materials":new_mats,"textures":textures,
         "images":new_images,"samplers":samplers,"accessors":new_access,
         "bufferViews":new_views,"buffers":[{"byteLength":len(output)}]}
    if "extensionsUsed" in doc:shard["extensionsUsed"]=doc["extensionsUsed"]
    if "extensionsRequired" in doc:shard["extensionsRequired"]=doc["extensionsRequired"]
    if "extras" in doc:shard["extras"]=doc["extras"]
    encoded=json.dumps(shard,ensure_ascii=False,separators=(",",":")).encode("utf-8")
    encoded+=b" " * ((-len(encoded))%4)
    while len(output)%4:output.append(0)
    shard["buffers"][0]["byteLength"]=len(output)
    encoded=json.dumps(shard,ensure_ascii=False,separators=(",",":")).encode("utf-8")
    encoded+=b" " * ((-len(encoded))%4)
    target=DEST/("Mesh_%02d_Prim_%02d.glb"%(mesh_index,primitive_index))
    with target.open("wb") as f:
        f.write(struct.pack("<4sII",b"glTF",2,12+8+len(encoded)+8+len(output)))
        f.write(struct.pack("<II",len(encoded),0x4e4f534a));f.write(encoded)
        f.write(struct.pack("<II",len(output),0x4e4942));f.write(output)
    # Compare every original embedded texture byte-for-byte with its shard counterpart.
    texture_checks=[]
    for old_im,new_idx in im_map.items():
        orig=doc["bufferViews"][images[old_im]["bufferView"]]
        newv=new_views[new_images[new_idx]["bufferView"]]
        a=blob[orig.get("byteOffset",0):orig.get("byteOffset",0)+orig["byteLength"]]
        b=output[newv["byteOffset"]:newv["byteOffset"]+newv["byteLength"]]
        texture_checks.append({"image":images[old_im].get("name"),"original_sha256":hashlib.sha256(a).hexdigest(),
                               "shard_sha256":hashlib.sha256(b).hexdigest(),"bytes_identical":a==b})
    # Prove mesh indexes, vertex counts, material slots, morph target counts preserved.
    orig_mesh=copy.deepcopy(doc["meshes"][mesh_index])
    orig_mesh["primitives"]=[orig_mesh["primitives"][primitive_index]]
    native_counts=[{"vertex_count":doc["accessors"][p["attributes"]["POSITION"]]["count"],
                    "target_count":len(p.get("targets",[])),"material_id":p.get("material")}
                   for p in orig_mesh["primitives"]]
    rebuilt_counts=[{"vertex_count":new_access[p["attributes"]["POSITION"]]["count"],
                    "target_count":len(p.get("targets",[])),"material_id":used_mat[p.get("material")] if "material" in p else None}
                   for p in shard["meshes"][0]["primitives"]]
    equal=native_counts==rebuilt_counts
    return {"name":orig_mesh.get("name"),"mesh_index":mesh_index,"primitive_index":primitive_index,"source_primitive_counts":native_counts,
            "shard_primitive_counts":rebuilt_counts,"primitives_equal":equal,
            "material_ids":used_mat,"material_count":len(new_mats),"alpha_fixes":alpha_changes,
            "source_mesh_morph_target_names":orig_mesh.get("extras",{}).get("targetNames",[]),
            "image_count":len(new_images),"texture_sha256_checks":texture_checks,
            "source_image_payloads_preserved":all(x["bytes_identical"] for x in texture_checks),
            "full_asset_no_shape_target_reduction":equal,"path":str(target),"bytes":target.stat().st_size,
            "sha256":hashlib.sha256(target.read_bytes()).hexdigest()}
items=[process_mesh(i,j) for i in range(len(doc["meshes"])) for j in range(len(doc["meshes"][i]["primitives"]))]
assert all(x["primitives_equal"] and x["source_image_payloads_preserved"] for x in items)
assert all_mat_ids=={p["material"] for m in doc["meshes"] for p in m["primitives"] if "material" in p}
manifest={"status":"SOURCE_LOSSLESS_SHARDS_VERIFIED","original_glb_sha256":ORIGINAL_SHA,
          "full_mesh_count":len(doc["meshes"]),"full_primitive_count":sum(len(m["primitives"]) for m in doc["meshes"]),
          "shard_primitive_count":sum(len(x["shard_primitive_counts"]) for x in items),
          "full_morph_links":sum(len(p.get("targets",[])) for m in doc["meshes"] for p in m["primitives"]),
          "shard_morph_links":sum(c["target_count"] for x in items for c in x["shard_primitive_counts"]),
          "full_used_material_count":len(all_mat_ids),
          "all_required_source_materials_covered":True,
          "full_texture_images_from_source":len(image_props),
          "original_images":image_props,
          "shards":items}
assert manifest["full_primitive_count"]==manifest["shard_primitive_count"]
assert manifest["full_morph_links"]==manifest["shard_morph_links"]
(DEST/"manifest_full_fidelity_primitives.json").write_text(json.dumps(manifest,indent=2,ensure_ascii=False),encoding="utf8")
print("LOSSLESS",manifest["status"],"shards",len(items),"primitives",manifest["shard_primitive_count"],
      "morph_links",manifest["shard_morph_links"],"textures",len(image_props))
for item in items:
    print("PART",item["mesh_index"],item["name"],"bytes",item["bytes"],"prims",len(item["shard_primitive_counts"]),
          "morphs",sum(c["target_count"] for c in item["shard_primitive_counts"]),
          "textures",item["image_count"],"modes_fixed",len(item["alpha_fixes"]))
