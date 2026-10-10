"""Actual physical S26 frame-to-frame skeletal motion: PIL-only diagnostics."""
from PIL import Image,ImageChops
import json,pathlib
R=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu")
files=sorted((R/"S26_LandscapeFrames").glob("f_*.png"))
assert len(files)>=40
frames=[Image.open(f).convert("RGB") for f in files]
masks=[Image.eval(f.convert("L"),lambda p:255 if p>30 else 0).convert("1") for f in frames]
bbox=[m.getbbox() for m in masks]
x1=min(x[0] for x in bbox if x);y1=min(x[1] for x in bbox if x)
x2=max(x[2] for x in bbox if x);y2=max(x[3] for x in bbox if x)
def npix(im):
 h=im.convert("L").histogram()
 return sum(h[1:])
def meas(i,j):
 a=frames[i].crop((x1,y1,x2,y2))
 b=frames[j].crop((x1,y1,x2,y2))
 m1=masks[i].crop((x1,y1,x2,y2));m2=masks[j].crop((x1,y1,x2,y2))
 un=ImageChops.logical_or(m1,m2);xor=ImageChops.logical_xor(m1,m2)
 diff=ImageChops.difference(a,b).split()
 mx=ImageChops.lighter(ImageChops.lighter(diff[0],diff[1]),diff[2])
 over=Image.eval(mx,lambda p:255 if p>20 else 0).convert("1")
 rgb=ImageChops.logical_and(over,un)
 n=npix(un)
 return {"frame_a":i+1,"frame_b":j+1,"t_a_sec":round(i/4,2),"t_b_sec":round(j/4,2),
         "foreground_pixels":n,"shape_changed_pixels":npix(xor),"shape_change_pct":round(100*npix(xor)/max(1,n),3),
         "rgb_changed_pixels":npix(rgb),"rgb_change_pct":round(100*npix(rgb)/max(1,n),3)}
report={"status":"MEASURED","frame_count":len(frames),"video_size":list(frames[0].size),
        "bbox":[x1,y1,x2,y2],"comparisons":{"quarter_cycle":meas(5,7),"opposite_pose":meas(5,9),
        "two_second_loop":meas(5,13),"another_opposite":meas(7,11),"another_loop":meas(10,18)}}
(R/"S26_NativeRestSway_frame_motion_metrics.json").write_text(json.dumps(report,indent=2),encoding="utf8")
print("STATUS",report["status"],"FRAMES",len(frames),"BBOX",report["bbox"])
for name,v in report["comparisons"].items():print(name,"SHAPE_CHANGED_PCT",v["shape_change_pct"],"RGB_CHANGED_PCT",v["rgb_change_pct"])
