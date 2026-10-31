#!/usr/bin/env python3
"""Reject any Luminous 3D export not proven to originate from the *exact*
approved Koikatsu card. No candidate matching by nickname, image or rig.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
ORIGINAL=ROOT/"Assets/NoWorld/Luminous/KoikatsuOriginalCards"
EXPORTS=Path(r"D:\UnrealProjects\G12_LuminousNativeImport_20261031\Saved\CorrectKoikatsuExports")
REQUIRED={
    "AWAKENED_CLOTHED":("LUMINOUS_AWAKENED_CLOTHED_KK.png","e6b53e82c0d87ecdbd0ff8bf510ce93a1d7e557b436382271fcfed6451897bd5"),
    "BERSERKER_CLOTHED":("LUMINOUS_BERSERKER_CLOTHED_KK.png","820307ce971382af9b620fa8610af60918ad01cc7267994ba9ac5cde96070862"),
    "GRAND_CLOTHED":("LUMINOUS_GRAND_CONVERGENCE_CLOTHED_KK.png","26c48e1c03c154f9f39a0b06afaade53423c11dbd079604e430162eb4cae5353"),
}
def sha(path: Path)->str:
    h=hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda:f.read(2**20),b""):
            h.update(chunk)
    return h.hexdigest()

def verify()->dict:
    result={"schema":"noworld.luminous.exact_canonical_card_to_kkbp.v1",
        "status":"NOT_ALL_SOURCE_EXPORTS_PROVEN","forms":{}}
    for state,(filename,expected) in REQUIRED.items():
        row={"source_filename":filename,"source_sha256":expected,"admitted":False}
        result["forms"][state]=row
        original=ORIGINAL/filename
        if not original.is_file() or sha(original)!=expected:
            row["failure"]="Approved original Koikatsu card is missing or mutated"
            continue
        receipt=EXPORTS/("Gate12_Card_"+state+"_Original_API_Export.json")
        if not receipt.is_file():
            row["failure"]="Native source export receipt absent; do not reuse historical candidates"
            continue
        try:
            data=json.loads(receipt.read_text("utf8"))
            p=Path(data["pmx_path"])
            if not (data["status"]=="EXPORTED_NATIVE_EXACT_ORIGINAL_CARD"
                    and data["state"]==state
                    and data["source_card_sha256"]==expected
                    and p.is_file()
                    and sha(p)==data["pmx_sha256"]
                    and p.stat().st_size>100000):
                raise ValueError("Native export identity, exact source card or PMX hash mismatched")
            row.update({"admitted":True,"pmx_path":str(p),"pmx_sha256":data["pmx_sha256"],"pmx_bytes":p.stat().st_size})
        except Exception as e:
            row["failure"]=str(e)
    if all(x["admitted"] for x in result["forms"].values()):
        result["status"]="PASS_THREE_EXACT_ORIGINAL_CARDS_NATIVE_PMX_PROVEN"
    return result

if __name__=="__main__":
    result=verify()
    print(json.dumps(result,indent=2))
    raise SystemExit(0 if result["status"].startswith("PASS") else 1)
