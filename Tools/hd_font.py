"""Redraw the game's bitmap fonts at four times their size, from a font file you own.

    python Tools/hd_font.py <folder holding PSP_GAME> <font file> <output folder>
    python Tools/hd_font.py --system "<font name>" <folder holding PSP_GAME> <output folder>

The game's fonts are grids of characters, listed in order by font.txt in its archive. Each
cell is drawn again from the font, and a character the font lacks is simply left out of the
file's list of replaced cells, so the game's own stays there, enlarged.

The game still asks for the same 512x512 texture. The renderer recognises it by the shape of
what is in it, and hands the card this version in its place: see src/render_gl.c.

The output and the font are yours and not this project's, so neither belongs in this
repository. Nothing here is fetched from anywhere.
"""
import ctypes
import os
import struct
import sys
from ctypes import wintypes

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lang_images import ARCHIVE, Archive, decode_png  # noqa: E402

SCALE = 4
ATLAS_SIDE = 512
# src/host/letters.c knows these two.
HALF_CELL_STRETCH = 2
CELL_MARGIN = 2
ROWS_PER_ATLAS = 32
FONTS = (('font01.png', 'font02.png', 'font03.png', 'font04.png'), 8, 16), \
        (('font12dot_01.png', 'font12dot_02.png', 'font12dot_03.png', 'font12dot_04.png'), 6, 12)
EM_FILL = 0.96    # how much of a cell's height the font's full height takes
ALPHA_SOLID = 127
FNV_START, FNV_PRIME = 2166136261, 16777619
FR_PRIVATE = 0x10
GGO_GRAY8_BITMAP = 6
GGO_METRICS = 0
GGI_MARK_NONEXISTING = 1
NO_GLYPH = 0xFFFF
FW_NORMAL = 400
SHIFTJIS_CHARSET = 128
GDI_ERROR = 0xFFFFFFFF

gdi = ctypes.WinDLL('gdi32', use_last_error=True)
user = ctypes.WinDLL('user32')


class GLYPHMETRICS(ctypes.Structure):
    _fields_ = [('w', wintypes.UINT), ('h', wintypes.UINT), ('x', wintypes.LONG), ('y', wintypes.LONG),
                ('cell_x', ctypes.c_short), ('cell_y', ctypes.c_short)]


class FIXED(ctypes.Structure):
    _fields_ = [('fract', wintypes.WORD), ('value', ctypes.c_short)]


class MAT2(ctypes.Structure):
    _fields_ = [('m11', FIXED), ('m12', FIXED), ('m21', FIXED), ('m22', FIXED)]


class TEXTMETRICW(ctypes.Structure):
    _fields_ = [('height', wintypes.LONG), ('ascent', wintypes.LONG), ('descent', wintypes.LONG),
                ('rest', ctypes.c_byte * 48)]


gdi.CreateFontW.restype = wintypes.HFONT
gdi.CreateCompatibleDC.restype = wintypes.HDC
gdi.CreateCompatibleDC.argtypes = [wintypes.HDC]
gdi.SelectObject.argtypes = [wintypes.HDC, wintypes.HGDIOBJ]
gdi.SelectObject.restype = wintypes.HGDIOBJ
gdi.GetGlyphOutlineW.argtypes = [wintypes.HDC, wintypes.UINT, wintypes.UINT, ctypes.POINTER(GLYPHMETRICS),
                                 wintypes.DWORD, ctypes.c_void_p, ctypes.POINTER(MAT2)]
gdi.GetGlyphOutlineW.restype = wintypes.DWORD
gdi.GetGlyphIndicesW.argtypes = [wintypes.HDC, wintypes.LPCWSTR, ctypes.c_int, ctypes.POINTER(wintypes.WORD), wintypes.DWORD]
gdi.GetGlyphIndicesW.restype = wintypes.DWORD
gdi.GetTextMetricsW.argtypes = [wintypes.HDC, ctypes.POINTER(TEXTMETRICW)]
gdi.GetTextFaceW.argtypes = [wintypes.HDC, ctypes.c_int, wintypes.LPWSTR]
gdi.AddFontResourceExW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, ctypes.c_void_p]


def GGO_METRICS_ADVANCE(drawer, ch):
    """How far a character moves the pen, which a space has too."""
    m = GLYPHMETRICS()
    gdi.GetGlyphOutlineW(drawer.dc, ord(ch), GGO_METRICS, ctypes.byref(m), 0, None, ctypes.byref(drawer.matrix))
    return m.cell_x


def load_font(path):
    """Make a font file usable by name for this process, and give the name."""
    if gdi.AddFontResourceExW(os.path.abspath(path), FR_PRIVATE, None) == 0:
        raise SystemExit('Windows could not read %s' % path)
    return family_of(path)


