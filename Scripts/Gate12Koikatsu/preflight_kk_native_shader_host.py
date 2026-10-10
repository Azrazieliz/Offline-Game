"""Read-only, fail-closed resource preflight for native UE 5.8.3 material repair.

No Unreal process is launched here. Native repair requires an independent explicit
approval and the same safety checks embedded in the UE Python script.
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

GIB = 1024 ** 3
MIN_TOTAL_BYTES = 15 * GIB   # ~16GB marketed installed memory
MIN_FREE_BYTES = 8 * GIB     # measured RAM free immediately before UE launch
EXPECTED_BRANCH = "production/gate12-koikatsu-ue583-fixture-20261010"
EXPECTED_FOUNDATION = "3f54f07cacbf193e160d0be15a7be5a7bc730c3b"


class MemoryStatus(ctypes.Structure):
    _fields_ = [("dwLength", ctypes.c_ulong),
                ("dwMemoryLoad", ctypes.c_ulong),
                ("ullTotalPhys", ctypes.c_ulonglong),
                ("ullAvailPhys", ctypes.c_ulonglong),
                ("ullTotalPageFile", ctypes.c_ulonglong),
                ("ullAvailPageFile", ctypes.c_ulonglong),
                ("ullTotalVirtual", ctypes.c_ulonglong),
                ("ullAvailVirtual", ctypes.c_ulonglong),
                ("ullAvailExtendedVirtual", ctypes.c_ulonglong)]


def physical_memory():
    if platform.system() != "Windows":
        raise RuntimeError("Windows native UE host required")
    value = MemoryStatus()
    value.dwLength = ctypes.sizeof(MemoryStatus)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    api = kernel.GlobalMemoryStatusEx
    api.argtypes = [ctypes.POINTER(MemoryStatus)]
    api.restype = ctypes.c_int
    if api(ctypes.byref(value)) == 0:
        raise OSError(ctypes.get_last_error(), "GlobalMemoryStatusEx failed")
    return int(value.ullTotalPhys), int(value.ullAvailPhys)


def git(root, *args):
    return subprocess.check_output(
        ["git", "-C", str(root), *args], text=True, stderr=subprocess.PIPE).strip()


def perform(project, foundation, engine):
    r = {"status": "BLOCKED", "host": socket.gethostname(),
         "timestamp_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
         "native_editor_started": False,
         "min_total_gib": MIN_TOTAL_BYTES / GIB,
         "min_available_gib": MIN_FREE_BYTES / GIB,
         "observed_total_gib": None, "observed_available_gib": None,
         "details": []}
    try:
        total, free = physical_memory()
        r["observed_total_gib"] = round(total / GIB, 3)
        r["observed_available_gib"] = round(free / GIB, 3)
        if total < MIN_TOTAL_BYTES or free < MIN_FREE_BYTES:
            r["status"] = "BLOCKED_INSUFFICIENT_PHYSICAL_RAM"
            r["details"].append("No native Unreal Editor launch is permitted")
            return r
        if not (project / "OfflineGame.uproject").is_file():
            raise RuntimeError("Missing isolated .uproject")
        if project.name != "OfflineGame_Gate12_Koikatsu_20261010":
            raise RuntimeError("Project root is not isolated Gate 12")
        if git(project, "branch", "--show-current") != EXPECTED_BRANCH:
            raise RuntimeError("Incorrect experimental branch")
        if git(foundation, "rev-parse", "HEAD") != EXPECTED_FOUNDATION:
            raise RuntimeError("Frozen Foundation commit differs")
        if git(foundation, "status", "--porcelain"):
            raise RuntimeError("Frozen Foundation is not clean")
        if not (engine / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe").is_file():
            raise RuntimeError("Exact Unreal editor path absent")
        version_file = engine / "Engine/Build/Build.version"
        if not version_file.is_file():
            raise RuntimeError("Engine Build.version missing")
        version = json.loads(version_file.read_text(encoding="utf-8-sig"))
        r["engine_version"] = {k: version.get(k)
                               for k in ("MajorVersion", "MinorVersion", "PatchVersion")}
        if (version.get("MajorVersion"), version.get("MinorVersion"),
                version.get("PatchVersion")) != (5, 8, 3):
            raise RuntimeError("UE version is not exactly 5.8.3")
        r["status"] = "PASS_HOST_READINESS_ONLY_NATIVE_NOT_RUN"
        r["details"].append("Requires explicitly approved native execution and in-process safety guard")
    except Exception as exc:
        r["status"] = "BLOCKED_PREFLIGHT_ERROR"
        r["details"].append(str(exc))
    return r


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--project", type=pathlib.Path, required=True)
    p.add_argument("--foundation", type=pathlib.Path, required=True)
    p.add_argument("--engine", type=pathlib.Path, required=True)
    p.add_argument("--receipt", type=pathlib.Path)
    a = p.parse_args()
    result = perform(a.project.resolve(), a.foundation.resolve(), a.engine.resolve())
    dest = a.receipt or a.project / "Saved/Gate12Koikatsu/native_shader_host_preflight.json"
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))
    return 0 if result["status"] == "PASS_HOST_READINESS_ONLY_NATIVE_NOT_RUN" else 10


if __name__ == "__main__":
    sys.exit(main())
