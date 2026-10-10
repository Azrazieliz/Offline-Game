"""Gate 12 UE5.8.3 host preflight. Use measured FREE RAM, not a hard 16GB-PC ban.

Profiles:
  probe  — one material read-only test; guard during process at >=850 MiB.
  repair — incremental material repair; guard during process at >=850 MiB.
Full Android cook is NOT authorized by either profile.
"""
import argparse
import ctypes
import json
import pathlib
import platform
import socket
import subprocess
import sys
import time

GIB=1024**3
MIB=1024**2
MIN_TOTAL_BYTES=0  # A single 8 GB device is allowed. Test available RAM instead.
MIN_FREE_BYTES=1900*MIB  # Legacy alias, probe-only; not a full-cook threshold.
MIN_REPAIR_FREE_BYTES=3400*MIB
LOW_RAM_STOP_BYTES=850*MIB
PROFILE_LIMITS={"probe":MIN_FREE_BYTES,"repair":MIN_REPAIR_FREE_BYTES}
EXPECTED_BRANCH="production/gate12-koikatsu-ue583-fixture-20261010"
EXPECTED_FOUNDATION="3f54f07cacbf193e160d0be15a7be5a7bc730c3b"

class MemoryStatus(ctypes.Structure):
    _fields_=[("dwLength",ctypes.c_ulong),("dwMemoryLoad",ctypes.c_ulong),
              ("ullTotalPhys",ctypes.c_ulonglong),("ullAvailPhys",ctypes.c_ulonglong),
              ("ullTotalPageFile",ctypes.c_ulonglong),("ullAvailPageFile",ctypes.c_ulonglong),
              ("ullTotalVirtual",ctypes.c_ulonglong),("ullAvailVirtual",ctypes.c_ulonglong),
              ("ullAvailExtendedVirtual",ctypes.c_ulonglong)]

def physical_memory():
    if platform.system()!="Windows":raise RuntimeError("Windows UE host required")
    value=MemoryStatus();value.dwLength=ctypes.sizeof(MemoryStatus)
    fn=ctypes.WinDLL("kernel32",use_last_error=True).GlobalMemoryStatusEx
    fn.argtypes=[ctypes.POINTER(MemoryStatus)];fn.restype=ctypes.c_int
    if not fn(ctypes.byref(value)):
        raise OSError(ctypes.get_last_error(),"GlobalMemoryStatusEx failed")
    return int(value.ullTotalPhys),int(value.ullAvailPhys)

def git(root,*args):
    return subprocess.check_output(["git","-C",str(root),*args],
                                   text=True,stderr=subprocess.PIPE).strip()

def perform(project,foundation,engine,profile="probe"):
    if profile not in PROFILE_LIMITS:raise ValueError("Invalid profile")
    r={"status":"BLOCKED","profile":profile,"host":socket.gethostname(),
       "timestamp_utc":time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime()),
       "native_editor_started":False,
       "min_available_gib":round(PROFILE_LIMITS[profile]/GIB,3),
       "runtime_stop_below_gib":round(LOW_RAM_STOP_BYTES/GIB,3),
       "observed_total_gib":None,"observed_available_gib":None,"details":[]}
    try:
        total,free=physical_memory()
        r["observed_total_gib"]=round(total/GIB,3)
        r["observed_available_gib"]=round(free/GIB,3)
        if free<PROFILE_LIMITS[profile]:
            r["status"]="BLOCKED_STARTING_FREE_RAM"
            r["details"].append("Other tests/source work can continue; only this native profile is deferred")
            return r
        if not (project/"OfflineGame.uproject").is_file():
            raise RuntimeError("Missing isolated .uproject")
        if project.name!="OfflineGame_Gate12_Koikatsu_20261010":
            raise RuntimeError("Wrong isolated project")
        if git(project,"branch","--show-current")!=EXPECTED_BRANCH:
            raise RuntimeError("Wrong experimental branch")
        if git(foundation,"rev-parse","HEAD")!=EXPECTED_FOUNDATION:
            raise RuntimeError("Frozen Foundation drift")
        if git(foundation,"status","--porcelain"):
            raise RuntimeError("Frozen Foundation dirty")
        if not (engine/"Engine/Binaries/Win64/UnrealEditor-Cmd.exe").is_file():
            raise RuntimeError("Unreal Editor commandlet absent")
        version_file=engine/"Engine/Build/Build.version"
        if not version_file.is_file():raise RuntimeError("Build.version absent")
        version=json.loads(version_file.read_text(encoding="utf-8-sig"))
        r["engine_version"]={k:version.get(k) for k in ("MajorVersion","MinorVersion","PatchVersion")}
        if tuple(r["engine_version"].values())!=(5,8,3):
            raise RuntimeError("UE must be exactly 5.8.3")
        r["status"]="PASS_PROFILE_PREFLIGHT_ONLY_NOT_NATIVE_PASS"
        r["details"].append("Unreal process requires independent RAM/time watchdog")
    except Exception as exc:
        r["status"]="BLOCKED_PREFLIGHT_ERROR"
        r["details"].append(str(exc))
    return r

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--project",type=pathlib.Path,required=True)
    p.add_argument("--foundation",type=pathlib.Path,required=True)
    p.add_argument("--engine",type=pathlib.Path,required=True)
    p.add_argument("--profile",choices=sorted(PROFILE_LIMITS),default="probe")
    p.add_argument("--receipt",type=pathlib.Path)
    a=p.parse_args()
    result=perform(a.project.resolve(),a.foundation.resolve(),a.engine.resolve(),a.profile)
    dest=a.receipt or a.project/"Saved/Gate12Koikatsu/native_shader_host_preflight.json"
    dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(result))
    return 0 if result["status"]=="PASS_PROFILE_PREFLIGHT_ONLY_NOT_NATIVE_PASS" else 10

if __name__=="__main__":
    sys.exit(main())
