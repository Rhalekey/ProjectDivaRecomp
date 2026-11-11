"""Read and write Alchemy IGB files, the game's 3D models, as a list of objects that can be changed.

    python Tools/igb.py dump <file.igb>           the classes, objects and memory blocks in it
    python Tools/igb.py check <file.igb> ...      that each file comes out the same once read and written

Written from the files themselves. The layout of version 8, the one the game uses:

    header (0x30)   sizes and counts of five of the sections below, the magic 0xFADA at 0x28,
                    and at 0x2C the version in the low half and flags in the high one
    strings         a table of names other sections point into by index
    meta fields     the kinds of field, igIntMetaField and so on
    alignments      names of kinds of memory (version 8 and below)
    meta objects    the classes: name, parent, and every field, inherited ones included
    externals       (flag) other directories this one can refer to
    pool names      (flag) the game's memory pools
    entries         the directory: one entry for every object and every block of memory
    indexes         (flag, shared entries) the directory's order, as indexes into the entries
    info            (flag) the directory index of the file's info list
    objects         each object of the directory, in its order: its class, its size, its fields
    memory          each block of memory of the directory, in its order, each one aligned

A field is stored in 4 byte steps; references to objects or memory are directory indexes, strings
are indexes into the strings table.
"""
import struct
import sys

MAGIC = 0xFADA
FLAG_INFO, FLAG_EXTERNAL, FLAG_SHARED, FLAG_POOLS = 0x8000, 0x4000, 0x2000, 0x1000


def u32(b, at):
    return struct.unpack_from('<I', b, at)[0]


def i32(b, at):
    return struct.unpack_from('<i', b, at)[0]


def pad4(n):
    return (n + 3) & ~3


class MetaObject:
    def __init__(self, name, major, minor, parent, slots, fields):
        self.name, self.major, self.minor, self.parent, self.slots = name, major, minor, parent, slots
        self.fields = fields          # (meta field index, slot, size), inherited ones first


class Object:
    """An object of the directory: its class and its fields as raw values, each a bytes of its size."""
    def __init__(self, meta, values):
        self.meta, self.values = meta, values


class Entry:
    """A directory entry: an igObjectDirEntry or igMemoryDirEntry, itself stored as an object."""
    def __init__(self, meta, values):
        self.meta, self.values = meta, values


