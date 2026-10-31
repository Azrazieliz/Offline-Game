"""Convert ONLY PMX authenticated by the exact original CLOTHED Koikatsu card exporter.
Source cards and PMX are immutable; Blender outputs isolated, no invented geometry."""
import bpy,sys,pathlib,hashlib,json,time,traceback,addon_utils
FORM=sys.argv[sys.argv.index("--")+1]
assert FORM in {"Awakened","Berserker","Grand"}
PROJECT=pathlib.Path(r"D:\UnrealProjects\G12_LuminousNativeImport_20261031")
K=pathlib.Path(r"D:\Tools\GameAssetPipeline\KoikatsuParty")
CARDS=pathlib.Path(r"D:\UnrealProjects\Gate12_WholeCharacter_Build_20261031\Assets\NoWorld\Luminous\KoikatsuOriginalCards")
IN=PROJECT/"Saved/CorrectKoikatsuExports"
STATE={"Awakened":"AWAKENED_CLOTHED","Berserker":"BERSERKER_CLOTHED","Grand":"GRAND_CLOTHED"}[FORM]
CARD=CARDS/("LUMINOUS_"+("GRAND_CONVERGENCE" if FORM=="Grand" else FORM.upper())+"_CLOTHED_KK.png")
PROOF=IN/("Gate12_Card_"+STATE+"_Original_API_Export.json")
OUT=PROJECT/"AuthenticClothedOriginalCard3D"/FORM
OUT.mkdir(parents=True,exist_ok=True)
RECEIPT=OUT/"ORIGINAL_APPROVED_CARD_FULL_3D_BLENDER_PROOF.json"
report={"form":FORM,"original_card":str(CARD),"original_export_receipt":str(PROOF),"status":"STARTED","native_unreal_import_verified":False,"reference_visual_accepted":False}
def sha(path):
 h=hashlib.sha256()
 with pathlib.Path(path).open("rb") as fd:
  for block in iter(lambda:fd.read(4*1024*1024),b""):h.update(block)
 return h.hexdigest()
def save():
 RECEIPT.write_text(json.dumps(report,indent=2,default=str)+"\n","utf8")
save()
try:
 assert PROOF.is_file(), "Native game exporter did not finish this exact card"
 source=json.loads(PROOF.read_text("utf8"))
 assert source["state"]==STATE and source["status"]=="EXPORTED_NATIVE_EXACT_ORIGINAL_CARD"
 assert sha(CARD)==source["source_card_sha256"], "Approved card original hash has changed"
 pmx=pathlib.Path(source["pmx_path"])
 assert pmx.name=="model.pmx" and pmx.is_file() and sha(pmx)==source["pmx_sha256"]
 assert pmx.parent.parent.resolve()==pathlib.Path(r"D:\Tools\Steam\steamapps\common\Koikatsu Party\Export_PMX").resolve()
 report.update({"original_card_sha256":sha(CARD),"native_pmx_sha256":sha(pmx),"native_pmx_path":str(pmx),"true_card_to_export_api_verified":True})
 save()
 sys.path[:0]=[str(K/"addons/KKBP_v9.5.0/unpacked_mmd_tools"),
               str(K/"addons/KKBP_v9.5.0/unpacked_mmd_tools/opencc_dependency"),
               str(K/"addons/KKBP_v9.5.0/unpacked_importer")]
 for addon in ("mmd_tools","kkbp_importer"):addon_utils.enable(addon,default_set=True,persistent=False)
 cfg=bpy.context.scene.kkbp;cfg.import_dir=str(pmx.parent)
 cfg.shader_dropdown="E";cfg.use_rigify=False;cfg.sfw_mode=False
 cfg.fix_seams=True;cfg.use_outline=False;cfg.separate_clothes=False
 import kkbp_importer.common as common
 common.toggle_console=lambda:None
 start=time.monotonic()
 bpy.ops.kkbp.kkbpimport("EXEC_DEFAULT")
 arms=[o for o in bpy.data.objects if o.type=="ARMATURE"]
 meshes=sorted((o for o in bpy.data.objects if o.type=="MESH"),key=lambda o:o.name)
 report["source_geometry"]={"armatures":len(arms),"bones":sum(len(a.data.bones) for a in arms),
     "mesh_parts":len(meshes),"vertices":sum(len(m.data.vertices) for m in meshes),
     "shape_keys":sum(len(m.data.shape_keys.key_blocks)-1 for m in meshes if m.data.shape_keys),
     "materials":len(bpy.data.materials),"images":len(bpy.data.images),
     "import_seconds":round(time.monotonic()-start,2)}
 assert len(arms)==1 and len(meshes)>=3 and report["source_geometry"]["bones"]>700
 assert report["source_geometry"]["vertices"]>10000 and report["source_geometry"]["shape_keys"]>0
 print("GENUINE_CARD_BLENDER_IMPORTED",FORM,report["source_geometry"],flush=True)
 scene=OUT/("Luminous_"+FORM+"_ExactOriginalClothed_Rig.blend")
 assert not scene.exists(),"Refuse overwrite immutable original conversion"
 bpy.ops.wm.save_as_mainfile(filepath=str(scene),check_existing=False,compress=False)
 report["source_blend"]=str(scene);report["source_blend_sha256"]=sha(scene)
 report["status"]="PASS_EXACT_CARD_REAL_MESH_RIG_BLEND"
 save()
 for i,m in enumerate(meshes):
  # KKBP hides some REAL source primitives outside the active view layer.
  # Link those original objects to the render/import scene; no proxies.
  if m.name not in bpy.context.view_layer.objects:
   collection=bpy.data.collections.new("ExactSourceHiddenMesh_"+str(i))
   bpy.context.scene.collection.children.link(collection)
   collection.objects.link(m)
   bpy.context.view_layer.update()
  bpy.ops.object.select_all(action="DESELECT")
  arms[0].hide_set(False);m.hide_set(False)
  arms[0].select_set(True);m.select_set(True);bpy.context.view_layer.objects.active=arms[0]
  out=OUT/("Luminous_"+FORM+"_TrueOriginalPart_%02d.glb"%i)
  assert not out.exists()
  bpy.ops.export_scene.gltf(filepath=str(out),export_format="GLB",use_selection=True,
    export_apply=False,export_skins=True,export_morph=True,export_yup=True)
  assert out.stat().st_size>100000, "Source mesh GLB missing"
  row={"part":i,"original_mesh_name":m.name,"source_vertices":len(m.data.vertices),
       "source_morphs":len(m.data.shape_keys.key_blocks)-1 if m.data.shape_keys else 0,
       "source_bones":report["source_geometry"]["bones"],
       "source_glb_sha256":sha(out),"path":str(out),"bytes":out.stat().st_size}
  report.setdefault("native_unreal_part_sources",[]).append(row)
  report["status"]="PARTIAL_SOURCE_EXPORT"
  save()
  print("GENUINE_CARD_ORIGINAL_SHARD",FORM,i,len(m.data.vertices),flush=True)
 assert len(report["native_unreal_part_sources"])==len(meshes)
 report["status"]="PASS_EXACT_ORIGINAL_KOIKATSU_CARD_SIX_SOURCED_GLBS_PENDING_NATIVE_UE"
except Exception as e:
 report["status"]="FAILED_AUTHENTIC_CARD_CONVERSION"
 report["error"]=repr(e)
 report["trace"]=traceback.format_exc()[-5500:]
 print("GENUINE_CARD_IMPORT_FAILED",FORM,str(e),flush=True)
finally:
 save()
 print("ORIGINAL_CARD_3D_RESULT",FORM,report["status"],flush=True)
