"""Gate12: create per-skeleton non-destructive local-space skeletal animations.
All bone translations, scales, and base orientations copied exactly from each
native imported Unreal Skeleton reference pose. No root bone tracks. No C++.
"""
import unreal,math,json,pathlib,time,traceback
R=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu")
M=json.loads((R/"full_fidelity_primitive_shards/manifest_full_fidelity_primitives.json").read_text(encoding="utf8"))
OUT="/Game/Experimental/Gate12Koikatsu/Animation/NativeRestSafe"
report={"status":"STARTED","target_dir":OUT,"parts":[],"source_sha256":M["original_glb_sha256"]}
def write():
 (R/"rest_pose_sway_authored_report.json").write_text(json.dumps(report,indent=2,default=str),encoding="utf8")
write()
def quat_multiply(a,b):
 return (a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
         a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
         a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
         a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2])
def make_quat(v):
 norm=math.sqrt(sum(x*x for x in v))
 return unreal.Quat(*(x/norm for x in v))
def delta(axis,radians):
 s=math.sin(radians/2);c=math.cos(radians/2)
 return (axis[0]*s,axis[1]*s,axis[2]*s,c)
def add_motion(seq,sk,record):
 pose=sk.get_reference_pose()
 names=set(str(n) for n in pose.get_bone_names())
 targets={"spine_02":((0,1,0),4.0,0),
          "upperarm_l":((0,0,1),14.0,0),
          "upperarm_r":((0,0,1),-14.0,0),
          "lowerarm_l":((0,0,1),4.0,0),
          "lowerarm_r":((0,0,1),-4.0,0),
          "neck":((0,1,0),2.5,0)}
 controller=seq.get_editor_property("controller")
 controller.set_frame_rate(unreal.FrameRate(numerator=30,denominator=1),should_transact=False)
 controller.set_number_of_frames(unreal.FrameNumber(value=60),should_transact=False)
 controller.open_bracket("Author genuine native reference-pose KK animation",False)
 try:
  for name,(axis,amplitude,phase) in targets.items():
   if name not in names:continue
   ref=pose.get_bone_pose(unreal.Name(name),unreal.AnimPoseSpaces.LOCAL)
   q=ref.rotation;qq=(q.x,q.y,q.z,q.w)
   keys_pos=[];keys_rot=[];keys_scale=[]
   for f in range(61):
    theta=math.radians(amplitude)*math.sin(2*math.pi*(f/60)+phase)
    q_new=make_quat(quat_multiply(qq,delta(axis,theta)))
    keys_pos.append(unreal.Vector(ref.translation.x,ref.translation.y,ref.translation.z))
    keys_rot.append(q_new)
    keys_scale.append(unreal.Vector(ref.scale3d.x,ref.scale3d.y,ref.scale3d.z))
   controller.add_bone_track(unreal.Name(name),False)
   res=controller.set_bone_track_keys(unreal.Name(name),keys_pos,keys_rot,keys_scale,False)
   if not res:raise RuntimeError("set_bone_track_keys returned false: "+name)
   record["tracks"].append(name)
 finally:controller.close_bracket(False)
 if len(record["tracks"])<2:raise RuntimeError("Expected motion bones missing on native skeleton")
 record["root_animation_tracks"]=0
 record["native_ref_pose_translation_samples"]={name:str(pose.get_bone_pose(unreal.Name(name),unreal.AnimPoseSpaces.LOCAL).translation) for name in ("Center","pelvis") if name in names}
 record["seconds"]=seq.get_play_length()
 record["num_frames"]=61
 record["ref_pose_source"]=sk.get_path_name()
 record["data_model_bone_tracks"]=[str(n) for n in controller.get_model_interface().get_bone_track_names()]
 record["tracks_verified"]=sorted(record["tracks"])==sorted(record["data_model_bone_tracks"])
 if not record["tracks_verified"]:raise RuntimeError("Native DataModel tracks do not match")
 if abs(record["seconds"]-2)>0.07:raise RuntimeError("Unexpected play duration")
 if not unreal.EditorAssetLibrary.save_loaded_asset(seq):raise RuntimeError("Could not persist native sequence")
 record["status"]="PASS"
try:
 if "OfflineGame_Gate12_Koikatsu_20261010" not in str(unreal.Paths.get_project_file_path()):
  raise RuntimeError("Not isolated worktree")
 tools=unreal.AssetToolsHelpers.get_asset_tools()
 count=0
 for part in M["shards"]:
  mesh=int(part["mesh_index"]);primitive=int(part["primitive_index"])
  if mesh in (4,5) and primitive!=0:continue
  folder="/Game/Experimental/Gate12Koikatsu/FullFidelity/Mesh%02d"%mesh if mesh in (4,5) else "/Game/Experimental/Gate12Koikatsu/FullPrimitives/Mesh%02dPrim%02d"%(mesh,primitive)
  meshes=[]
  for path in unreal.EditorAssetLibrary.list_assets(folder,recursive=True,include_folder=False):
   asset=unreal.load_asset(path)
   if isinstance(asset,unreal.SkeletalMesh):meshes.append(asset)
  if len(meshes)!=1:raise RuntimeError("Expected one actual SkeletalMesh: "+folder)
  sk=meshes[0].get_editor_property("skeleton")
  name="AN_KK_NativeRestSafe_M%02d_P%02d"%(mesh,primitive)
  path=OUT+"/"+name
  record={"mesh_index":mesh,"primitive_index":primitive,"mesh":meshes[0].get_path_name(),
          "skeleton":sk.get_path_name(),"sequence":path,"tracks":[],"status":"STARTED"}
  report["parts"].append(record);write()
  if unreal.EditorAssetLibrary.does_asset_exist(path):raise RuntimeError("Refusing overwrite "+path)
  factory=unreal.AnimSequenceFactory()
  factory.set_editor_property("target_skeleton",sk)
  seq=tools.create_asset(name,OUT,unreal.AnimSequence,factory)
  if not seq:raise RuntimeError("Could not construct new AnimSequence for "+path)
  add_motion(seq,sk,record)
  count+=1;write()
 report["unique_native_sequences"]=count
 report["status"]="PASS" if count==19 else "FAIL"
except Exception as e:
 report["status"]="FAIL";report["error"]=str(e);report["trace"]=traceback.format_exc()[-2200:]
finally:
 report["completed_utc"]=time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())
 write();unreal.log("G12_NATIVE_RESTSAFE_SEQUENCE_AUTHOR_"+report["status"])
