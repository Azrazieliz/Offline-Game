"""Gate 12: exact-skeleton advanced arm/neck stress + native facial morph curve QA.
Creates NEW sequences in separate folder and QA maps; never overwrites proven clips.
All source geometry, morph deltas, material, texture bytes stay unchanged.
"""
import unreal,math,json,pathlib,time,traceback
R=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu")
BASE=json.loads((R/"rest_pose_sway_authored_report.json").read_text(encoding="utf8"))
BASEMAP="/Game/Experimental/Gate12Koikatsu/FullFidelityQA/Maps/L_KK_CompleteSource_UE583"
OUT="/Game/Experimental/Gate12Koikatsu/Animation/StressFaceQA"
MAP="/Game/Experimental/Gate12Koikatsu/FullFidelityQA/Maps/L_KK_StressFace_UE583"
r={"status":"STARTED","sequences":[],"map":MAP,"face_curves":[]}
def save(): (R/"facial_stress_authoring_report.json").write_text(json.dumps(r,indent=2,default=str),encoding="utf8")
def unitquat(q):
 k=math.sqrt(sum(v*v for v in q))
 return unreal.Quat(*(v/k for v in q))
def qmul(a,b):
 return (a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
         a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
         a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
         a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2])
save()
try:
 if BASE["status"]!="PASS" or len(BASE["parts"])!=19:raise RuntimeError("Original QA not validated")
 if "OfflineGame_Gate12_Koikatsu_20261010" not in str(unreal.Paths.get_project_file_path()):raise RuntimeError("wrong Unreal project")
 tools=unreal.AssetToolsHelpers.get_asset_tools()
 # Deliberately higher range than 14 degree prior test to challenge mesh/clothes:
 motions={"spine_02":((0,1,0),12,0), "upperarm_l":((0,0,1),36,0),
  "upperarm_r":((0,0,1),-36,0), "lowerarm_l":((0,1,0),22,0.8),
  "lowerarm_r":((0,1,0),-22,0.8), "neck":((0,1,0),9,0.3)}
 names={}
 for idx,entry in enumerate(BASE["parts"]):
  mesh=unreal.load_asset(entry["mesh"]);old=unreal.load_asset(entry["sequence"])
  if not isinstance(mesh,unreal.SkeletalMesh) or not isinstance(old,unreal.AnimSequence):raise RuntimeError("Source assets missing")
  sk=mesh.get_editor_property("skeleton")
  if sk.get_path_name()!=entry["skeleton"]:raise RuntimeError("Skeleton changed since baseline")
  name="AN_KK_StressFace_M%02d_P%02d"%(entry["mesh_index"],entry["primitive_index"])
  dest=OUT+"/"+name
  if unreal.EditorAssetLibrary.does_asset_exist(dest):
   clip=unreal.load_asset(dest)
  else:
   clip=unreal.EditorAssetLibrary.duplicate_asset(entry["sequence"],dest)
  if not isinstance(clip,unreal.AnimSequence):raise RuntimeError("Failed to duplicate source clip "+dest)
  controller=clip.get_editor_property("controller");pose=sk.get_reference_pose()
  available=set(map(str,pose.get_bone_names()))
  part={"mesh_index":entry["mesh_index"],"prim":entry["primitive_index"],
        "mesh":entry["mesh"],"skeleton":sk.get_path_name(),"animation":dest,"bones":[],"curves":[]}
  r["sequences"].append(part);save()
  # Preserve per-bone rest translations/scales; reference quaternion multiplied by small local-space test deltas.
  for bone,(axis,deg,phase) in motions.items():
   if bone not in available:continue
   ref=pose.get_bone_pose(unreal.Name(bone),unreal.AnimPoseSpaces.LOCAL)
   q=ref.rotation
   qref=(q.x,q.y,q.z,q.w)
   pkeys=[];rkeys=[];skeys=[]
   for f in range(61):
    angle=math.radians(deg)*math.sin(2*math.pi*f/60+phase)
    dq=(axis[0]*math.sin(angle/2),axis[1]*math.sin(angle/2),axis[2]*math.sin(angle/2),math.cos(angle/2))
    pkeys.append(unreal.Vector(ref.translation.x,ref.translation.y,ref.translation.z))
    rkeys.append(unitquat(qmul(qref,dq)))
    skeys.append(unreal.Vector(ref.scale3d.x,ref.scale3d.y,ref.scale3d.z))
   if not controller.set_bone_track_keys(unreal.Name(bone),pkeys,rkeys,skeys,False):
    raise RuntimeError("Could not set stress keys for "+bone)
   part["bones"].append(bone)
  # Use real available imported facial morphs only. Each of the face mesh sections gets its own exact target.
  allmorph=set(t.get_name() for t in mesh.get_editor_property("morph_targets"))
  desired=[]
  if "Mouth_Happy" in allmorph:desired.append(("Mouth_Happy","smile"))
  if "Mouth__Ah__Sound__L_" in allmorph:desired.append(("Mouth__Ah__Sound__L_","mouth_open"))
  if "Eyebrow_Surprised" in allmorph:desired.append(("Eyebrow_Surprised","brows"))
  for morph,kind in desired:
   curve=unreal.AnimationCurveIdentifier()
   curve.set_curve_identifier(unreal.Name(morph),unreal.RawCurveTrackTypes.RCT_FLOAT)
   if not controller.add_curve(curve,curve_flags=4,should_transact=False):
    raise RuntimeError("Could not create facial curve "+morph)
   keys=[]
   for f in range(61):
    time_s=f/30
    # Smile peaks at t=.5, mouth-open peaks at t=1.5, brows follow smile.
    wave=0.5-0.5*math.cos(2*math.pi*f/60)
    value=(0.85*wave if kind=="smile" else 0.70*(1-wave) if kind=="mouth_open" else 0.80*wave)
    keys.append(unreal.RichCurveKey(time=time_s,value=value))
   if not controller.set_curve_keys(curve,keys,False):raise RuntimeError("Could not set facial curve keys: "+morph)
   sk.set_curve_meta_data_morph_target(unreal.Name(morph),True)
   part["curves"].append(morph)
   r["face_curves"].append({"mesh_index":entry["mesh_index"],"prim":entry["primitive_index"],
                            "morph":morph,"kind":kind,"target_is_real_imported_morph":True})
  if desired and not unreal.EditorAssetLibrary.save_loaded_asset(sk):
   raise RuntimeError("Cannot save registered morph curve meta on skeleton")
  if not unreal.EditorAssetLibrary.save_loaded_asset(clip):
   raise RuntimeError("Could not save facial/stress clip: "+dest)
  part["duration"]=clip.get_play_length();part["root_tracks"]=0;part["status"]="PASS"
  names[entry["mesh"]]=dest;save()
 if len(r["sequences"])!=19 or not r["face_curves"]:raise RuntimeError("Incomplete authored assets or no face curves")
 # Duplicate independent complete full quality scene, assign 19 exact skeleton sequences and bring camera closer.
 if unreal.EditorAssetLibrary.does_asset_exist(MAP):raise RuntimeError("Refuse QA level overwrite")
 if not unreal.EditorAssetLibrary.duplicate_asset(BASEMAP,MAP):raise RuntimeError("QA map duplicate failed")
 world=unreal.EditorLoadingAndSavingUtils.load_map(MAP)
 if not world:raise RuntimeError("Cannot load stress+face QA map")
 actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SkeletalMeshActor)
 if len(actors)!=19:raise RuntimeError("Scene skeletal actor count mismatch")
 for a in actors:
  comp=a.skeletal_mesh_component
  path=comp.get_skeletal_mesh_asset().get_path_name()
  if path not in names:raise RuntimeError("Unknown mesh "+path)
  seq=unreal.load_asset(names[path])
  if seq.get_editor_property("skeleton").get_path_name()!=comp.get_skeletal_mesh_asset().get_editor_property("skeleton").get_path_name():raise RuntimeError("Exact skeleton mismatch")
  comp.override_animation_data(anim_to_play=seq,is_looping=True,is_playing=True,position=0.0,play_rate=1.0)
 cameras=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor)
 if len(cameras)!=1:raise RuntimeError("Expected 1 QA camera")
 # Close enough for face/torso inspection while showing skirt and arms; no mesh transforms altered.
 camera=cameras[0];r["camera_before"]=str(camera.get_actor_location())
 camera.set_actor_location(unreal.Vector(0,125,128),False,False)
 camera.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-90,roll=0),False)
 camera.set_editor_property("auto_activate_for_player",unreal.AutoReceiveInput.PLAYER0)
 r["camera_after"]=str(camera.get_actor_location())
 r["bound_meshes"]=len(actors);r["curves_count"]=len(r["face_curves"])
 if not unreal.EditorLoadingAndSavingUtils.save_map(world,MAP):raise RuntimeError("Failed save stressface map")
 r["status"]="PASS"
except Exception as e:r["status"]="FAIL";r["error"]=str(e);r["trace"]=traceback.format_exc()[-2300:]
finally:r["finished_utc"]=time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime());save();unreal.log("G12_STRESS_FACE_AUTHOR_"+r["status"])
