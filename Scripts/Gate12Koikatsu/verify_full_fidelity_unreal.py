"""UE 5.8.3 Python commandlet: authoritative all-source fidelity acceptance.
Compares real native imported SkeletalMesh morph counts and texture dimensions to
full-source GLB, no placeholders and no reduced assets accepted.
"""
import unreal,json,os,re,time,traceback,struct,pathlib
R=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu")
m=json.loads((R/"full_fidelity_primitive_shards/manifest_full_fidelity_primitives.json").read_text(encoding="utf8"))
effective_source=json.loads((R/"effective_morph_source_audit.json").read_text(encoding="utf8"))
effective_counts={(int(x["mesh"]),int(x["primitive"])):int(x["summary"].get("effective_position",0)) for x in effective_source}
out={"status":"STARTED","source_sha256":m["original_glb_sha256"],
     "expected_primitives":m["full_primitive_count"],"expected_morph_links":m["full_morph_links"],
     "parts":[],"project":str(unreal.Paths.get_project_file_path()),"started":time.time()}
def write():
 (R/"full_fidelity_native_audit.json").write_text(json.dumps(out,indent=2,default=str),encoding="utf8")
write()
try:
 if "OfflineGame_Gate12_Koikatsu_20261010" not in out["project"]:raise RuntimeError("Wrong project")
 for src in m["shards"]:
  mesh_i=int(src["mesh_index"]);prim_i=int(src["primitive_index"])
  if mesh_i in (4,5):
   folder=("/Game/Experimental/Gate12Koikatsu/FullFidelity/Mesh%02d"%mesh_i)
  else:
   folder="/Game/Experimental/Gate12Koikatsu/FullPrimitives/Mesh%02dPrim%02d"%(mesh_i,prim_i)
  part={"mesh_index":mesh_i,"primitive_index":prim_i,"asset_folder":folder,"expected_morph_count":src["shard_primitive_counts"][0]["target_count"],
        "expected_material_count":len(src["material_ids"]),"expect_image_hash_checks":all(x["bytes_identical"] for x in src["texture_sha256_checks"])}
  assets=[]
  for path in unreal.EditorAssetLibrary.list_assets(folder,recursive=True,include_folder=False):
   a=unreal.load_asset(path)
   if not a:continue
   kind=a.get_class().get_name()
   row={"path":str(path),"class":kind}
   if isinstance(a,unreal.SkeletalMesh):
    row["morphs"]=len(a.get_editor_property("morph_targets"))
    row["morph_names"]=[x.get_name() for x in a.get_editor_property("morph_targets")]
    sk=a.get_editor_property("skeleton")
    row["skeleton_path"]=sk.get_path_name() if sk else None
    row["bone_count"]=len(sk.get_editor_property("bone_tree")) if sk else None
    mats=a.get_editor_property("materials")
    row["material_slots"]=[{"name":str(s.get_editor_property("material_slot_name")),
                            "material":s.get_editor_property("material_interface").get_path_name() if s.get_editor_property("material_interface") else None} for s in mats]
   if kind in ("MaterialInstanceConstant","Material"):
    try:row["blend_mode"]=str(a.get_blend_mode())
    except:pass
    try:
     parameters=a.get_editor_property("texture_parameter_values")
     row["textures_bound"]=[{"name":str(x.get_editor_property("parameter_info").get_editor_property("name")),
        "asset":x.get_editor_property("parameter_value").get_path_name() if x.get_editor_property("parameter_value") else None} for x in parameters]
    except:pass
   if isinstance(a,unreal.Texture2D):
    try:row["resolution"]=[a.blueprint_get_size_x(),a.blueprint_get_size_y()]
    except Exception as e:row["resolution_error"]=str(e)
   assets.append(row)
  part["assets"]=assets
  meshes=[x for x in assets if x["class"]=="SkeletalMesh"]
  expected=int(src["shard_primitive_counts"][0]["target_count"])
  part["native_mesh_found"]=len(meshes)==1
  required_effective=(sum(v for (mk,pk),v in effective_counts.items() if mk==mesh_i) if mesh_i in (4,5) else effective_counts[(mesh_i,prim_i)])
  part["expected_nonzero_displacement_morph_count"]=required_effective
  part["native_morph_pass"]=len(meshes)==1 and meshes[0]["morphs"]==required_effective
  part["native_material_slots"]=len(meshes[0]["material_slots"]) if len(meshes)==1 else None
  part["material_pass"]=len(meshes)==1 and part["native_material_slots"]==(1 if mesh_i in (4,5) and prim_i!=0 else len(src["material_ids"]))
  # Full mesh 4/5 has respectively 2 and 6 slots, and all primitive shards share that same imported mesh.
  if mesh_i in (4,5):part["material_pass"]=len(meshes)==1 and part["native_material_slots"]==sum(1 for x in m["shards"] if x["mesh_index"]==mesh_i)
  part["native_texture_count"]=sum(1 for x in assets if x["class"]=="Texture2D")
  part["all_textures_have_positive_resolution"]=all(x.get("resolution",[0,0])[0]>0 and x.get("resolution",[0,0])[1]>0 for x in assets if x["class"]=="Texture2D")
  expected_source_img=[x["image"] for shard in (m["shards"] if mesh_i in (4,5) else [src]) for x in shard["texture_sha256_checks"] if mesh_i not in (4,5) or shard["mesh_index"]==mesh_i]
  original_dims={re.sub(r"[^0-9a-z]","",v["name"].lower()):[v["width"],v["height"]] for v in m["original_images"].values()}
  actual_textures={re.sub(r"[^0-9a-z]","",pathlib.Path(t["path"].split(".")[-1]).name.lower()):t.get("resolution") for t in assets if t["class"]=="Texture2D"}
  target_dims={re.sub(r"[^0-9a-z]","",name.lower()):original_dims.get(re.sub(r"[^0-9a-z]","",name.lower())) for name in expected_source_img}
  part["texture_dimension_validation"]=[{"texture":key,"expected":value,"native":actual_textures.get(key),"pass":value is not None and actual_textures.get(key)==value} for key,value in target_dims.items()]
  part["full_texture_resolution_pass"]=all(x["pass"] for x in part["texture_dimension_validation"]) and all(x["native"] for x in part["texture_dimension_validation"])
  part["native_acceptance"]="PASS" if part["native_mesh_found"] and part["native_morph_pass"] and part["material_pass"] and part["full_texture_resolution_pass"] else "FAIL"
  out["parts"].append(part);write()
 out["total_native_skeletal_meshes"]=len({x["asset_folder"] for x in out["parts"]})
 out["total_native_material_slots"]=sum(p.get("native_material_slots",0) for p in out["parts"] if p["mesh_index"]<4)+sum(out["parts"][next(i for i,x in enumerate(out["parts"]) if x["mesh_index"]==mi)]["native_material_slots"] for mi in (4,5))
 out["status"]="PASS" if all(x["native_acceptance"]=="PASS" for x in out["parts"]) and len(out["parts"])==m["full_primitive_count"] else "FAIL"
except Exception as e:out["status"]="ERROR";out["error"]=str(e);out["trace"]=traceback.format_exc()[-1500:]
finally:out["finished"]=time.time();write();unreal.log("G12_FULL_FIDELITY_NATIVE_AUDIT_"+out["status"])
