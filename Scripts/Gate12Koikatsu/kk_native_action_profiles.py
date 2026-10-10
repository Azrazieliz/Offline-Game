"""Deterministic in-place UE source-rig action poses for one-skin Koikatsu fixture.

These are engineering placeholder motion profiles, NOT imported Koikatsu keyframes,
motion capture or a visual gameplay PASS. No root displacement; CharacterMovement
continues to own actor translation. Original rig rest transforms are retained.
"""
from dataclasses import dataclass
import math

FPS=30
@dataclass(frozen=True)
class Action:
    name:str
    frames:int
    looping:bool
    semantic:str

ACTIONS=(
    Action("Walk",30,True,"reciprocal gait"),
    Action("Run",24,True,"faster deeper reciprocal gait"),
    Action("Jump",30,False,"takeoff and arm lift"),
    Action("Fall",30,True,"airborne held fall"),
    Action("Land",18,False,"leg compression and recovery"),
    Action("TurnLeft",24,True,"left foot pivot and torso rotation"),
    Action("TurnRight",24,True,"right foot pivot and torso rotation"),
    Action("Dodge",24,False,"lateral evasion pose"),
    Action("Action",30,False,"one-arm interaction/attack gesture"),
)
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
TWO_PI=2.0*math.pi

def action(name):
    return next(a for a in ACTIONS if a.name==name)

def angles_degrees(name, frame):
    """Return local delta degrees keyed by exact bone name (native ref pose + delta)."""
    a=action(name)
    if not 0<=frame<=a.frames:
        raise ValueError("frame outside action")
    t=frame/a.frames
    cyc=math.sin(TWO_PI*t)
    opposite=-cyc
    # Positive knees bend toward the leg's own local positive rotation axis.
    bend_l=max(0.0,cyc)
    bend_r=max(0.0,opposite)
    if name=="Walk":
        return {"thigh_l":22*cyc,"thigh_r":22*opposite,
                "calf_l":25*bend_l,"calf_r":25*bend_r,
                "Left ankle":-9*cyc,"Right ankle":-9*opposite,
                "upperarm_l":-13*cyc,"upperarm_r":13*cyc,
                "spine_02":2.5*math.sin(TWO_PI*t*2)}
    if name=="Run":
        return {"thigh_l":43*cyc,"thigh_r":43*opposite,
                "calf_l":48*bend_l,"calf_r":48*bend_r,
                "Left ankle":-16*cyc,"Right ankle":-16*opposite,
                "upperarm_l":-29*cyc,"upperarm_r":29*cyc,
                "lowerarm_l":16,"lowerarm_r":-16,
                "spine_02":-8+2.5*math.sin(TWO_PI*t*2)}
    if name=="Jump":
        envelope=math.sin(math.pi*t)
        return {"thigh_l":-18*envelope,"thigh_r":-18*envelope,
                "calf_l":23*envelope,"calf_r":23*envelope,
                "upperarm_l":-48*envelope,"upperarm_r":48*envelope,
                "spine_02":-9*envelope}
    if name=="Fall":
        envelope=min(1.0,t*5)
        return {"thigh_l":10*envelope,"thigh_r":10*envelope,
                "calf_l":18*envelope,"calf_r":18*envelope,
                "upperarm_l":-18*envelope,"upperarm_r":18*envelope,
                "spine_02":7*envelope}
    if name=="Land":
        compression=math.sin(math.pi*t)**2
        return {"thigh_l":25*compression,"thigh_r":25*compression,
                "calf_l":35*compression,"calf_r":35*compression,
                "spine_02":17*compression,"upperarm_l":8*compression,
                "upperarm_r":-8*compression}
    if name in ("TurnLeft","TurnRight"):
        d=1 if name=="TurnLeft" else -1
        pivot=math.sin(TWO_PI*t)
        return {"spine_03":d*17*pivot,"neck":d*-5*pivot,
                "thigh_l":d*7*pivot,"thigh_r":d*-7*pivot,
                "Left ankle":d*-7*pivot,"Right ankle":d*7*pivot}
    if name=="Dodge":
        sweep=math.sin(math.pi*t)
        return {"spine_03":-25*sweep,"spine_02":13*sweep,
                "thigh_l":-30*sweep,"thigh_r":18*sweep,
                "calf_l":12*sweep,"calf_r":32*sweep,
                "upperarm_l":22*sweep,"upperarm_r":-28*sweep}
    if name=="Action":
        swing=math.sin(math.pi*t)
        return {"upperarm_r":-75*swing,"lowerarm_r":-38*swing,
                "upperarm_l":8*swing,"spine_03":16*swing,"neck":-4*swing}
    raise ValueError("Unknown action "+name)

def validate():
    assert len({a.name for a in ACTIONS})==len(ACTIONS)
    assert not ROOT_NAMES.intersection(AXES)
    signatures={}
    for a in ACTIONS:
        assert a.frames>=15
        poses=[angles_degrees(a.name,f) for f in range(a.frames+1)]
        assert all(k in AXES and math.isfinite(deg) for p in poses for k,deg in p.items())
        signature=tuple(tuple(round(p.get(b,0),3) for b in sorted(AXES)) for p in poses)
        assert signature not in signatures.values(), "Duplicate action "+a.name
        signatures[a.name]=signature
    assert angles_degrees("Walk",0)["thigh_l"]==0
    assert abs(angles_degrees("Run",6)["thigh_l"])>abs(angles_degrees("Walk",6)["thigh_l"])
    assert angles_degrees("TurnLeft",6)["spine_03"]==-angles_degrees("TurnRight",6)["spine_03"]
    return {"status":"PASS_STATIC_DISTINCT_PROCEDURAL_ACTION_DEFINITIONS",
            "action_count":len(ACTIONS),"names":[a.name for a in ACTIONS],
            "not_native_UE_authored":True,"not_android_validated":True}

if __name__=="__main__":
    import json
    print(json.dumps(validate(),indent=2))
