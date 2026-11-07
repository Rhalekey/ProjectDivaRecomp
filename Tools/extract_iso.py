"""Copy PSP_GAME out of a PSP disc image, and check it is the version this project is for.

    python Tools/extract_iso.py <game.iso> <output folder>

Reads the ISO 9660 file system directly, so nothing else needs to be installed.
"""
import hashlib
import os
import struct
import sys

SECTOR = 2048
FIRST_DESCRIPTOR = 16
CHUNK = 1 << 20
FLAG_DIRECTORY = 2

# The encrypted executable on the v1.01 disc.
EXPECTED_DISC_EBOOT = '5239282078'


def entries(iso, sector, size):
    iso.seek(sector * SECTOR)
    data = iso.read(size)
    at = 0
    while at < len(data):
        length = data[at]
        if length == 0:
            # Records do not cross a sector: the rest of this one is padding.
            at = (at // SECTOR + 1) * SECTOR
            continue
        record = data[at:at + length]
        name_length = record[32]
        raw = record[33:33 + name_length]
        at += length
        if raw in (b'\0', b'\1'):
            continue
        name = raw.split(b';')[0].decode('ascii', 'replace')
        start = struct.unpack('<I', record[2:6])[0]
        size_of = struct.unpack('<I', record[10:14])[0]
        yield name, start, size_of, bool(record[25] & FLAG_DIRECTORY)


def copy_tree(iso, sector, size, out):
    os.makedirs(out, exist_ok=True)
    count = 0
    for name, start, length, is_dir in entries(iso, sector, size):
        path = os.path.join(out, name)
        if is_dir:
            count += copy_tree(iso, start, length, path)
            continue
        iso.seek(start * SECTOR)
        left = length
        with open(path, 'wb') as f:
            while left:
                part = iso.read(min(CHUNK, left))
                if not part:
                    raise SystemExit('The image ended before the end of %s: the file seems incomplete.' % name)
                f.write(part)
                left -= len(part)
        count += 1
    return count


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    with open(sys.argv[1], 'rb') as iso:
        iso.seek(FIRST_DESCRIPTOR * SECTOR)
        descriptor = iso.read(SECTOR)
        if descriptor[1:6] != b'CD001':
            raise SystemExit('This does not look like an ISO. Use the game\'s ISO, not a CSO or ZIP.')
        root = descriptor[156:190]
        root_sector = struct.unpack('<I', root[2:6])[0]
        root_size = struct.unpack('<I', root[10:14])[0]
        found = [e for e in entries(iso, root_sector, root_size) if e[0] == 'PSP_GAME' and e[3]]
        if not found:
            raise SystemExit('This image has no PSP_GAME folder.')
        _, sector, size, _ = found[0]
        count = copy_tree(iso, sector, size, os.path.join(sys.argv[2], 'PSP_GAME'))
    eboot = os.path.join(sys.argv[2], 'PSP_GAME', 'SYSDIR', 'EBOOT.BIN')
    with open(eboot, 'rb') as f:
        digest = hashlib.sha1(f.read()).hexdigest()
    if not digest.startswith(EXPECTED_DISC_EBOOT):
        raise SystemExit('This ISO is not the original ULJM05472 v1.01 (SHA-1 of its EBOOT: %s).' % digest)
    print('%d files copied. The ISO is ULJM05472 v1.01.' % count)


if __name__ == '__main__':
    main()
