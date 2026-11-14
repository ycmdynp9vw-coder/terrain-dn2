"""Genera la patch di rilascio: dal .syx originale Elektron OS 1.12 e dalla nostra build, per ogni sezione cambiata una lista di
operazioni che ricostruiscono il contenuto nuovo copiando dal contenuto originale (dello strumento di chi costruisce) e
aggiungendo solo i nostri byte. Uso: python3 mkpatch.py ORIGINALE.syx BUILD.syx USCITA.json"""
import sys, os, json, hashlib, base64, pathlib
# dnfw (https://github.com/angellinares/dn2_firmware_explore) python package: set DNFW_SRC to its src/ folder.
DNFW = pathlib.Path(os.environ.get("DNFW_SRC") or pathlib.Path(__file__).resolve().parents[2] / "upstream/dn2_firmware_explore/src")
sys.path.insert(0, str(DNFW))
from dnfw.firmware.load import load

K = 8

def ops_for(a, b):
    idx = {}
    for i in range(len(a) - K + 1):
        idx.setdefault(a[i:i + K], i)
    ops, lit, i = [], bytearray(), 0
    def flush():
        if lit:
            ops.append(["l", base64.b64encode(bytes(lit)).decode()]); lit.clear()
    while i < len(b):
        j = None
        if i + K <= len(a) and a[i:i + K] == b[i:i + K]:
            j = i
        else:
            w = b[i:i + K]
            if len(w) == K and len(set(w)) > 1:
                j = idx.get(w)
        if j is None:
            lit.append(b[i]); i += 1; continue
        n = K
        while i + n < len(b) and j + n < len(a) and b[i + n] == a[j + n]:
            n += 1
        flush(); ops.append(["c", j, n]); i += n
    flush()
    return ops

def apply(a, ops):
    out = bytearray()
    for op in ops:
        if op[0] == "c": out += a[op[1]:op[1] + op[2]]
        else: out += base64.b64decode(op[1])
    return bytes(out)

def main(orig, build, dest):
    ro, rb = pathlib.Path(orig).read_bytes(), pathlib.Path(build).read_bytes()
    fo, fb = load(ro), load(rb)
    so = {s.id: s for s in fo.container.sections}
    patch = {"format": 1, "input_sha256": hashlib.sha256(ro).hexdigest(), "build_sha256": hashlib.sha256(rb).hexdigest(), "sections": {}}
    lit_tot = 0
    for s in fb.container.sections:
        o = so[s.id]
        ca, cb = o.unpack(), s.unpack()
        if ca is None or cb is None:
            assert o.stored == s.stored, f"sezione {s.id} grezza e cambiata"
            continue
        if ca == cb: continue
        ops = ops_for(ca, cb)
        assert apply(ca, ops) == cb
        lit = sum(len(base64.b64decode(op[1])) for op in ops if op[0] == "l")
        lit_tot += lit
        patch.setdefault("input_sections", {})[str(s.id)] = hashlib.sha256(ca).hexdigest()
        patch["sections"][str(s.id)] = {"length": len(cb), "sha256": hashlib.sha256(cb).hexdigest(), "ops": ops}
        print(f"sezione {s.id}: {len(cb)} byte, {len(ops)} operazioni, nostri {lit} byte")
    pathlib.Path(dest).write_text(json.dumps(patch, separators=(",", ":")))
    print("letterali totali", lit_tot, "file", pathlib.Path(dest).stat().st_size)

if __name__ == "__main__":
    main(*sys.argv[1:4])
