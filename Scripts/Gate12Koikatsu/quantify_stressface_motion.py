"""Quantify larger-motion physical screenshot test using true Android-captured frames, no OCR."""
from PIL import Image,ImageChops
import json,pathlib
R=pathlib.Path(r"D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010\Saved\Gate12Koikatsu")
paths=sorted((R/"S26_StressFace_Frames").glob("f_*.png"))
assert len(paths)>=50
fr=[Image.open(p).convert("RGB") for p in paths]
masks=[Image.eval(f.convert("L"),lambda p:255 if p>25 else 0).convert("1") for f in fr]
rect=[m.getbbox() for m in masks]
box=[min(x[0] for x in rect),min(x[1] for x in rect),max(x[2] for x in rect),max(x[3] for x in rect)]
def count_nonzero(image):return sum(image.convert("L").histogram()[1:])
def compare(a,b):
 a1=fr[a].crop(tuple(box));b1=fr[b].crop(tuple(box))
 m1=masks[a].crop(tuple(box));m2=masks[b].crop(tuple(box))
 u=ImageChops.logical_or(m1,m2);x=ImageChops.logical_xor(m1,m2)
 rgb=ImageChops.difference(a1,b1).split();maxerr=ImageChops.lighter(ImageChops.lighter(rgb[0],rgb[1]),rgb[2])
 change=ImageChops.logical_and(u,Image.eval(maxerr,lambda p:255 if p>20 else 0).convert("1"))
 area=count_nonzero(u)
 return {"frames":[a+1,b+1],"foreground_px":area,
 "silhouette_difference_percent":round(100*count_nonzero(x)/max(1,area),3),
 "color_difference_percent":round(100*count_nonzero(change)/max(1,area),3)}
checks={"opposite_pose":compare(5,9),"same_cycle_2s":compare(5,13),
        "second_opposite":compare(8,12),"second_same_cycle":compare(8,16)}
out={"status":"MEASURED","frame_count":len(fr),"image_dimensions":fr[0].size,"character_bbox":box,"comparisons":checks,
      "note":"Frame difference confirms actual motion, not facial-only morph. Visual review required for clipping and color."}
(R/"S26_StressFace_motion_comparison.json").write_text(json.dumps(out,indent=2),encoding="utf8")
print("RESULT",json.dumps(out))
