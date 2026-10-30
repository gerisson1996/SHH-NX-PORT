"""shh-nx (2026-10-29, docs/perfil-shh.md SHH-09): GPRs, code size and scratch of pixel shaders as uam compiles them
for the console (uam.exe from tools/deko3d_build_tools.sh: CompileBest, as uam_api), with and without the 32-GPR cap
of the gpr32 pass (constant 910 = 1).

    python tools/deko3d_gpr_report.py <glsl dir> <work dir> HASH [HASH...]
"""
import os
import struct
import subprocess
import sys

UAM = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'out', 'deko3d_tools', 'uam-build', 'uam.exe')


def compile_stats(glsl_path, out_path):
    r = subprocess.run([UAM, '-s', 'frag', '-o', out_path, glsl_path], capture_output=True, text=True)
    if r.returncode != 0 or not os.path.isfile(out_path):
        return None
    data = open(out_path, 'rb').read()
    magic, header_sz, control_sz, code_sz, programs_off, num_programs = struct.unpack_from('<6I', data, 0)
    _type, _entry, gprs, _cb_off, _cb_sz, scratch = struct.unpack_from('<6I', data, programs_off)
    return gprs, code_sz, scratch


def main():
    glsl_dir, work = sys.argv[1], sys.argv[2]
    os.makedirs(work, exist_ok=True)
    print('%-18s %5s %7s %7s | %5s %7s %7s' % ('shader', 'gprs', 'code', 'scratch', 'cap32', 'code', 'scratch'))
    for h in sys.argv[3:]:
        src = os.path.join(glsl_dir, 'ps_%s.glsl' % h)
        text = open(src, encoding='utf-8').read()
        base = compile_stats(src, os.path.join(work, h + '.dksh'))
        lines = text.split('\n', 1)
        capped_path = os.path.join(work, h + '_cap.glsl')
        with open(capped_path, 'w', encoding='utf-8') as f:
            f.write(lines[0] + '\n#define SPIRV_CROSS_CONSTANT_ID_910 1u\n' + lines[1])
        cap = compile_stats(capped_path, os.path.join(work, h + '_cap.dksh'))
        fmt = lambda s: ('%5d %7d %7d' % s) if s else '  failed'
        print('%-18s %s | %s' % (h, fmt(base), fmt(cap)))


if __name__ == '__main__':
    main()
