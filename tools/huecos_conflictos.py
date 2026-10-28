#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Removes from app/huecos.toml the gaps that split a real function in two.

Some gaps that tools/huecos.py finds are not separate functions: they are blocks of the
function before them (switch cases, the rest of a loop) that the analysis did not reach.
Declaring one as a function cuts its parent, and the codegen then warns about branches
that cross the cut:

    Unresolved conditional branch to 0xA from 0xB
    Unresolved b target 0xA from 0xB
    Unresolved function 0xA from 0xB
    Jump target 0xA unresolved at bctr 0xB
    bdnz at B branches outside function to A

Every gap start that lies between the two addresses of such a warning is one of those cuts.
When no gap lies between them, the gap right before both (within CERCA bytes) is blamed: the
function made from it got the wrong bounds.
This script takes them out of app/huecos.toml and adds them to tools/huecos_excluir.txt, so
tools/huecos.py leaves them out next time. Run the codegen again afterwards, and repeat
until no warning points at a gap.

Usage:
    python tools/huecos_conflictos.py [--log app/codegen.log] [--toml app/huecos.toml]
"""
import argparse
import bisect
import re

PATRONES = [
    re.compile(r"Unresolved conditional branch to 0x([0-9A-F]{8}) from 0x([0-9A-F]{8})"),
    re.compile(r"Unresolved b target 0x([0-9A-F]{8}) from 0x([0-9A-F]{8})"),
    re.compile(r"Unresolved function 0x([0-9A-F]{8}) from 0x([0-9A-F]{8})"),
    re.compile(r"Jump target 0x([0-9A-F]{8}) unresolved at bctr 0x([0-9A-F]{8})"),
    re.compile(r"bdnz at ([0-9A-F]{8}) branches outside function to ([0-9A-F]{8})"),
]
# Gaps that are not code at all: data embedded in .text (an empty function that returns silently
# if called) and the import thunks (declaring one hides the import, and the link fails).
NO_CODIGO = [
    re.compile(r"Function 0x([0-9A-F]{8}) has no blocks - generating stub"),
    re.compile(r"\[functions\] 0x([0-9A-F]{8}) outranked by import"),
]
CERCA = 256  # bytes: how far a warning may be from the gap before it and still be blamed on it
ENTRADA = re.compile(r'^"0x([0-9A-Fa-f]{8})"\s*=')


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--log", default="app/codegen.log")
    p.add_argument("--toml", default="app/huecos.toml")
    p.add_argument("--excluir", default="tools/huecos_excluir.txt")
    args = p.parse_args()

    lineas = open(args.toml, encoding="utf-8").read().splitlines()
    huecos = sorted(int(m.group(1), 16) for l in lineas if (m := ENTRADA.match(l)))

    pares = set()
    no_codigo = set()
    for linea in open(args.log, encoding="utf-8", errors="replace"):
        for patron in NO_CODIGO:
            m = patron.search(linea)
            if m:
                no_codigo.add(int(m.group(1), 16))
        for patron in PATRONES:
            m = patron.search(linea)
            if m:
                a, b = int(m.group(1), 16), int(m.group(2), 16)
                pares.add((min(a, b), max(a, b)))
                break

    culpables = no_codigo & set(huecos)
    sin_hueco = 0
    for lo, hi in pares:
        i = bisect.bisect_right(huecos, lo)
        j = bisect.bisect_right(huecos, hi)
        if i < j:
            culpables.update(huecos[i:j])
        elif i > 0 and lo - huecos[i - 1] <= CERCA:
            # Both ends inside the function made from the gap just before them: the analysis
            # gave it the wrong bounds, so the gap is not a function start either.
            culpables.add(huecos[i - 1])
        else:
            sin_hueco += 1

    quedan = [l for l in lineas
              if not ((m := ENTRADA.match(l)) and int(m.group(1), 16) in culpables)]
    with open(args.toml, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(quedan) + "\n")

    with open(args.excluir, "a", encoding="utf-8", newline="\n") as fh:
        for a in sorted(culpables):
            motivo = "no es codigo" if a in no_codigo else "parte una funcion"
            fh.write("0x%08X  # %s (codegen.log)\n" % (a, motivo))

    print("%d avisos de salto; %d huecos culpables quitados de %s; %d avisos sin hueco entre medias"
          % (len(pares), len(culpables), args.toml, sin_hueco))
    print("quedan %d huecos" % (len(huecos) - len(culpables)))


if __name__ == "__main__":
    main()
