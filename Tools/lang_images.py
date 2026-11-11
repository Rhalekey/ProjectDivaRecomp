"""Make the game's pictures of text say something else, from the game's own files.

    python Tools/lang_images.py <folder holding PSP_GAME> lang/en_images.txt <output folder>
    python Tools/lang_images.py <folder holding PSP_GAME> lang/en_images.txt <output folder> <font file> <folder of large textures>

Menu buttons and titles are drawn into PNG files inside the game's archive. This reads
them, wipes the words and writes new ones in the game's own font, keeping every picture
and icon around them. The output is the game's art, so it stays on your computer, in a
folder the game loads like a mod: do not add it to this repository.

With a font, each picture is also made four times larger with the words drawn from it, and
the renderer shows that version in the picture's place. Letters then take their own widths.

Each line of the table is

    <picture> | <name in its layout file, or @x,y,w,h> | <how far in the words start> | <style> | <text>

where the style is one of
    plate    words centred on a button, each row's colour taken from the column at that distance
    wide     the same in the large font
    clear    words centred on nothing, where the old words stood alone
    clearbig the same in the large font
    hint     small words beside a button icon, in the colour they had
    button   the same with a dark edge, for words shown over the picture
    button+N the same with icon and words N further right, for a row of buttons that the
             game places for longer words, and that would lean to the left otherwise
    title    large words on the title plate
"""
import itertools
import os
import re
import struct
import sys
import zlib

NAME_RECORD = 48
NAME_LEN = 32
ARCHIVE = os.path.join('PSP_GAME', 'USRDIR', 'media', 'afs', 'DivaDataList.afs')

SMALL_FONT = ('font12dot_01.png', 6, 12)
LARGE_FONT = ('font01.png', 8, 16)
EDGE = (42, 46, 66, 255)
TITLE_MARGIN_RIGHT = 26
TITLE_MARGIN_Y = 3


# ---- the archive
def read_archive(path):
    with open(path, 'rb') as f:
        head = f.read(8)
        count = struct.unpack('<I', head[4:])[0]
        table = struct.unpack('<%dI' % (count * 2), f.read(count * 8))
        names_at = struct.unpack('<I', f.read(4))[0]
        f.seek(names_at)
        names = f.read(count * NAME_RECORD)
    return [(names[i * NAME_RECORD:i * NAME_RECORD + NAME_LEN].split(b'\0')[0].decode('latin1'),
             table[i * 2], table[i * 2 + 1]) for i in range(count)]


class Archive:
    def __init__(self, path):
        self.path = path
        self.entries = read_archive(path)

    def read(self, name):
        for entry, offset, size in self.entries:
            if entry == name:
                with open(self.path, 'rb') as f:
                    f.seek(offset)
                    return f.read(size)
        raise SystemExit('the archive has no %s' % name)

    def find(self, wanted):
        """The name as the archive has it, which cuts long names short."""
        for entry, _, _ in self.entries:
            if entry == wanted or (len(entry) >= 16 and wanted.startswith(entry.rstrip('.'))):
                return entry
        raise SystemExit('the archive has no %s' % wanted)


