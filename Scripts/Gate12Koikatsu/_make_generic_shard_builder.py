"""Build a reusable command-line version of the independently tested source sharder."""
from pathlib import Path
src=Path(__file__).with_name("build_lossless_primitive_shards.py")
dst=Path(__file__).with_name("build_kk_export_generic.py")
s=src.read_text(encoding="utf8")
s=s.replace('import os,json,struct,hashlib,io,copy,pathlib,collections','import os,json,struct,hashlib,io,copy,pathlib,collections,argparse,re')
start=s.index('SOURCE=pathlib.Path(')
end=s.index('\nmagic,ver,actual_len=',start)
head='''PARSER=argparse.ArgumentParser(description="Lossless per-primitive Koikatsu GLB extraction; strict one-skin input")
PARSER.add_argument("--source-glb",required=True,type=pathlib.Path)
PARSER.add_argument("--character-id",required=True)
PARSER.add_argument("--output-root",required=True,type=pathlib.Path)
PARSER.add_argument("--expected-sha256",default="")
PARSER.add_argument("--inspect-only",action="store_true")
args=PARSER.parse_args()
if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_-]{2,63}",args.character_id):
    raise ValueError("Unsafe character identifier")
SOURCE=args.source_glb.resolve(strict=True)
if SOURCE.suffix.lower()!=".glb" or not SOURCE.is_file():
    raise ValueError("Not an existing .glb regular file")
ROOT=args.output_root.resolve()
DEST=ROOT/args.character_id/"shards"
if ROOT==SOURCE.parent or ROOT in SOURCE.parents:
    raise ValueError("Output root must be separate from original export directory")
if DEST.exists() and not args.inspect_only:
    raise FileExistsError("Refusing to reuse or overwrite "+str(DEST))
data=SOURCE.read_bytes()
ORIGINAL_SHA=hashlib.sha256(data).hexdigest()
if args.expected_sha256:
    if not re.fullmatch(r"[0-9a-fA-F]{64}",args.expected_sha256):
        raise ValueError("Malformed expected SHA256")
    if ORIGINAL_SHA!=args.expected_sha256.lower():
        raise RuntimeError("Original GLB sha256 mismatch")
if len(data)<20 or data[:4]!=b"glTF":
    raise ValueError("Not glTF binary")
'''
s=s[:start]+head+s[end:]
s=s.replace('assert len(doc["meshes"])==6 and len(doc["skins"])==1',
'''if len(doc.get("skins",[]))!=1:
    raise ValueError("This proven sharder supports exactly one skin; refusing multi-skin export")
if not doc.get("meshes"):
    raise ValueError("No meshes")
if args.inspect_only:
    print(json.dumps({"status":"INSPECT_ONLY","sha256":ORIGINAL_SHA,
        "character_id":args.character_id,"mesh_count":len(doc["meshes"]),
        "primitive_count":sum(len(m["primitives"]) for m in doc["meshes"]),
        "source_morph_links":sum(len(p.get("targets",[])) for m in doc["meshes"] for p in m["primitives"]),
        "source_materials":len(doc.get("materials",[])),
        "skin_count":len(doc["skins"]),"source_bytes":len(data)},indent=2))
    raise SystemExit(0)
DEST.mkdir(exist_ok=False,parents=True)''')
s=s.replace('manifest={"status":"SOURCE_LOSSLESS_SHARDS_VERIFIED","original_glb_sha256":ORIGINAL_SHA,',
'''manifest={"status":"SOURCE_LOSSLESS_SHARDS_VERIFIED","character_id":args.character_id,
          "source_glb_path":str(SOURCE),"pipeline_version":"generic-1",
          "original_glb_sha256":ORIGINAL_SHA,''')
if 'SOURCE=pathlib.Path(' in s or 'len(doc["meshes"])==6' in s:
    raise RuntimeError("Failed to remove hardcoded fixture source")
dst.write_text(s,encoding="utf8")
print("GENERATED",dst,"BYTES",dst.stat().st_size)
