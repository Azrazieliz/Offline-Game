import json,struct,pathlib,copy,hashlib,io
from PIL import Image

SOURCE=pathlib.Path(r"D:\Tools\GameAssetPipeline\KoikatsuParty\characters\G01-KK-PIPELINE-TEST-0002\conversion\character_unreal_skeleton.glb")
DIR=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu")
data=SOURCE.read_bytes()
if data[:4]!=b"glTF":raise ValueError("invalid GLB")
off=12;chunks=[]
while off<len(data):
    sz,typ=struct.unpack_from("<II",data,off);off+=8
    chunks.append((typ,data[off:off+sz]));off+=sz
doc=json.loads(chunks[0][1])
binary=next(v for k,v in chunks if k==0x4e4942)
variants=[
    ("FaceBodyQA",[(3,[0,11])],3,256),
    ("FaceEyesQA",[(3,[0,1,4,5,7,8,9])],3,128),
    ("HairClothesQA",[(4,[0,1]),(5,[0,1,4,5])],1,128)
]
def build(label,parts,morph_limit,tex_size):
    mesh_map={};newmeshes=[];needed_accessors=set();old_mat_list=[]
    for meshidx,primids in parts:
        m=copy.deepcopy(doc["meshes"][meshidx])
        prims=[]
        names=m.get("extras",{}).get("targetNames",[])
        target_idx=[]
        for kw in ("eye_face.f00_egao_op","Eye 両目閉じ","eye_face.f00_winkl_op","Basis.001"):
            for i,n in enumerate(names):
                if n.casefold()==kw.casefold() and i not in target_idx:target_idx.append(i)
                if len(target_idx)>=morph_limit:break
            if len(target_idx)>=morph_limit:break
        for j in range(min(morph_limit,len(m["primitives"][0].get("targets",[])))):
            if len(target_idx) >= morph_limit: break
            if j not in target_idx: target_idx.append(j)
        target_idx=sorted(target_idx)[:morph_limit]
        for i in primids:
            p=copy.deepcopy(m["primitives"][i])
            if "indices" in p:needed_accessors.add(p["indices"])
            needed_accessors.update(p.get("attributes",{}).values())
            if "targets" in p:
                p["targets"]=[p["targets"][j] for j in target_idx]
                for target in p["targets"]:needed_accessors.update(target.values())
            if "material" in p:old_mat_list.append(p["material"])
            prims.append(p)
        m["primitives"]=prims
        if "weights" in m:m["weights"]=[m["weights"][i] for i in target_idx]
        if "extras" in m and "targetNames" in m["extras"]:m["extras"]["targetNames"]=[names[i] for i in target_idx]
        mesh_map[meshidx]=len(newmeshes);newmeshes.append(m)
    needed_accessors.add(doc["skins"][0]["inverseBindMatrices"])
    buffer_views=[];buffer=bytearray()
    bv_map={}
    def append_blob(b,template=None):
        while len(buffer)%4:buffer.append(0)
        offset=len(buffer);buffer.extend(b)
        info={"buffer":0,"byteOffset":offset,"byteLength":len(b)}
        if template:
            for attr in ("byteStride","target"):
                if attr in template:info[attr]=template[attr]
        buffer_views.append(info);return len(buffer_views)-1
    def remap_bv(index):
        if index in bv_map:return bv_map[index]
        orig=doc["bufferViews"][index]
        start=orig.get("byteOffset",0);end=start+orig["byteLength"]
        nid=append_blob(binary[start:end],orig)
        bv_map[index]=nid
        return nid
    access_map={};accessors=[]
    for ix in sorted(needed_accessors):
        a=copy.deepcopy(doc["accessors"][ix])
        if "bufferView" in a:a["bufferView"]=remap_bv(a["bufferView"])
        if "sparse" in a:
            for key in ("indices","values"):a["sparse"][key]["bufferView"]=remap_bv(a["sparse"][key]["bufferView"])
        access_map[ix]=len(accessors);accessors.append(a)
    for m in newmeshes:
        for p in m["primitives"]:
            if "indices" in p:p["indices"]=access_map[p["indices"]]
            p["attributes"]={k:access_map[v] for k,v in p["attributes"].items()}
            for t in p.get("targets",[]):
                for k,v in list(t.items()):t[k]=access_map[v]
    old_material_ids=sorted(set(old_mat_list))
    materials=[];textures=[];images=[];samplers=[];texture_map={};sampler_map={};mat_map={}
    for oldmat in old_material_ids:
        m=copy.deepcopy(doc["materials"][oldmat])
        # Retain actual glTF alpha and original base-color texture, downscaled to avoid OOM.
        pbr=m.setdefault("pbrMetallicRoughness",{})
        if "baseColorTexture" in pbr:
            src_tex=pbr["baseColorTexture"]["index"]
            if src_tex not in texture_map:
                tex=copy.deepcopy(doc["textures"][src_tex])
                old_image=doc["images"][tex["source"]]
                bv=doc["bufferViews"][old_image["bufferView"]]
                pos=bv.get("byteOffset",0)
                im=Image.open(io.BytesIO(binary[pos:pos+bv["byteLength"]]))
                im.thumbnail((tex_size,tex_size))
                if im.mode not in ("RGB","RGBA"):im=im.convert("RGBA")
                png=io.BytesIO();im.save(png,format="PNG",optimize=True)
                image={"name":old_image.get("name"),"mimeType":"image/png","bufferView":append_blob(png.getvalue())}
                tex["source"]=len(images);images.append(image)
                if "sampler" in tex:
                    s=tex["sampler"]
                    if s not in sampler_map:
                        sampler_map[s]=len(samplers);samplers.append(copy.deepcopy(doc["samplers"][s]))
                    tex["sampler"]=sampler_map[s]
                texture_map[src_tex]=len(textures);textures.append(tex)
            pbr["baseColorTexture"]["index"]=texture_map[src_tex]
        mat_map[oldmat]=len(materials);materials.append(m)
    for m in newmeshes:
        for p in m["primitives"]:
            if "material" in p:p["material"]=mat_map[p["material"]]
    nodes=copy.deepcopy(doc["nodes"])
    selected_node_ids=[]
    for i,n in enumerate(nodes):
        if "mesh" in n:
            if n["mesh"] in mesh_map:
                n["mesh"]=mesh_map[n["mesh"]];selected_node_ids.append(i)
            else:n.pop("mesh",None);n.pop("skin",None)
    # skinned meshes are separated into scene-root siblings of armature
    # to prevent UE Interchange 'non-root skinned mesh parent transforms ignored'
    for n in nodes:
        if "children" in n:n["children"]=[c for c in n["children"] if c not in selected_node_ids]
    scenes=copy.deepcopy(doc["scenes"])
    for scene in scenes:
        scene["nodes"]=list(dict.fromkeys(scene.get("nodes",[])+selected_node_ids))
    skins=copy.deepcopy(doc["skins"])
    skins[0]["inverseBindMatrices"]=access_map[skins[0]["inverseBindMatrices"]]
    nd={"asset":doc["asset"],"scene":doc.get("scene",0),"scenes":scenes,"nodes":nodes,"skins":skins,"meshes":newmeshes,"accessors":accessors,"bufferViews":buffer_views,"buffers":[{"byteLength":len(buffer)}],"materials":materials,"textures":textures,"images":images,"samplers":samplers}
    if "extensionsUsed" in doc and "KHR_materials_unlit" in doc["extensionsUsed"]:
        # Keep extension declaration only if any retained material uses it.
        if any("KHR_materials_unlit" in m.get("extensions",{}) for m in materials):nd["extensionsUsed"]=["KHR_materials_unlit"]
    raw=json.dumps(nd,ensure_ascii=False,separators=(",",":")).encode("utf-8")
    raw+=b" " * ((-len(raw))%4)
    while len(buffer)%4:buffer.append(0)
    total=12+8+len(raw)+8+len(buffer)
    target=DIR/("character_"+label+".glb")
    with target.open("wb") as f:
        f.write(struct.pack("<4sII",b"glTF",2,total))
        f.write(struct.pack("<II",len(raw),0x4e4f534a));f.write(raw)
        f.write(struct.pack("<II",len(buffer),0x4e4942));f.write(buffer)
    item={"name":label,"path":str(target),"sha256":hashlib.sha256(target.read_bytes()).hexdigest(),"bytes":target.stat().st_size,"skin_joints":len(skins[0]["joints"]),"meshes":len(newmeshes),"primitives":sum(len(m["primitives"]) for m in newmeshes),"morph_targets_per_mesh":[len(m["primitives"][0].get("targets",[])) for m in newmeshes],"texture_count":len(images),"material_count":len(materials),"alpha_modes":[m.get("alphaMode","OPAQUE") for m in materials],"accessors":len(accessors),"bufferViews":len(buffer_views),"nodes":len(nodes),"scene_root_nodes":scenes[0]["nodes"]}
    return item
items=[build(*v) for v in variants]
(DIR/"split_glb_qa_manifest.json").write_text(json.dumps(items,indent=2),encoding="utf8")
for x in items:print(json.dumps(x,ensure_ascii=True))
