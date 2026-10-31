import unreal,os,json,hashlib,pathlib,traceback,time,gc
PROJECT=pathlib.Path(r"D:\UnrealProjects\G12_LuminousNativeImport_20261031")
SOURCES=PROJECT/"Sources"
FORM=os.environ.get("G12_NATIVE_FORM","")
assert FORM in ("Awakened","Berserker","Grand")
DIR=SOURCES/("Luminous_"+FORM)/"shards"
manifest=json.loads((DIR/"manifest_full_fidelity_primitives.json").read_text(encoding="utf8"))
assert manifest["status"]=="SOURCE_LOSSLESS_SHARDS_VERIFIED"
assert len(manifest["shards"])==6 and manifest["full_primitive_count"]==manifest["shard_primitive_count"]
assert manifest["full_morph_links"]==manifest["shard_morph_links"]
assert manifest["all_required_source_materials_covered"]
assert all(x["full_asset_no_shape_target_reduction"] and x["source_image_payloads_preserved"] for x in manifest["shards"])
assert "G12_LuminousNativeImport" in str(unreal.Paths.get_project_file_path())
DEST="/Game/NoWorld/Characters/Luminous/"+FORM+"/NativeSourceParts"
REPORT=PROJECT/"Saved"/("NativeShards_"+FORM+".json")
REPORT.parent.mkdir(exist_ok=True,parents=True)
report={"schema":"noworld.gate12.verified_sharded_native_import.v1","form":FORM,"status":"STARTED","source_glb_sha256":manifest["original_glb_sha256"],"source_primitive_count":manifest["full_primitive_count"],"source_morph_links":manifest["full_morph_links"],"real_native_ue_skeletal_assets":0,"parts":[],"output_package":DEST}
def save():REPORT.write_text(json.dumps(report,indent=2),encoding="utf8")
save()
try:
    for index,part in enumerate(manifest["shards"]):
        file=pathlib.Path(part["path"])
        assert file.is_file()
        with file.open("rb") as f:sha=hashlib.file_digest(f,"sha256").hexdigest()
        assert sha==part["sha256"],"Lossless original mesh shard hash mismatch"
        out=DEST+"/Part_"+str(index).zfill(2)
        if unreal.EditorAssetLibrary.does_directory_exist(out):
            raise RuntimeError("Refuse overwrite existing native import "+out)
        row={"index":index,"source_sha256":sha,"original_primitives":len(part["shard_primitive_counts"]),"morph_source_references":sum(x["target_count"] for x in part["shard_primitive_counts"]),"status":"IMPORTING","destination":out}
        report["parts"].append(row);save()
        task=unreal.AssetImportTask()
        task.filename=str(file);task.destination_path=out
        task.automated=True;task.async_=False;task.save=True;task.replace_existing=False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        del task
        paths=list(unreal.EditorAssetLibrary.list_assets(out,True,False))
        row["uassets"]=len(paths)
        if not paths:raise RuntimeError("UE importer produced zero UAssets for real source part "+str(index))
        row["package_paths"]=[str(x) for x in paths]
        # The pipeline must actually create skeletal meshes; resolve from asset
        # registry metadata without loading all 1000-bone objects into memory.
        registry=unreal.AssetRegistryHelpers.get_asset_registry()
        row["native_classes"]={}
        for asset in registry.get_assets_by_path(out,recursive=True):
            typ=str(asset.asset_class_path.asset_name)
            row["native_classes"][typ]=row["native_classes"].get(typ,0)+1
        count=row["native_classes"].get("SkeletalMesh",0)
        if not count:raise RuntimeError("Original skinned mesh did not produce Unreal SkeletalMesh in "+out)
        report["real_native_ue_skeletal_assets"]+=count
        row["status"]="PASS_SOURCE_BACKED_NATIVE_SKELETAL_IMPORT"
        unreal.log("G12_NATIVE_PART_SUCCESS "+FORM+" "+str(index)+" "+str(count))
        save()
        del paths,registry
        gc.collect()
    report["status"]="PASS_SIX_NATIVE_UNREAL_SOURCE_PARTS" if report["real_native_ue_skeletal_assets"]>=6 else "FAIL_NO_FULL_CHARACTER"
except Exception as error:
    report["status"]="PARTIAL_OR_FAILED"
    report["error"]=str(error);report["traceback"]=traceback.format_exc()[-5000:]
finally:
    report["finished_at"]=time.time()
    save()
    unreal.log("G12_NATIVE_"+FORM+"_"+report["status"])
