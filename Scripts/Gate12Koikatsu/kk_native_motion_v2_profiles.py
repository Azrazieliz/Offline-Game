"""Post-freeze controlled Koikatsu v2 gait/swim *engineering* profiles.

Full source rig preserved and no root translation. Improved bounded reciprocal
gait, repeatable loop endpoints and swim-specific poses. These are NOT final
licensed motion-capture or visually approved animations. Fidelity must be
assessed on a physical device before approval.
"""
from dataclasses import dataclass
import math

FPS=30
TWO_PI=math.tau
AXES={
    "thigh_l":(0,1,0),"thigh_r":(0,1,0),
    "calf_l":(0,1,0),"calf_r":(0,1,0),
    "Left ankle":(0,1,0),"Right ankle":(0,1,0),
    "upperarm_l":(0,1,0),"upperarm_r":(0,1,0),
    "lowerarm_l":(0,1,0),"lowerarm_r":(0,1,0),
    "spine_02":(0,1,0),"spine_03":(0,0,1),
    "neck":(0,0,1),
}
ROOT_NAMES=frozenset(("Center","pelvis","Pelvis","Armature ChikaHaruno"))

@dataclass(frozen=True)
class Action:
    name: str
    frames: int
    looping: bool
    semantic: str

ACTIONS=(
    Action("Walk",30,True,"bounded 12-degree stride and arm counter-swing"),
    Action("Run",24,True,"bounded 24-degree stride and calf recovery"),
    Action("Swim",36,True,"bounded alternating crawl motion, independent of ground locomotion"),
    Action("SwimIdle",54,True,"relaxed, low-amplitude water treading posture"),
)
def action(name):
    for a in ACTIONS:
        if a.name==name:return a
    raise ValueError("Unsupported v2 motion "+name)

def angles_degrees(name,frame):
    a=action(name)
    if not isinstance(frame,int) or frame<0 or frame>a.frames:
        raise ValueError("V2 frame outside motion")
    t=frame/a.frames
    sinus=math.sin(TWO_PI*t)
    reciprocal=-sinus
    bend_l=max(0.0,math.sin(TWO_PI*t+0.65))**2
    bend_r=max(0.0,math.sin(TWO_PI*t+math.pi+0.65))**2
    bob=math.sin(2*TWO_PI*t)
    if name=="Walk":
        return {
          "thigh_l":12*sinus,"thigh_r":12*reciprocal,
          "calf_l":17*bend_l,"calf_r":17*bend_r,
          "Left ankle":-4.0*sinus,"Right ankle":-4.0*reciprocal,
          "upperarm_l":-8*sinus,"upperarm_r":8*sinus,
          "lowerarm_l":-3.0*sinus,"lowerarm_r":3.0*sinus,
          "spine_02":0.9*bob,"spine_03":0.4*sinus,"neck":-0.3*sinus}
    if name=="Run":
        return {
          "thigh_l":24*sinus,"thigh_r":24*reciprocal,
          "calf_l":28*bend_l,"calf_r":28*bend_r,
          "Left ankle":-7*sinus,"Right ankle":-7*reciprocal,
          "upperarm_l":-17*sinus,"upperarm_r":17*sinus,
          "lowerarm_l":9+5*sinus,"lowerarm_r":-9-5*sinus,
          "spine_02":-2.5+1.5*bob,"spine_03":0.8*sinus,"neck":-0.45*sinus}
    if name=="Swim":
        # Deliberately non-final in-place crawl approximation; a real swim
        # retarget is needed for fully horizontal body orientation.
        return {
          "thigh_l":9*sinus-3,"thigh_r":9*reciprocal-3,
          "calf_l":9*bend_l,"calf_r":9*bend_r,
          "Left ankle":-3*sinus,"Right ankle":-3*reciprocal,
          "upperarm_l":-24*sinus,"upperarm_r":24*sinus,
          "lowerarm_l":16*max(0.0,sinus)**2,"lowerarm_r":16*max(0.0,reciprocal)**2,
          "spine_02":-18+2.2*bob,"spine_03":2.1*sinus,"neck":-1.2*sinus}
    if name=="SwimIdle":
        return {
          "thigh_l":3.5*sinus,"thigh_r":3.5*reciprocal,
          "calf_l":5*bend_l,"calf_r":5*bend_r,
          "Left ankle":-2.0*sinus,"Right ankle":-2.0*reciprocal,
          "upperarm_l":-7*sinus,"upperarm_r":7*sinus,
          "lowerarm_l":4*max(0.0,sinus),"lowerarm_r":4*max(0.0,reciprocal),
          "spine_02":-7+1.3*bob,"spine_03":0.8*sinus,"neck":-0.4*sinus}
    raise AssertionError(name)

def validate():
    assert len(ACTIONS)==4 and len(set(a.name for a in ACTIONS))==4
    assert not ROOT_NAMES.intersection(AXES)
    bound={"Walk":18,"Run":29,"Swim":25,"SwimIdle":9}
    for a in ACTIONS:
        samples=[angles_degrees(a.name,f) for f in range(a.frames+1)]
        assert all(set(x).issubset(AXES) for x in samples)
        assert all(math.isfinite(v) for pose in samples for v in pose.values())
        assert all(abs(v)<=bound[a.name] for pose in samples for v in pose.values())
        assert all(abs(samples[0][k]-samples[-1][k])<1e-6 for k in samples[0]),"Loop seam pop: "+a.name
        assert all(abs((samples[1][k]-samples[0][k])-(samples[-1][k]-samples[-2][k]))<1.6 for k in samples[0]),"Non-matching seam velocity: "+a.name
    assert max(abs(angles_degrees("Walk",f)["thigh_l"]) for f in range(31))<=12
    assert max(abs(angles_degrees("Run",f)["thigh_l"]) for f in range(25))<=24
    return {"status":"PASS_STATIC_V2_BOUNDED_LOOP_SEAMS","action_count":len(ACTIONS),"actions":[a.name for a in ACTIONS],
            "full_fidelity_source_untouched":True,"real_device_quality":"NOT_TESTED",
            "postfreeze_not_final_locmotion":True}

if __name__=="__main__":
    import json
    print(json.dumps(validate(),indent=2))