def family_of(path):
    """The name a font file gives itself, which is what Windows looks it up by."""
    with open(path, 'rb') as f:
        data = f.read()
    start = struct.unpack('>I', data[12:16])[0] if data[:4] == b'ttcf' else 0
    tables = struct.unpack('>H', data[start + 4:start + 6])[0]
    for i in range(tables):
        tag, _, offset, _ = struct.unpack('>4sIII', data[start + 12 + i * 16:start + 28 + i * 16])
        if tag != b'name':
            continue
        count, strings = struct.unpack('>HH', data[offset + 2:offset + 6])
        found = {}
        for r in range(count):
            platform, _, language, name_id, length, at = struct.unpack(
                '>6H', data[offset + 6 + r * 12:offset + 18 + r * 12])
            if name_id == 1 and platform == 3:
                text = data[offset + strings + at:offset + strings + at + length]
                found[language] = text.decode('utf-16-be', 'replace')
        if found:
            return found.get(0x409, next(iter(found.values())))
    raise SystemExit('%s has no name table' % path)


class Drawer:
    def __init__(self, family, cell_h):
        self.dc = gdi.CreateCompatibleDC(None)
        height = int(cell_h * EM_FILL)
        self.font = gdi.CreateFontW(-height, 0, 0, 0, FW_NORMAL, 0, 0, 0, SHIFTJIS_CHARSET, 0, 0, 4, 0, family)
        gdi.SelectObject(self.dc, self.font)
        face = ctypes.create_unicode_buffer(128)
        gdi.GetTextFaceW(self.dc, 128, face)
        if face.value.lower() != family.lower():
            raise SystemExit('Windows gave "%s" when asked for "%s"' % (face.value, family))
        metrics = TEXTMETRICW()
        gdi.GetTextMetricsW(self.dc, ctypes.byref(metrics))
        self.ascent = metrics.ascent
        self.height = metrics.height
        one = FIXED(0, 1)
        zero = FIXED(0, 0)
        self.matrix = MAT2(one, zero, zero, one)

    def has(self, ch):
        index = wintypes.WORD(0)
        gdi.GetGlyphIndicesW(self.dc, ch, 1, ctypes.byref(index), GGI_MARK_NONEXISTING)
        return index.value != NO_GLYPH

    def draws(self, ch):
        """Some fonts say they have a letter and then give nothing to draw."""
        return self.has(ch) and self.glyph(ch) is not None

    def glyph(self, ch):
        """Coverage 0..64 per pixel, with its size and offset from the left and the baseline."""
        m = GLYPHMETRICS()
        size = gdi.GetGlyphOutlineW(self.dc, ord(ch), GGO_GRAY8_BITMAP, ctypes.byref(m), 0, None,
                                    ctypes.byref(self.matrix))
        if size == GDI_ERROR or size == 0:
            return None
        buffer = ctypes.create_string_buffer(size)
        gdi.GetGlyphOutlineW(self.dc, ord(ch), GGO_GRAY8_BITMAP, ctypes.byref(m), size, buffer,
                             ctypes.byref(self.matrix))
        return buffer.raw, m


def shape_hash(pixels):
    """What the renderer computes for a texture: one bit per pixel, solid or not.
    The game keeps its textures upside down compared with their PNG files."""
    h = FNV_START
    for row in reversed(pixels):
        for p in row:
            h = ((h ^ (1 if p[3] > ALPHA_SOLID else 0)) * FNV_PRIME) & 0xFFFFFFFF
    return h


# Letters the game's font has no place for, drawn over half-width kana, which it never shows.
# src/host/translation.c puts the same bytes in the text.
OVER_KANA = {'\uff61': '\u00e4', '\uff62': '\u00f6', '\uff63': '\u00fc', '\uff64': '\u00c4',
             '\uff65': '\u00d6', '\uff66': '\u00dc', '\uff67': '\u00df'}
FREE_FONT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'fonts', 'MPLUSRounded1c-Medium.ttf')


