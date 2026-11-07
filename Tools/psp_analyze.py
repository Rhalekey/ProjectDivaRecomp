"""Reconhecimento de um ELF de PSP: secoes, simbolos, imports/exports e instrucoes.

Uso: python psp_analyze.py <elf> [--json saida.json]
"""
import argparse
import json
import re
import struct
from collections import Counter

# Opcodes primarios do Allegrex que pertencem a VFPU.
VFPU_PRIMARY = {
    0x12: "cop2 (mfv/mtv/bvf/bvt)",
    0x18: "vfpu0 (vadd/vsub/vdiv...)",
    0x19: "vfpu1 (vmul/vdot/vscl...)",
    0x1B: "vfpu3 (vcmp/vmin/vmax...)",
    0x32: "lv.s",
    0x34: "vfpu4 (vmov/vsqrt/vrot...)",
    0x35: "lvl.q/lvr.q",
    0x36: "lv.q",
    0x37: "vfpu5 (vpfx/viim/vfim)",
    0x3A: "sv.s",
    0x3C: "vfpu6 (vmmul/vtfm/vhdp...)",
    0x3D: "svl.q/svr.q",
    0x3E: "sv.q",
    0x3F: "vfpu7 (vnop/vsync/vflush)",
}
PRIMARY_NAMES = {
    0x00: "special", 0x01: "regimm", 0x02: "j", 0x03: "jal", 0x04: "beq", 0x05: "bne",
    0x06: "blez", 0x07: "bgtz", 0x08: "addi", 0x09: "addiu", 0x0A: "slti", 0x0B: "sltiu",
    0x0C: "andi", 0x0D: "ori", 0x0E: "xori", 0x0F: "lui", 0x10: "cop0", 0x11: "cop1",
    0x14: "beql", 0x15: "bnel", 0x16: "blezl", 0x17: "bgtzl", 0x1C: "special2",
    0x1F: "special3", 0x20: "lb", 0x21: "lh", 0x22: "lwl", 0x23: "lw", 0x24: "lbu",
    0x25: "lhu", 0x26: "lwr", 0x28: "sb", 0x29: "sh", 0x2A: "swl", 0x2B: "sw",
    0x2E: "swr", 0x2F: "cache", 0x30: "ll", 0x31: "lwc1", 0x38: "sc", 0x39: "swc1",
}


class Image:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        d = self.d
        if d[:4] != b"\x7fELF":
            raise SystemExit("not an ELF")
        h = struct.unpack_from("<HHIIIIIHHHHHH", d, 16)
        self.entry, self.phoff, self.shoff = h[3], h[4], h[5]
        self.phentsize, self.phnum, self.shentsize, self.shnum, self.shstrndx = h[8:13]
        self.segs = [
            struct.unpack_from("<8I", d, self.phoff + i * self.phentsize)
            for i in range(self.phnum)
        ]

    def v2o(self, va):
        for p_type, off, vaddr, _, filesz, *_ in self.segs:
            if p_type == 1 and vaddr <= va < vaddr + filesz:
                return va - vaddr + off
        return None

    def cstr(self, va):
        o = self.v2o(va)
        if o is None:
            return None
        return self.d[o:self.d.index(b"\0", o)].decode("latin1")

    def u32(self, va):
        return struct.unpack_from("<I", self.d, self.v2o(va))[0]

    def sections(self):
        d = self.d
        raw = [
            struct.unpack_from("<10I", d, self.shoff + i * self.shentsize)
            for i in range(self.shnum)
        ]
        stroff = raw[self.shstrndx][4] if raw else 0
        out = []
        for name, typ, flags, addr, off, size, link, info, align, entsize in raw:
            end = d.index(b"\0", stroff + name)
            out.append({
                "name": d[stroff + name:end].decode("latin1"), "type": typ, "flags": flags,
                "addr": addr, "off": off, "size": size, "link": link, "entsize": entsize,
            })
        return out


def read_module_info(img):
    off = img.segs[0][3] & 0x7FFFFFFF
    attr = struct.unpack_from("<H", img.d, off)[0]
    name = img.d[off + 4:off + 32].split(b"\0")[0].decode("latin1")
    gp, ent_top, ent_end, stub_top, stub_end = struct.unpack_from("<5I", img.d, off + 32)
    return dict(attr=attr, name=name, gp=gp, ent_top=ent_top, ent_end=ent_end,
                stub_top=stub_top, stub_end=stub_end)


def read_imports(img, mi):
    libs = []
    va = mi["stub_top"]
    while va < mi["stub_end"]:
        o = img.v2o(va)
        name_p, ver, attr, size, varc, funcc, nids_p, funcs_p = struct.unpack_from(
            "<IHHBBHII", img.d, o)
        if size == 0:
            raise SystemExit(f"entrada de import invalida em {va:#x}")
        libs.append({
            "lib": img.cstr(name_p) if name_p else None,
            "version": ver, "attr": attr, "vars": varc,
            "funcs": [
                {"nid": img.u32(nids_p + i * 4), "stub": funcs_p + i * 8}
                for i in range(funcc)
            ],
        })
        va += size * 4
    return libs


def read_exports(img, mi):
    out = []
    va = mi["ent_top"]
    while va < mi["ent_end"]:
        name_p, ver, attr, size, varc, funcc, tbl = struct.unpack_from("<IHHBBHI", img.d, img.v2o(va))
        if size == 0:
            break
        entries = [
            {
                "nid": img.u32(tbl + i * 4),
                "addr": img.u32(tbl + (funcc + varc + i) * 4),
                "kind": "func" if i < funcc else "var",
            }
            for i in range(funcc + varc)
        ]
        out.append({"lib": img.cstr(name_p) if name_p else None, "entries": entries})
        va += size * 4
    return out


