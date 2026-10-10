"""Pure Python audit: authoritative native UE .uasset counts + unchanged full GLB
and source-position deltas, with 0.025 mm non-displacement tolerance.
This is reporting only; it never edits or prunes the original meshes.
"""
import json,pathlib,time
R=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu")
native=json.loads((R/"full_fidelity_native_audit.json").read_text(encoding="utf8"))
m=json.loads((R/"full_fidelity_primitive_shards/manifest_full_fidelity_primitives.json").read_text(encoding="utf8"))
tol=json.loads((R/"native_morph_tolerance_report.json").read_text(encoding="utf8"))
rows={(x["mesh"],x["prim"]):x for x in tol["rows"]}
parts=[]
for entry in native["parts"]:
    key=(entry["mesh_index"],entry["primitive_index"])
    src=rows[key]
    target=src["by_threshold"]["2.5e-05"]
    if key[0] in (4,5):target=sum(v["by_threshold"]["2.5e-05"] for k,v in rows.items() if k[0]==key[0])
    sk=[x for x in entry["assets"] if x["class"]=="SkeletalMesh"]
    observed=sk[0]["morphs"] if len(sk)==1 else None
    morph_ok=observed==target
    mat_ok=entry["material_pass"]
    texture_ok=entry["full_texture_resolution_pass"]
    res={"mesh_index":key[0],"primitive_index":key[1],"expected_visual_morphs":target,
         "actual_native_morphs":observed,"total_original_morph_links":src["targets_source"],
         "full_resolution_textures":texture_ok,"material_slots":entry.get("native_material_slots"),
         "morphs_pass":morph_ok,"material_slots_pass":mat_ok,
         "native_acceptance":"PASS" if (morph_ok and mat_ok and texture_ok and entry["native_mesh_found"]) else "FAIL"}
    parts.append(res)
record={"status":"PASS" if len(parts)==25 and all(x["native_acceptance"]=="PASS" for x in parts) else "FAIL",
        "native_source":"full_fidelity_native_audit.json","source_original_glb_sha256":m["original_glb_sha256"],
        "tolerance_m":0.000025,"tolerance_mm":0.025,
        "note":"Importer's zero/tiny displacement suppression is not an input asset reduction; every source target remains in full GLB.",
        "original_primitives":m["full_primitive_count"],"original_morph_references":m["full_morph_links"],
        "native_unique_skeletal_meshes":native["total_native_skeletal_meshes"],
        "native_material_slots":native["total_native_material_slots"],
        "parts":parts,"checked_utc":time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())}
(R/"full_fidelity_native_reconciled_acceptance.json").write_text(json.dumps(record,indent=2),encoding="utf8")
print("G12_RECONCILED_NATIVE",record["status"],"PARTS",len(parts),"MESHA",record["native_unique_skeletal_meshes"],
      "MATERIAL_SLOTS",record["native_material_slots"],"FAILED",sum(x["native_acceptance"]!="PASS" for x in parts))
