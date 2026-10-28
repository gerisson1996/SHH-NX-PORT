#!/usr/bin/env python3
"""Census of a code range of the generated C++ (the game's Direct3D library), to find the functions Renut hooks.

  python -I tools/d3d_censo.py [--gen app/generated/default] [--lo 828E0000] [--hi 82930000] [--fn 82911D68 ...]

For each function: instruction count, callers (from the whole module), callees, kernel imports it calls, whether
it touches GPU MMIO (lis rX,0x7FC8) and the device offsets (>= 1024) it reads or writes most through r3/r31.
The same idea as naruto-rise-of-a-ninja-nx's tools/d3d_census.py, written for this repository.
"""
import argparse
import collections
import glob
import os
import pickle
import re


def leer(gen):
    # Parsing ~1,000 generated files takes a minute: the result is kept next to them and reused while no file is
    # newer.
    cache = os.path.join(gen, ".d3d_censo.pickle")
    fuentes = glob.glob(os.path.join(gen, "*_recomp.*.cpp"))
    if os.path.exists(cache) and all(os.path.getmtime(p) <= os.path.getmtime(cache) for p in fuentes):
        with open(cache, "rb") as f:
            return pickle.load(f)
    funcs = leer_fuentes(gen)
    with open(cache, "wb") as f:
        pickle.dump(funcs, f)
    return funcs


def leer_fuentes(gen):
    funcs = {}
    actual = None
    for path in sorted(glob.glob(os.path.join(gen, "*_recomp.*.cpp"))):
        with open(path, errors="replace") as f:
            for line in f:
                m = re.match(r"DEFINE_REX_FUNC\(sub_([0-9A-F]+)\)", line)
                if m:
                    actual = int(m.group(1), 16)
                    funcs[actual] = dict(n=0, calls=[], imps=set(), mmio=False, offs=collections.Counter(), ins=[])
                    continue
                if actual is None:
                    continue
                if line.startswith("}"):
                    actual = None
                    continue
                fn = funcs[actual]
                if line.startswith("\t// "):
                    fn["n"] += 1
                    ins = line[4:].strip()
                    fn["ins"].append(ins)
                    m = re.match(r"bl 0x([0-9a-f]+)", ins)
                    if m:
                        fn["calls"].append(int(m.group(1), 16))
                    if re.match(r"lis r\d+,32712$", ins):
                        fn["mmio"] = True
                    m = re.match(r"(l|st)[a-z]*\s+r\d+,(\d+)\(r(31|3)\)$", ins)
                    if m and int(m.group(2)) >= 1024:
                        fn["offs"][int(m.group(2))] += 1
                m = re.search(r"__imp__([A-Za-z0-9_]+)\(", line)
                if m:
                    fn["imps"].add(m.group(1))
    return funcs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gen", default="app/generated/default")
    ap.add_argument("--lo", default="828E0000")
    ap.add_argument("--hi", default="82930000")
    ap.add_argument("--fn", nargs="*", default=[])
    ap.add_argument("--asm", action="store_true", help="also print the instructions of each selected function")
    ap.add_argument("--busca", help="regex: only functions with an instruction that matches")
    a = ap.parse_args()
    funcs = leer(a.gen)
    llamadores = collections.defaultdict(set)
    for fa, fn in funcs.items():
        for c in fn["calls"]:
            llamadores[c].add(fa)
    lo, hi = int(a.lo, 16), int(a.hi, 16)
    sel = [int(x, 16) for x in a.fn] or [x for x in sorted(funcs) if lo <= x < hi]
    busca = re.compile(a.busca) if a.busca else None
    for fa in sel:
        fn = funcs.get(fa)
        if not fn:
            print(f"{fa:08X}: no es una funcion")
            continue
        if busca and not any(busca.search(i) for i in fn["ins"]):
            continue
        cr = sorted(llamadores[fa])
        partes = [f"{fa:08X} n={fn['n']:4d} llamadores={len(cr):3d}"]
        if fn["mmio"]:
            partes.append("MMIO")
        if fn["imps"]:
            partes.append("imp=" + ",".join(sorted(fn["imps"])))
        cs = sorted(set(fn["calls"]))
        if cs:
            partes.append("llama=" + ",".join(f"{c:08X}" for c in cs[:12]) + ("..." if len(cs) > 12 else ""))
        if cr:
            partes.append("desde=" + ",".join(f"{c:08X}" for c in cr[:8]) + ("..." if len(cr) > 8 else ""))
        if fn["offs"]:
            partes.append("offs=" + ",".join(f"{o}x{n}" for o, n in fn["offs"].most_common(8)))
        print(" ".join(partes))
        if a.asm:
            for i in fn["ins"]:
                print("    " + i)


if __name__ == "__main__":
    main()