def scan_code(img, sec, mi):
    """Varre linearmente uma secao de codigo e conta instrucoes e fluxo de controle."""
    prim, vfpu, flow = Counter(), Counter(), Counter()
    call_targets = set()
    data = img.d[sec["off"]:sec["off"] + sec["size"]]
    start, end = sec["addr"], sec["addr"] + sec["size"]
    out_of_range = 0
    for i in range(0, len(data) - 3, 4):
        w = struct.unpack_from("<I", data, i)[0]
        op = w >> 26
        pc = start + i
        if op in VFPU_PRIMARY:
            vfpu[VFPU_PRIMARY[op]] += 1
        prim[PRIMARY_NAMES.get(op, f"op{op:#04x}")] += 1
        if op == 0x03:
            target = ((pc + 4) & 0xF0000000) | ((w & 0x03FFFFFF) << 2)
            if start <= target < end:
                call_targets.add(target)
            else:
                out_of_range += 1
            flow["jal"] += 1
        elif op == 0:
            fn = w & 0x3F
            rs = (w >> 21) & 0x1F
            if fn == 0x08:
                flow["jr $ra" if rs == 31 else "jr <reg> (tabela/tail call)"] += 1
            elif fn == 0x09:
                flow["jalr (chamada indireta)"] += 1
            elif fn == 0x0C:
                flow["syscall"] += 1
            elif fn == 0x0D:
                flow["break"] += 1
    return dict(prim=prim, vfpu=vfpu, flow=flow, call_targets=call_targets,
                jal_out_of_range=out_of_range, words=len(data) // 4)


def read_symbols(img, secs):
    syms = []
    for s in secs:
        if s["type"] != 2:  # SHT_SYMTAB
            continue
        strsec = secs[s["link"]]
        for off in range(s["off"], s["off"] + s["size"], 16):
            name, value, size, info, _, shndx = struct.unpack_from("<IIIBBH", img.d, off)
            start = strsec["off"] + name
            syms.append({
                "name": img.d[start:img.d.index(b"\0", start)].decode("latin1"),
                "value": value, "size": size, "type": info & 0xF,
            })
    return syms


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("--json")
    a = ap.parse_args()
    img = Image(a.elf)
    mi = read_module_info(img)
    print(f"modulo {mi['name']!r} attr={mi['attr']:#x} gp={mi['gp']:#x} entry={img.entry:#x}")

    secs = img.sections()
    print(f"\n== secoes ({len(secs)})")
    for s in secs:
        print(f"  {s['name']:<22} tipo={s['type']:#04x} flags={s['flags']:#04x} "
              f"addr={s['addr']:#010x} off={s['off']:#09x} size={s['size']:#09x}")

    syms = read_symbols(img, secs)
    funcs = [x for x in syms if x["type"] == 2 and x["name"]]
    print(f"\n== simbolos: {len(syms)} no total, {len(funcs)} funcoes nomeadas")
    for x in funcs[:15]:
        print(f"  {x['value']:#010x} {x['size']:#07x} {x['name']}")

    imports = read_imports(img, mi)
    total = sum(len(l["funcs"]) for l in imports)
    print(f"\n== imports: {len(imports)} bibliotecas, {total} funcoes")
    for l in imports:
        print(f"  {l['lib']:<28} {len(l['funcs']):>4} funcs  {l['vars']} vars")

    exports = read_exports(img, mi)
    print("\n== exports")
    for e in exports:
        print(f"  {e['lib']}: " + ", ".join(
            f"{x['kind']} {x['nid']:08x}->{x['addr']:#x}" for x in e["entries"]))

    code = [s for s in secs if s["flags"] & 4 and s["type"] == 1 and s["size"]]
    prim, vfpu, flow = Counter(), Counter(), Counter()
    targets, words = set(), 0
    print("\n== codigo")
    for s in code:
        r = scan_code(img, s, mi)
        prim += r["prim"]; vfpu += r["vfpu"]; flow += r["flow"]
        targets |= r["call_targets"]; words += r["words"]
        print(f"  {s['name']}: {r['words']} palavras, jal fora da secao: {r['jal_out_of_range']}")
    print(f"  alvos unicos de jal (estimativa minima de funcoes): {len(targets)}")
    print("\n  fluxo de controle:")
    for k, v in flow.most_common():
        print(f"    {k:<32} {v}")
    n_vfpu = sum(vfpu.values())
    print(f"\n  VFPU: {n_vfpu} palavras ({100 * n_vfpu / max(words, 1):.1f}% do codigo)")
    for k, v in vfpu.most_common():
        print(f"    {k:<32} {v}")
    print("\n  opcodes primarios mais comuns:")
    for k, v in prim.most_common(12):
        print(f"    {k:<32} {v}")

    print("\n== strings relevantes")
    pat = re.compile(rb"[\x20-\x7e]{5,}")
    keys = re.compile(
        r"alchemy|intrinsic|igGeom|criware|\bcri\b|sofdec|adx|\.prx|flash0|disc0|host0|ms0|"
        r"\.afs|\.pmf|\.at3|copyright|\(c\)|version|\.cpp|\.c\b|lua|\.dsc|script", re.I)
    seen = []
    for m in pat.finditer(img.d):
        s = m.group().decode("latin1")
        if keys.search(s):
            seen.append(s)
    print(f"  {len(seen)} ocorrencias; primeiras 70:")
    for s in seen[:70]:
        print("   ", s[:110])

    if a.json:
        with open(a.json, "w") as f:
            json.dump({
                "module": mi, "sections": secs, "imports": imports, "exports": exports,
                "call_targets": sorted(targets), "symbols": funcs, "strings": seen,
            }, f, indent=1)
        print(f"\nJSON: {a.json}")


if __name__ == "__main__":
    main()
