"""Build a configurable copy of the independently UE-5.8.3-validated Blueprint assembly script."""
from pathlib import Path
src=Path(__file__).with_name("author_playable_character_blueprints.py")
dst=Path(__file__).with_name("author_kk_playable_from_descriptor.py")
s=src.read_text(encoding="utf8")
s=s.replace("import unreal,json,pathlib,traceback,time","import unreal,json,pathlib,traceback,time,os,re")
a=s.index('R=pathlib.Path(r')
b=s.index('o={"status":"STARTED"',a)
preamble='''if "KK_BLUEPRINT_DESCRIPTOR" not in os.environ:
 raise RuntimeError("Require KK_BLUEPRINT_DESCRIPTOR env var to a prior native validated descriptor")
D=pathlib.Path(os.environ["KK_BLUEPRINT_DESCRIPTOR"]).resolve(strict=True)
BASE=json.loads(D.read_text(encoding="utf8"))
if BASE.get("schema_version")!="KK_UNREAL_CHARACTER_DESCRIPTOR_1":
 raise RuntimeError("Wrong descriptor contract version")
CID=BASE.get("character_id","")
if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_-]{2,63}",CID):
 raise RuntimeError("Unsafe content ID")
SLUG=CID.replace("-","_")
R=D.parent
DIR="/Game/Experimental/KoikatsuConverted/"+SLUG+"/PlayableQA"
PLAYER=DIR+"/BP_KoikatsuPlayable_"+SLUG
GM=DIR+"/BP_KoikatsuWorldMode_"+SLUG
WORLD=DIR+"/Maps/L_KoikatsuPlayable_"+SLUG
SOURCE_MAP=os.environ.get("KK_SOURCE_MAP","/Game/Maps/StartingWorld")
REPORT=R/(SLUG+"_playable_assembly.json")
'''
s=s[:a]+preamble+s[b:]
s=s.replace('def save(): (R/"playable_blueprint_authoring_report.json").write_text','def save(): REPORT.write_text')
s=s.replace('if BASE["status"]!="PASS" or len(BASE["parts"])!=19:raise RuntimeError("Missing already-verified original native skeleton data")',
'''if BASE["status"]!="PASS" or len(BASE["parts"])!=BASE["native_part_count"]:
  raise RuntimeError("Missing native acceptance for all part slots")''')
s=s.replace('if part["mesh_index"] in (0,1,2):comp.set_editor_property("hidden_in_game",True)',
            'if not part.get("visible",True):comp.set_editor_property("hidden_in_game",True)')
s=s.replace('"disabled_optional_effect":part["mesh_index"] in (0,1,2)',
            '"disabled_optional_effect":not part.get("visible",True)')
s=s.replace('if len(o["parts"])!=19:raise RuntimeError("Incomplete blueprint")',
            'if len(o["parts"])!=BASE["native_part_count"]:raise RuntimeError("Incomplete blueprint")')
s=s.replace('if obj.get_name().lower() in ("mesh","character_mesh0","prototypebody"):',
            'if obj.get_name().lower() in ("mesh","character_mesh0","prototypebody"):')
start=s.index(' if not unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP,WORLD):')
end=s.index(' o["status"]="PASS_BLUEPRINT_ASSEMBLED_PENDING_COOK_AND_ANDROID"',start)
s=s[:start]+''' if os.environ.get("KK_DUPLICATE_WORLD","0")=="1":
  if not unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP,WORLD):
   raise RuntimeError("Could not clone source level into isolated test")
  world=unreal.EditorLoadingAndSavingUtils.load_map(WORLD)
  world.get_world_settings().set_editor_property("default_game_mode",gm_cls)
  if not unreal.EditorLoadingAndSavingUtils.save_map(world,WORLD):
   raise RuntimeError("QA map did not save")
  o["actual_playable_map_saved"]=True
 else:
  o["actual_playable_map_saved"]=False
 '''+s[end:]
s=s.replace('o={"status":"STARTED","player":PLAYER,"game_mode":GM,"world":WORLD,"parts":[],"run_type":"ENGINEERING_FIXTURE"}',
            'o={"status":"STARTED","player":PLAYER,"game_mode":GM,"world":WORLD,"parts":[],"run_type":"ENGINEERING_UNAPPROVED"}')
dst.write_text(s,encoding="utf8")
print("GENERATED_GENERIC_BP_AUTHOR",dst,len(s))