class IGB:
    def __init__(self, data):
        self.read(data)

    # ---- reading
    def read(self, b):
        h = struct.unpack_from('<12I', b, 0)
        if h[10] != MAGIC:
            raise ValueError('not an IGB file')
        self.version, self.flags = h[11] & 0xFFFF, h[11] >> 16
        if self.version != 8:
            raise ValueError('IGB version %d, only 8 is known' % self.version)
        entry_size, entry_count, mo_size, mo_count, obj_size, obj_count, mem_size, mem_count, mf_size, mf_count = h[:10]
        p = 0x30

        # strings
        size, count = u32(b, p), u32(b, p + 4)
        self.strings, q = [], p + 8
        for _ in range(count):
            n = u32(b, q)
            self.strings.append(b[q + 4:q + 4 + n].split(b'\0')[0].decode('latin1') if n else None)
            q += 4 + n
        self.strings_tail = b[q:p + size]
        p += size

        # meta fields
        names_at = p + mf_count * 12
        self.meta_fields = []
        for i in range(mf_count):
            n, major, minor = struct.unpack_from('<III', b, p + i * 12)
            self.meta_fields.append((b[names_at:names_at + n - 1].decode('latin1'), major, minor))
            names_at += n
        p += mf_size

        # alignments: kept as they are
        size = u32(b, p)
        self.alignments = b[p:p + size]
        p += size

        # meta objects
        rest = p + mo_count * 24
        self.meta_objects = []
        for i in range(mo_count):
            n, major, minor, nfields, parent, slots = struct.unpack_from('<IIIIiI', b, p + i * 24)
            name = b[rest:rest + n].split(b'\0')[0].decode('latin1')
            self.meta_objects.append((n, major, minor, parent, slots))
            rest += n
            fields = [struct.unpack_from('<HHH', b, rest + j * 6) for j in range(nfields)]
            rest += nfields * 6
            self.meta_objects[-1] = MetaObject(name, major, minor, parent, slots, fields)
            self.meta_objects[-1].name_len = n
        p += mo_size

        self.externals = self.pools = b''
        if self.flags & FLAG_EXTERNAL:
            size = u32(b, p)
            self.externals = b[p:p + size]
            p += size
        if self.flags & FLAG_POOLS:
            size = u32(b, p)
            self.pools = b[p:p + size]
            p += size

        # entries
        end = p + entry_size
        self.entries = []
        for _ in range(entry_count):
            meta, size = u32(b, p), u32(b, p + 4)
            self.entries.append(Entry(meta, self.read_fields(b, p, meta, size)))
            p += size
        if p != end:
            raise ValueError('entries end at %#x, not %#x' % (p, end))

        self.indexes = list(range(entry_count))
        if self.flags & FLAG_SHARED:
            size, count = u32(b, p), u32(b, p + 4)
            self.indexes = list(struct.unpack_from('<%dH' % count, b, p + 8))
            self.indexes_tail = b[p + 8 + count * 2:p + size]
            p += size
        self.info = None
        if self.flags & FLAG_INFO:
            self.info = i32(b, p)
            p += 4

        # objects, in the directory's order
        self.items = []               # per directory slot: an Object, or bytes for memory
        objects_at, objects_end = p, p + obj_size
        for slot, index in enumerate(self.indexes):
            e = self.entries[index]
            if self.is_object_entry(e):
                meta, size = u32(b, p), u32(b, p + 4)
                self.items.append(Object(meta, self.read_fields(b, p, meta, size)))
                p += size
            else:
                self.items.append(None)
        if p != objects_end:
            raise ValueError('objects end at %#x, not %#x' % (p, objects_end))

        # memory, in the directory's order, each block aligned
        mem_at = p
        self.gaps = []
        for slot, index in enumerate(self.indexes):
            e = self.entries[index]
            if self.is_object_entry(e):
                continue
            n = self.memory_size(e)
            start = mem_at + self.align_up(p - mem_at, self.memory_alignment(e))
            self.gaps.append(b[p:start])
            self.items[slot] = b[start:start + n]
            p = start + n
        self.tail = b[p:]
        if p > mem_at + mem_size:
            raise ValueError('memory runs past its size')
        self.mem_size = mem_size

    def read_fields(self, b, at, meta, size):
        values, q = [], at + 8
        for field, _slot, fsize in self.meta_objects[meta].fields:
            values.append(b[q:q + fsize])
            q += pad4(fsize)
        if q != at + size:
            values.append(b[q:at + size])     # anything past the fields, kept
        return values

    # ---- what entries say
    def entry_class(self, e):
        return self.meta_objects[e.meta].name

    def is_object_entry(self, e):
        return self.entry_class(e) == 'igObjectDirEntry'

    def memory_size(self, e):
        return u32(e.values[1], 0)

    def memory_type(self, e):
        t = i32(e.values[2], 0)
        return self.meta_fields[t][0] if t >= 0 else None

    def memory_alignment(self, e):
        # The field names a kind of memory from the alignments section, or is -1; in all of the
        # game's files every block simply follows the one before at the next multiple of 4.
        return 4

    @staticmethod
    def align_up(n, a):
        return (n + a - 1) // a * a

    # ---- writing
    def write(self):
        out = bytearray(0x30)

        strings = bytearray()
        for s in self.strings:
            if s is None:
                strings += struct.pack('<I', 0)
            else:
                raw = s.encode('latin1') + b'\0'
                strings += struct.pack('<I', len(raw)) + raw
        strings += self.strings_tail
        out += struct.pack('<II', len(strings) + 8, len(self.strings)) + strings

        mf = bytearray()
        names = bytearray()
        for name, major, minor in self.meta_fields:
            raw = name.encode('latin1') + b'\0'
            mf += struct.pack('<III', len(raw), major, minor)
            names += raw
        mf += names
        out += mf
        out += self.alignments

        mo = bytearray()
        rest = bytearray()
        for m in self.meta_objects:
            raw = m.name.encode('latin1') + b'\0'
            raw += b'\0' * (m.name_len - len(raw)) if m.name_len >= len(raw) else b''
            mo += struct.pack('<IIIIiI', len(raw), m.major, m.minor, len(m.fields), m.parent, m.slots)
            rest += raw + b''.join(struct.pack('<HHH', *f) for f in m.fields)
        mo += rest
        out += mo
        out += self.externals + self.pools

        entries = bytearray()
        for e in self.entries:
            entries += self.write_fields(e.meta, e.values)
        out += entries
        if self.flags & FLAG_SHARED:
            body = struct.pack('<%dH' % len(self.indexes), *self.indexes) + self.indexes_tail
            out += struct.pack('<II', len(body) + 8, len(self.indexes)) + body
        if self.flags & FLAG_INFO:
            out += struct.pack('<i', self.info)

        objects = bytearray()
        count = 0
        for item in self.items:
            if isinstance(item, Object):
                objects += self.write_fields(item.meta, item.values)
                count += 1
        out += objects

        memory = bytearray()
        mcount = 0
        for slot, index in enumerate(self.indexes):
            e = self.entries[index]
            if self.is_object_entry(e):
                continue
            block = self.items[slot]
            start = self.align_up(len(memory), self.memory_alignment(e))
            memory += b'\0' * (start - len(memory)) + block
            mcount += 1
        mem_size = len(memory)
        out += memory + self.tail

        struct.pack_into('<12I', out, 0, len(entries), len(self.entries), len(mo), len(self.meta_objects), len(objects), count,
                         mem_size, mcount, len(mf), len(self.meta_fields), MAGIC, (self.flags << 16) | self.version)
        return bytes(out)

    def write_fields(self, meta, values):
        body = bytearray()
        fields = self.meta_objects[meta].fields
        for i, v in enumerate(values):
            body += v
            if i < len(fields):
                body += b'\0' * (pad4(len(v)) - len(v))
        return struct.pack('<II', meta, len(body) + 8) + body

    # ---- looking
    def field_names(self, meta):
        return [self.meta_fields[f][0] for f, _s, _z in self.meta_objects[meta].fields]

    def dump(self):
        print('version %d, flags %#x, %d strings, %d classes, %d entries' % (self.version, self.flags, len(self.strings),
                                                                         len(self.meta_objects), len(self.entries)))
        for i, m in enumerate(self.meta_objects):
            parent = self.meta_objects[m.parent].name if m.parent >= 0 else '-'
            print('class %2d %-28s < %-22s %s' % (i, m.name, parent, ' '.join('%s/%d' % (self.meta_fields[f][0][2:-9], z) for f, _s, z in m.fields)))
        for slot, index in enumerate(self.indexes):
            item = self.items[slot]
            if isinstance(item, Object):
                print('%4d obj %-26s %s' % (slot, self.meta_objects[item.meta].name, ' '.join(v.hex() if len(v) <= 8 else '<%d>' % len(v) for v in item.values)))
            else:
                e = self.entries[index]
                print('%4d mem %-26s %d bytes, align %d' % (slot, self.memory_type(e), len(item), self.memory_alignment(e)))


    # ---- changing
    def own_entry(self, slot):
        """The slot's directory entry, made its own first: entries that are alike are shared by
        many slots, and a change to a shared one would change them all."""
        index = self.indexes[slot]
        if sum(1 for i in self.indexes if i == index) > 1:
            e = self.entries[index]
            self.entries.append(Entry(e.meta, list(e.values)))
            self.indexes[slot] = len(self.entries) - 1
        return self.entries[self.indexes[slot]]

    def set_memory(self, slot, data):
        """A block of memory given new contents, of any size."""
        self.own_entry(slot).values[1] = struct.pack('<I', len(data))
        self.items[slot] = bytes(data)

    def add_memory(self, like_slot, data):
        """A new block, of the kind of another; returns its slot."""
        e = self.entries[self.indexes[like_slot]]
        self.entries.append(Entry(e.meta, list(e.values)))
        self.entries[-1].values[1] = struct.pack('<I', len(data))
        self.indexes.append(len(self.entries) - 1)
        self.items.append(bytes(data))
        return len(self.items) - 1

    def add_object(self, like_slot, values):
        """A new object, of the class of another; returns its slot."""
        e = self.entries[self.indexes[like_slot]]
        self.entries.append(Entry(e.meta, list(e.values)))
        self.indexes.append(len(self.entries) - 1)
        self.items.append(Object(self.items[like_slot].meta, list(values)))
        return len(self.items) - 1

    # ---- references
    REF_FIELDS = ('igObjectRefMetaField', 'igMemoryRefMetaField')

    def refs_of(self, slot):
        """The directory slots this slot refers to: from an object's reference fields, or the
        words of a block of references."""
        item = self.items[slot]
        out = []
        if isinstance(item, Object):
            for (f, _s, _z), v in zip(self.meta_objects[item.meta].fields, item.values):
                if self.meta_fields[f][0] in self.REF_FIELDS and len(v) == 4 and i32(v, 0) >= 0:
                    out.append(i32(v, 0))
        elif self.memory_type(self.entries[self.indexes[slot]]) in self.REF_FIELDS:
            out += [r for r in struct.unpack('<%di' % (len(item) // 4), item[:len(item) // 4 * 4]) if r >= 0]
        return out

    def collect(self):
        """Leaves out every object and block nothing reaches from the info list; returns how many."""
        if self.info is None:
            return 0
        seen, todo = set(), [self.info]
        while todo:
            slot = todo.pop()
            if slot in seen or not 0 <= slot < len(self.items):
                continue
            seen.add(slot)
            todo += self.refs_of(slot)
        keep = [s for s in range(len(self.items)) if s in seen]
        new = {old: i for i, old in enumerate(keep)}
        remap = lambda r: new.get(r, -1) if r >= 0 else r
        for slot in keep:
            item = self.items[slot]
            if isinstance(item, Object):
                for k, ((f, _s, _z), v) in enumerate(zip(self.meta_objects[item.meta].fields, item.values)):
                    if self.meta_fields[f][0] in self.REF_FIELDS and len(v) == 4:
                        item.values[k] = struct.pack('<i', remap(i32(v, 0)))
            elif self.memory_type(self.entries[self.indexes[slot]]) in self.REF_FIELDS:
                n = len(item) // 4
                words = [remap(r) for r in struct.unpack('<%di' % n, item[:n * 4])]
                self.items[slot] = struct.pack('<%di' % n, *words) + item[n * 4:]
        # Entries still used, in their order.
        used = sorted({self.indexes[s] for s in keep})
        entry_at = {old: i for i, old in enumerate(used)}
        dropped = len(self.items) - len(keep)
        self.entries = [self.entries[i] for i in used]
        self.indexes = [entry_at[self.indexes[s]] for s in keep]
        self.items = [self.items[s] for s in keep]
        self.info = new[self.info]
        return dropped


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    if sys.argv[1] == 'dump':
        IGB(open(sys.argv[2], 'rb').read()).dump()
    elif sys.argv[1] == 'check':
        bad = 0
        for path in sys.argv[2:]:
            data = open(path, 'rb').read()
            try:
                again = IGB(data).write()
            except Exception as e:  # noqa: BLE001
                print('%s: %s' % (path, e))
                bad += 1
                continue
            if again != data:
                at = next((i for i in range(min(len(data), len(again))) if data[i] != again[i]), min(len(data), len(again)))
                print('%s: differs at %#x (%d and %d bytes)' % (path, at, len(data), len(again)))
                bad += 1
        print('%d of %d the same' % (len(sys.argv) - 2 - bad, len(sys.argv) - 2))
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
