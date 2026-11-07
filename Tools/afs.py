"""List and extract the files inside the game's AFS archives, for making mods.

    python Tools/afs.py list <archive.afs> [text to search for]
    python Tools/afs.py extract <archive.afs> <name> <output folder>

The extracted files are the game's. Do not share them or add them to this repository.
"""
import os
import struct
import sys

NAME_RECORD = 48
NAME_LEN = 32


def read_entries(path):
    with open(path, 'rb') as f:
        head = f.read(8)
        if head[:4] != b'AFS\0':
            raise SystemExit('%s is not an AFS archive' % path)
        count = struct.unpack('<I', head[4:])[0]
        table = struct.unpack('<%dI' % (count * 2), f.read(count * 8))
        names_at, names_size = struct.unpack('<II', f.read(8))
        f.seek(names_at)
        names = f.read(names_size)
    entries = []
    for i in range(count):
        raw = names[i * NAME_RECORD:i * NAME_RECORD + NAME_LEN]
        entries.append((raw.split(b'\0')[0].decode('latin1'), table[i * 2], table[i * 2 + 1]))
    return entries


def main():
    args = sys.argv[1:]
    if len(args) >= 2 and args[0] == 'list':
        wanted = args[2].lower() if len(args) > 2 else ''
        for name, _, size in read_entries(args[1]):
            if wanted in name.lower():
                print('%10d  %s' % (size, name))
    elif len(args) == 4 and args[0] == 'extract':
        matches = [e for e in read_entries(args[1]) if e[0] == args[2]]
        if not matches:
            raise SystemExit('no entry named %s' % args[2])
        _, offset, size = matches[0]
        os.makedirs(args[3], exist_ok=True)
        out = os.path.join(args[3], args[2])
        with open(args[1], 'rb') as f:
            f.seek(offset)
            data = f.read(size)
        with open(out, 'wb') as f:
            f.write(data)
        print('%s: %d bytes' % (out, size))
        if len(matches) > 1:
            print('%d entries share this name; a mod replaces them all' % len(matches))
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
