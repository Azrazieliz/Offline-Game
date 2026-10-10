import unreal,json,pathlib,traceback
w=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010")
p="/Game/Experimental/Gate12Koikatsu/PlayableQA/BP_KoikatsuFixturePlayable"
result={"status":"STARTED","api":{}}
try:
 b=unreal.load_asset(p)
 c=unreal.EditorAssetLibrary.load_blueprint_class(p)
 n=unreal.load_class(None,"/Script/OfflineGame.OGKoikatsuPlayableCharacter")
 l=unreal.load_class(None,"/Script/OfflineGame.OGWorldPrototypeCharacter")
 result["found"]=[str(x) for x in (b,c,n,l)]
 result["function_names"]={k:hasattr(unreal,k) for k in ("get_type_from_class","get_default_object")}
 result["class_methods"]=[x for x in dir(c) if "super" in x.lower() or "parent" in x.lower() or "child" in x.lower()]
 result["blueprint_methods"]=[x for x in dir(unreal.BlueprintEditorLibrary) if "parent" in x.lower() or "reparent" in x.lower()]
 result["cdo"]=str(unreal.get_default_object(c))
 for q in ("parent_class","generated_class","skeleton_generated_class"):
  try:result["api"][q]=str(b.get_editor_property(q))
  except Exception as e:result["api"][q]="NOT_EXPOSED: "+str(e)[:140]
 if hasattr(unreal,"get_type_from_class"):
  for name,cls in (("native",n),("legacy",l)):
   try:
    typ=unreal.get_type_from_class(cls)
    result[name+"_pytype"]=str(typ)
    result[name+"_is_instance"]=isinstance(unreal.get_default_object(c),typ)
   except Exception as e:result[name+"_type_error"]=str(e)
 result["status"]="PASS_READ_ONLY_REFLECTION_INSPECTION"
except Exception as e:
 result["status"]="FAIL_READ_ONLY_REFLECTION";result["error"]=str(e);result["trace"]=traceback.format_exc()[-1300:]
finally:
 (w/"Saved/Gate12Koikatsu/NativeActionClips/native_reflection_api_probe.json").write_text(json.dumps(result,indent=2),encoding="utf8")
 unreal.log("G12_NATIVE_REFLECTION_"+result["status"])