# ---- PNG, as far as these files need it
def decode_png(data):
    i = 8
    idat = b''
    palette = alpha = None
    while i < len(data):
        n, kind = struct.unpack('>I4s', data[i:i + 8])
        chunk = data[i + 8:i + 8 + n]
        i += 12 + n
        if kind == b'IHDR':
            w, h, depth, colour, _, _, interlace = struct.unpack('>IIBBBBB', chunk)
        elif kind == b'IDAT':
            idat += chunk
        elif kind == b'PLTE':
            palette = chunk
        elif kind == b'tRNS':
            alpha = chunk
    if interlace:
        raise SystemExit('interlaced PNG')
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[colour]
    stride = (w * channels * depth + 7) // 8
    step = max(1, channels * depth // 8)
    raw = zlib.decompress(idat)
    rows = []
    prev = bytearray(stride)
    at = 0
    for _ in range(h):
        kind = raw[at]
        row = bytearray(raw[at + 1:at + 1 + stride])
        at += 1 + stride
        # A row with no filter is as it is: nothing to undo.
        for x in (range(stride) if kind else ()):
            a = row[x - step] if x >= step else 0
            b = prev[x]
            c = prev[x - step] if x >= step else 0
            if kind == 1:
                row[x] = (row[x] + a) & 255
            elif kind == 2:
                row[x] = (row[x] + b) & 255
            elif kind == 3:
                row[x] = (row[x] + (a + b) // 2) & 255
            elif kind == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                row[x] = (row[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(row)
        prev = row
    pixels = []
    for row in rows:
        if colour == 6:
            pixels.append([tuple(row[x:x + 4]) for x in range(0, stride, 4)])
        elif colour == 3:
            if depth == 4:
                values = [(row[x // 2] >> (4 if x % 2 == 0 else 0)) & 15 for x in range(w)]
            else:
                values = list(row)
            pixels.append([(palette[v * 3], palette[v * 3 + 1], palette[v * 3 + 2],
                            alpha[v] if alpha and v < len(alpha) else 255) for v in values])
        elif colour == 2:
            pixels.append([tuple(row[x:x + 3]) + (255,) for x in range(0, stride, 3)])
        else:
            raise SystemExit('unexpected PNG colour type %d' % colour)
    return w, h, pixels


def encode_png(w, h, pixels):
    raw = b''.join(b'\0' + bytes(itertools.chain.from_iterable(row)) for row in pixels)

    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


# ---- drawing
class Font:
    def __init__(self, archive, spec):
        name, self.w, self.h = spec
        _, _, self.pixels = decode_png(archive.read(name))

    def cell(self, ch):
        code = ord(ch)
        if 33 <= code <= 96:
            return (code - 33) * self.w, 0
        if 97 <= code <= 126:
            return (code - 97) * self.w, self.h
        return None

    def draw(self, target, x, y, text, colour, clip, edge=None):
        left, top, right, bottom = clip
        marks = []
        for ch in text:
            at = self.cell(ch)
            if at:
                for gy in range(self.h):
                    for gx in range(self.w):
                        if self.pixels[at[1] + gy][at[0] + gx][3] > 110:
                            marks.append((x + gx, y + gy))
            x += self.w
        if edge:
            for mx, my in marks:
                for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                    if left <= mx + dx < right and top <= my + dy < bottom:
                        target[my + dy][mx + dx] = edge
        for mx, my in marks:
            if left <= mx < right and top <= my < bottom:
                target[my][mx] = colour


    def draw_smooth(self, target, x, y, text, colour, under, clip):
        """For large letters, whose soft edges are worth keeping. Over a known colour."""
        left, top, right, bottom = clip
        for ch in text:
            at = self.cell(ch)
            if at:
                for gy in range(self.h):
                    for gx in range(self.w):
                        a = self.pixels[at[1] + gy][at[0] + gx][3]
                        px, py = x + gx, y + gy
                        if a and left <= px < right and top <= py < bottom:
                            target[py][px] = tuple((colour[i] * a + under[i] * (255 - a)) // 255 for i in range(3)) + (255,)
            x += self.w


def most_common(pixels, x0, y0, x1, y1, skip=None):
    counts = {}
    for y in range(y0, y1):
        for x in range(x0, x1):
            p = pixels[y][x]
            if p[3] >= 200 and p != skip:
                counts[p] = counts.get(p, 0) + 1
    return max(counts, key=counts.get) if counts else (255, 255, 255, 255)


def layout_of(archive, picture):
    stem = picture.split('.')[0]
    for candidate in (stem + '_log.txt', stem + '.txt'):
        try:
            text = archive.read(archive.find(candidate)).decode('latin1')
            break
        except SystemExit:
            continue
    else:
        raise SystemExit('no layout for %s' % picture)
    rects = {}
    for line in text.splitlines()[3:]:
        parts = line.split()
        if len(parts) == 5:
            rects[parts[0]] = tuple(int(v) for v in parts[1:])
    return rects


def colour_hash(pixels):
    """A picture by its colours, in the order the game keeps it. See render_gl.c."""
    h = 2166136261
    for row in reversed(pixels):
        for r, g, b, a in row:
            key = ((1 if a > 127 else 0) | (r >> 5) << 1 | (g >> 5) << 4 | (b >> 5) << 7 | 1 << 10) if a else 0
            h = ((h ^ key) * 16777619) & 0xFFFFFFFF
    return h


def fill_region(pixels, x0, y0, x1, y1, colour_of_row):
    for py in range(y0, y1):
        colour = colour_of_row(py)
        for px in range(x0, x1):
            pixels[py][px] = colour


class Label:
    """Where a label's words go, for drawing them again at a larger size."""

    def __init__(self, region, saved, left, top, width, height, centred, colour, edge):
        self.region = region
        self.saved = saved
        self.left, self.top, self.width, self.height = left, top, width, height
        self.centred = centred
        self.colour = colour
        self.edge = edge


def apply_label(pixels, rect, start, style, text, small, large):
    """Wipe the old words and write the new. Returns what is needed, what there is, and the label."""
    x, y, w, h = rect
    if style.startswith('button+'):
        moved = int(style[len('button+'):])
        for row in pixels[y:y + h]:
            row[x + moved:x + moved + start] = row[x:x + start]
            row[x:x + moved] = [(0, 0, 0, 0)] * moved
        start += moved
        style = 'button'
    if style in ('plate', 'wide', 'clear', 'clearbig'):
        font = large if style in ('wide', 'clearbig') else small
        fill_region(pixels, x, y, x + w, y + h, (lambda py: (0, 0, 0, 0)) if style in ('clear', 'clearbig') else (lambda py: pixels[py][x + start]))
        saved = [row[x:x + w] for row in pixels[y:y + h]]
        width = len(text) * font.w
        colour = (240, 244, 250, 255)
        at = (x + (w - width) // 2, y + (h - font.h) // 2)
        clip = (x, y, x + w, y + h)
        if style == 'wide':
            font.draw_smooth(pixels, at[0], at[1], text, colour, pixels[y + h // 2][x], clip)
        else:
            font.draw(pixels, at[0], at[1], text, colour, clip, EDGE)
        return width, w, Label((x, y, w, h), saved, x, y, w, h, True, colour, None if style == 'wide' else EDGE)
    if style == 'title':
        right = x + w - TITLE_MARGIN_RIGHT
        plate = pixels[y + TITLE_MARGIN_Y][x + start]
        # Some of these pictures go on below the plate, with lines that are not to be wiped.
        bottom = y + TITLE_MARGIN_Y
        while bottom < y + h and pixels[bottom][x + start] == plate:
            bottom += 1
        top = y + TITLE_MARGIN_Y
        colour = most_common(pixels, x + start, top, right, bottom, skip=plate)
        fill_region(pixels, x + start, top, right, bottom, lambda py: plate)
        region = (x + start, top, right - x - start, bottom - top)
        saved = [row[region[0]:region[0] + region[2]] for row in pixels[region[1]:region[1] + region[3]]]
        clip = (x + start, top, right, bottom)
        large.draw_smooth(pixels, x + start, top + (bottom - top - large.h) // 2, text, colour, plate, clip)
        return len(text) * large.w, clip[2] - clip[0], Label(region, saved, region[0], region[1], region[2], region[3], False, colour, None)
    colour = (232, 234, 244, 255) if style == 'button' else most_common(pixels, x + start, y, x + w, y + h)
    fill_region(pixels, x + start, y, x + w, y + h, lambda py: (0, 0, 0, 0))
    region = (x + start, y, w - start, h)
    saved = [row[region[0]:region[0] + region[2]] for row in pixels[y:y + h]]
    edge = EDGE if style == 'button' else None
    small.draw(pixels, x + start, y + (h - small.h) // 2, text, colour, (x + start, y, x + w, y + h), edge)
    return len(text) * small.w - 1, w - start, Label(region, saved, region[0], y, region[2], h, False, colour, edge)


# ---- the same words, large
# Pictures that come in dozens are made at less than the usual size, to keep the disk free.
SCALE = 4
HD_EDGE = 3
LARGE_SCALE = 4
MANY_SCALE = 2
MANY_PREFIX = 'pv_start_'


def set_scale(picture):
    global SCALE, HD_EDGE
    SCALE = MANY_SCALE if picture.startswith(MANY_PREFIX) else LARGE_SCALE
    HD_EDGE = SCALE - 1


def enlarge(pixels):
    """The game's own art, each pixel simply repeated: nothing added to it."""
    return [[p for p in row for _ in range(SCALE)] for row in pixels for _ in range(SCALE)]


# ---- the same art, cleaned. With DIVA_UI_SMOOTH set, large pictures are made in this way.
PLATE_NAME = re.compile(r'(btn|tub|button|tempo(on|off)|bpm(on|off)|timewin|timeber|namewin|planeo|memory)')
PLATE_MARGIN = 2
PLATE_NEAR = 40
PLATE_REACH = 2


def plate_rects(archive, picture):
    """The pieces with a dotted texture, which is pixels showing and not a part of the design."""
    try:
        rects = layout_of(archive, picture)
    except SystemExit:
        return []
    return [box for name, box in rects.items() if PLATE_NAME.search(name)]


def clean_plates(pixels, plates):
    """Each pixel of a plate becomes the mean of those around it that are like it, which takes
    out the dots and leaves the plate's gradient, and what stands out from it, as signs do."""
    rows = [list(row) for row in pixels]
    for x, y, w, h in plates:
        for py in range(y + PLATE_MARGIN, y + h - PLATE_MARGIN):
            for px in range(x + PLATE_MARGIN, x + w - PLATE_MARGIN):
                e = pixels[py][px]
                if not e[3]:
                    continue
                near = []
                for qy in range(max(py - PLATE_REACH, y + PLATE_MARGIN), min(py + PLATE_REACH + 1, y + h - PLATE_MARGIN)):
                    for qx in range(max(px - PLATE_REACH, x + PLATE_MARGIN), min(px + PLATE_REACH + 1, x + w - PLATE_MARGIN)):
                        p = pixels[qy][qx]
                        if p[3] and abs(p[0] - e[0]) < PLATE_NEAR and abs(p[1] - e[1]) < PLATE_NEAR and abs(p[2] - e[2]) < PLATE_NEAR:
                            near.append(p)
                count = len(near)
                rows[py][px] = (sum(p[0] for p in near) // count, sum(p[1] for p in near) // count, sum(p[2] for p in near) // count, e[3])
    return rows


def scale2x(rows):
    """Twice the size, turning the steps of a diagonal into a line. A corner is only cut where the
    diagonal goes on past it, so that squares keep theirs, and edges stay hard."""
    out = []
    last = len(rows) - 1
    for y, row in enumerate(rows):
        up, down = rows[max(y - 1, 0)], rows[min(y + 1, last)]
        top, bottom = [], []
        end = len(row) - 1
        for x, e in enumerate(row):
            left, right = max(x - 1, 0), min(x + 1, end)
            b, h, d, f = up[x], down[x], row[left], row[right]
            e0 = e1 = e2 = e3 = e
            if b != h and d != f:
                a, c, g, i = up[left], up[right], down[left], down[right]
                if d == b and (c == e or g == e):
                    e0 = d
                if f == b and (a == e or i == e):
                    e1 = f
                if d == h and (a == e or i == e):
                    e2 = d
                if f == h and (c == e or g == e):
                    e3 = f
            top += [e0, e1]
            bottom += [e2, e3]
        out += [top, bottom]
    return out


def smooth_enlarge(pixels, plates, scale):
    rows = clean_plates(pixels, plates)
    while scale > 1:
        rows = scale2x(rows)
        scale //= 2
    return [[tuple(p) for p in row] for row in rows]


def blend(under, colour, coverage):
    """Coverage 0..255 of a solid colour over a pixel that may be transparent."""
    if coverage >= 255:
        return colour
    a = under[3] * (255 - coverage) // 255 + coverage
    if not a:
        return (0, 0, 0, 0)
    rgb = tuple((colour[i] * coverage + under[i] * under[3] * (255 - coverage) // 255) // a for i in range(3))
    return (min(255, rgb[0]), min(255, rgb[1]), min(255, rgb[2]), a)


def draw_large(canvas, label, text, drawer):
    """Letters at their own widths, squeezed only if they would not fit."""
    from hd_font import GGO_METRICS_ADVANCE
    pen, marks = 0, []
    for ch in text:
        glyph = drawer.glyph(ch)
        if glyph:
            marks.append((pen, glyph))
        pen += GGO_METRICS_ADVANCE(drawer, ch)
    room = label.width * SCALE - 2 * HD_EDGE
    squeeze = min(1.0, room / pen) if pen else 1.0
    used = int(pen * squeeze)
    left = label.left * SCALE + ((room - used) // 2 + HD_EDGE if label.centred else HD_EDGE)
    top = label.top * SCALE + (label.height * SCALE - drawer.height) // 2
    coverage = {}
    for start, (raw, m) in marks:
        stride = (m.w + 3) & ~3
        for gy in range(m.h):
            py = top + drawer.ascent - m.y + gy
            for gx in range(m.w):
                v = raw[gy * stride + gx]
                if v:
                    key = (left + int((start + m.x + gx) * squeeze), py)
                    coverage[key] = max(coverage.get(key, 0), min(255, v * 255 // 64))
    x0, y0 = label.region[0] * SCALE, label.region[1] * SCALE
    x1, y1 = x0 + label.region[2] * SCALE, y0 + label.region[3] * SCALE
    if label.edge:
        solid = [k for k, v in coverage.items() if v > 96]
        for (px, py) in solid:
            for dy in range(-HD_EDGE, HD_EDGE + 1):
                for dx in range(-HD_EDGE, HD_EDGE + 1):
                    qx, qy = px + dx, py + dy
                    if x0 <= qx < x1 and y0 <= qy < y1:
                        canvas[qy][qx] = label.edge
    for (px, py), v in coverage.items():
        if x0 <= px < x1 and y0 <= py < y1:
            canvas[py][px] = blend(canvas[py][px], label.colour, v)


def write_large(pixels, original_hash, labels, font_family, cell_heights, out, plates=()):
    from hd_font import Drawer
    wiped = [list(row) for row in pixels]
    for label, _, _ in labels:
        x, y, w, h = label.region
        for row_number, saved_row in enumerate(label.saved):
            wiped[y + row_number][x:x + w] = saved_row
    canvas = smooth_enlarge(wiped, plates, SCALE) if os.environ.get('DIVA_UI_SMOOTH') else enlarge(wiped)
    for label, text, style in labels:
        size = min(label.height * SCALE, (16 if style in ('title', 'wide', 'clearbig') else 11) * SCALE)
        if size not in cell_heights:
            cell_heights[size] = Drawer(font_family, size)
        drawer = cell_heights[size]
        draw_large(canvas, label, text, drawer)
    # Named for the picture it stands in for, which the game goes on loading whatever the language.
    name = '%08X.rgba' % original_hash
    with open(os.path.join(out, name), 'wb') as f:
        f.write(struct.pack('<II', len(canvas[0]), len(canvas)))
        for row in reversed(canvas):
            f.write(bytes(itertools.chain.from_iterable(row)))
    return name


def main():
    args = sys.argv[1:]
    if len(args) not in (3, 5):
        raise SystemExit(__doc__)
    archive = Archive(os.path.join(args[0], ARCHIVE))
    small = Font(archive, SMALL_FONT)
    large = Font(archive, LARGE_FONT)

    pictures = {}
    with open(args[1], encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            if not line.strip() or line.startswith('#'):
                continue
            picture, name, start, style, text = [part.strip() for part in line.split('|')]
            # A picture ending in * is every picture in the archive that starts that way.
            if picture.endswith('*'):
                names = sorted({e[0] for e in archive.entries if e[0].startswith(picture[:-1]) and '.p' in e[0]})
            else:
                names = [picture]
            for one in names:
                pictures.setdefault(one, []).append((name, int(start), style, text))

    out = args[2]
    os.makedirs(out, exist_ok=True)
    family = None
    if len(args) == 5:
        from hd_font import Drawer, FREE_FONT, load_font
        family = load_font(args[3])
        # A font of the player's own may lack this language's letters. The free one has them.
        probe = Drawer(family, 44)
        lacking = {ch for labels in pictures.values() for label in labels for ch in label[3] if ch != ' ' and not probe.draws(ch)}
        if lacking:
            print('%s has no %s, so these pictures use the free font' % (family, sorted(lacking)))
            family = load_font(FREE_FONT)
        os.makedirs(args[4], exist_ok=True)
    drawers = {}
    for picture, labels in pictures.items():
        entry = archive.find(picture)
        w, h, pixels = decode_png(archive.read(entry))
        rects = layout_of(archive, picture) if any(not label[0].startswith('@') for label in labels) else {}
        set_scale(picture)
        original_hash = colour_hash(pixels)
        done = []
        for name, start, style, text in labels:
            if name.startswith('@'):
                rect = tuple(int(v) for v in name[1:].split(','))
            elif name in rects:
                rect = rects[name]
            else:
                raise SystemExit('%s has no %s' % (picture, name))
            needed, room, label = apply_label(pixels, rect, start, style, text, small, large)
            done.append((label, text, style))
            if needed > room:
                print('too long: %s "%s" needs %d, has %d' % (name, text, needed, room))
        with open(os.path.join(out, entry), 'wb') as f:
            f.write(encode_png(w, h, pixels))
        plates = plate_rects(archive, entry)
        large_name = write_large(pixels, original_hash, done, family, drawers, args[4], plates) if family else ''
        print('%s: %d labels %s' % (entry, len(labels), large_name))


if __name__ == '__main__':
    main()