def cells_of(font_list):
    """(atlas number, row, column in half cells, width in half cells, character)."""
    atlas_cells = []
    for number, line in enumerate(font_list.split(b'\r\n')):
        column = i = 0
        while i < len(line):
            lead = line[i]
            two = 0x81 <= lead <= 0x9F or 0xE0 <= lead <= 0xFC
            raw = line[i:i + (2 if two else 1)]
            i += len(raw)
            try:
                ch = raw.decode('cp932')
            except UnicodeDecodeError:
                ch = ' '
            atlas_cells.append((number // ROWS_PER_ATLAS, number % ROWS_PER_ATLAS, column, len(raw), OVER_KANA.get(ch, ch)))
            column += len(raw)
    return atlas_cells


def write_atlas(archive, name, half_w, cell_h, cells, main_drawer, fallback, out):
    _, _, pixels = decode_png(archive.read(name))
    # A page with half-width letters is wider, because they get stretched to fill their cells.
    across = SCALE * (HALF_CELL_STRETCH if any(c[2] == 1 for c in cells) else 1)
    side_w, side = ATLAS_SIDE * across, ATLAS_SIDE * SCALE
    # Start from the game's own coverage, enlarged, so whatever is not redrawn is still there.
    alpha = bytearray(side_w * side)
    for y, row in enumerate(pixels):
        for x, p in enumerate(row):
            if p[3]:
                for yy in range(SCALE):
                    at = (y * SCALE + yy) * side_w + x * across
                    alpha[at:at + across] = bytes((p[3],)) * across
    drawn = 0
    letters = set(OVER_KANA.values())
    for row_number, column, width, ch in cells:
        # A font of the player's own may be Japanese only, and these have to be there.
        drawer = fallback if ch in letters and not main_drawer.draws(ch) else main_drawer
        if ch == ' ' or ch == '　' or not drawer.has(ch):
            continue
        x0 = column * half_w * across
        y0 = row_number * cell_h * SCALE
        w = width * half_w * across
        h = cell_h * SCALE
        for yy in range(h):
            at = (y0 + yy) * side_w + x0
            alpha[at:at + w] = bytes(w)
        glyph = drawer.glyph(ch)
        if not glyph:
            continue
        raw, m = glyph
        stride = (m.w + 3) & ~3
        # A half-width letter fills its cell, and src/host/letters.c gives it back its width.
        if width == 1:
            out_w = w - 2 * CELL_MARGIN
            left = x0 + CELL_MARGIN
        else:
            out_w = min(m.w * across // SCALE, w)
            left = x0 + (w - out_w) // 2
        top = y0 + (h - drawer.height) // 2 + drawer.ascent - m.y
        for gy in range(m.h):
            for ox in range(out_w):
                first = ox * m.w // out_w
                last = max(first + 1, (ox + 1) * m.w // out_w)
                v = sum(raw[gy * stride + gx] for gx in range(first, last)) // (last - first)
                px, py = left + ox, top + gy
                if v and y0 <= py < y0 + h:
                    alpha[py * side_w + px] = min(255, v * 255 // 64)
        drawn += 1
    path = os.path.join(out, '%08X.alpha' % shape_hash(pixels))
    with open(path, 'wb') as f:
        f.write(struct.pack('<II', side_w, side))
        for y in range(side - 1, -1, -1):
            f.write(alpha[y * side_w:(y + 1) * side_w])
    print('%s: %d characters drawn, %s' % (name, drawn, os.path.basename(path)))


def letters_of_half_cells():
    """Each byte that is a half-width letter, with the letter."""
    over = {'\uff61': 0xA1, '\uff62': 0xA2, '\uff63': 0xA3, '\uff64': 0xA4, '\uff65': 0xA5, '\uff66': 0xA6, '\uff67': 0xA7}
    letters = [(b, bytes((b,)).decode('cp932')) for b in range(0x20, 0x7F)]
    return letters + [(byte, OVER_KANA[kana]) for kana, byte in over.items()]


def write_widths(path, measured):
    """How wide each half-width letter really is, in pixels of these files, for src/host/letters.c."""
    with open(path, 'w', encoding='ascii') as f:
        for half_w, drawer, fallback in measured:
            for byte, ch in letters_of_half_cells():
                source = fallback if not drawer.draws(ch) and ch != ' ' else drawer
                advance = GGO_METRICS_ADVANCE(source, ch)
                glyph = source.glyph(ch) if ch != ' ' else None
                ink = glyph[1].w if glyph else 0
                bearing = glyph[1].x if glyph else 0
                # Digits stay in columns.
                if ch.isdigit():
                    advance = half_w * SCALE
                    bearing = (advance - ink) // 2
                f.write('%d %d %d %d %d\n' % (half_w, byte, advance, bearing, ink))


def main():
    args = sys.argv[1:]
    if len(args) == 4 and args[0] == '--system':
        family, disc, out = args[1], args[2], args[3]
    elif len(args) == 3:
        disc, path, out = args
        family = load_font(path)
    else:
        raise SystemExit(__doc__)
    print('font: %s' % family)
    archive = Archive(os.path.join(disc, ARCHIVE))
    cells = cells_of(archive.read('font.txt'))
    free_family = load_font(FREE_FONT)
    os.makedirs(out, exist_ok=True)
    measured = []
    for names, half_w, cell_h in FONTS:
        drawer = Drawer(family, cell_h * SCALE)
        fallback = Drawer(free_family, cell_h * SCALE)
        measured.append((half_w, drawer, fallback))
        for number, name in enumerate(names):
            mine = [c[1:] for c in cells if c[0] == number]
            write_atlas(archive, name, half_w, cell_h, mine, drawer, fallback, out)
    write_widths(os.path.join(out, 'letter_widths.txt'), measured)


if __name__ == '__main__':
    main()
