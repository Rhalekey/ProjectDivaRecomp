import struct, sys
d = open(sys.argv[1], "rb").read()

(e_type, e_machine, e_version, e_entry, e_phoff, e_shoff, e_flags,
 e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx) = \
    struct.unpack_from("<HHIIIIIHHHHHH", d, 16)

print(f"tipo: {e_type:#x}  (0x2 = EXEC, 0xffa0 = PRX relocavel)")
print(f"maquina: {e_machine} (8 = MIPS)  entry: {e_entry:#x}")
print(f"program headers: {e_phnum}  section headers: {e_shnum}")

phs = []
for i in range(e_phnum):
    ph = struct.unpack_from("<8I", d, e_phoff + i * e_phentsize)
    phs.append(ph)
    p_type, p_off, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = ph
    print(f"  seg{i}: tipo={p_type:#x} off={p_off:#x} vaddr={p_vaddr:#x} "
          f"filesz={p_filesz:#x} memsz={p_memsz:#x} flags={p_flags:#x}")

try:
    off = phs[0][3] & 0x7FFFFFFF
    attr, = struct.unpack_from("<H", d, off)
    name = d[off+4:off+31].split(b"\0")[0].decode(errors="replace")
    gp, ent_top, ent_end, stub_top, stub_end = struct.unpack_from("<5I", d, off + 32)
    print(f"modulo: {name!r} attr={attr:#x} gp={gp:#x}")
    print(f"exports={ent_top:#x}-{ent_end:#x} imports={stub_top:#x}-{stub_end:#x}")
except Exception as e:
    print("could not read the module info:", e)