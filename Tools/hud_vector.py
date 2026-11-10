"""Draw the song screen's HUD from vectors, at four times the size of the game's own.

    python Tools/hud_vector.py <folder holding PSP_GAME> <folder of large textures>

Nothing here comes from a picture: every shape, letter and number is defined below, and
placed where the game's layout files say that piece goes. The game asks for its usual
textures and the renderer shows these in their place, so positions and timing stay the game's.

The game's art is only read to name the output, after a hash of the picture it replaces.
"""
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lang_images import ARCHIVE, Archive, colour_hash, decode_png, layout_of  # noqa: E402

SCALE = 4
SUBSAMPLES = 4
LINE = (205, 225, 235)
DARK = (17, 23, 27)
BAND_ALPHA = 164
THIN_ALPHA = 75
RED = (150, 22, 60)
RED_LIGHT = (235, 30, 90)

# Note symbols, which are inside the game's models and so are named by the hash the game's
# own texture has in memory. See DIVA_DUMP_TEXTURES in src/host/main.c.
NOTE_SIDE = 64
NOTE_CIRCLE = 'AC18437B'
NOTE_CROSS = '17820130'
NOTE_SQUARE = 'D50BEC6E'
NOTE_TRIANGLE = '0FD2D4FB'
TARGETS = 'F9253859'
TARGETS_SIDE = 128
MUSIC_QUAVER = '197E63B7'
MUSIC_CROTCHET = '36F32297'
MUSIC_SMALL = '56231C2F'
MUSIC_PAIR = 'C7E9888D'
MUSIC_BEAMED = 'E203437B'
MUSIC_BEAMED_SMALL = 'F09E2755'
ARC_BLUE = 'B8F13842'
ARC_WHITE = 'BB2E504A'
ARC_W, ARC_H = 128, 64


# ---- drawing
class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w * SCALE, h * SCALE
        self.data = bytearray(self.w * self.h * 4)

    def paint(self, polys, colour, alpha=255, clip=None):
        """Fill the union of polygons given in the game's pixels. Colour is a tuple or fn(x, y)."""
        polys = [[(x * SCALE, y * SCALE) for x, y in p] for p in polys if len(p) >= 3]
        if not polys:
            return
        xs = [x for p in polys for x, _ in p]
        ys = [y for p in polys for _, y in p]
        x0, x1 = max(0, int(min(xs))), min(self.w, int(max(xs)) + 2)
        y0, y1 = max(0, int(min(ys))), min(self.h, int(max(ys)) + 2)
        if clip:
            x0, y0 = max(x0, clip[0] * SCALE), max(y0, clip[1] * SCALE)
            x1, y1 = min(x1, clip[2] * SCALE), min(y1, clip[3] * SCALE)
        if x1 <= x0 or y1 <= y0:
            return
        width = x1 - x0
        fn = colour if callable(colour) else None
        for y in range(y0, y1):
            total = [0.0] * width
            for poly in polys:
                row = [0.0] * width
                for s in range(SUBSAMPLES):
                    sy = y + (s + 0.5) / SUBSAMPLES
                    crossings = []
                    for i, (ax, ay) in enumerate(poly):
                        bx, by = poly[(i + 1) % len(poly)]
                        if (ay <= sy < by) or (by <= sy < ay):
                            crossings.append(ax + (sy - ay) * (bx - ax) / (by - ay))
                    crossings.sort()
                    for k in range(0, len(crossings) - 1, 2):
                        a, b = max(crossings[k], x0), min(crossings[k + 1], x1)
                        if b <= a:
                            continue
                        ia, ib = int(a), int(b)
                        if ia == ib:
                            row[ia - x0] += (b - a) / SUBSAMPLES
                            continue
                        row[ia - x0] += (ia + 1 - a) / SUBSAMPLES
                        for ix in range(ia + 1, ib):
                            row[ix - x0] += 1 / SUBSAMPLES
                        if ib < x1:
                            row[ib - x0] += (b - ib) / SUBSAMPLES
                for i, v in enumerate(row):
                    if v > total[i]:
                        total[i] = v
            base = (y * self.w + x0) * 4
            for i, v in enumerate(total):
                if v <= 0:
                    continue
                if fn:
                    c = fn((x0 + i) / SCALE, y / SCALE)
                    r, g, b = c[:3]
                    a = c[3] if len(c) > 3 else alpha
                else:
                    r, g, b = colour[:3]
                    a = alpha
                cover = min(1.0, v) * a / 255
                at = base + i * 4
                under = self.data[at + 3] / 255
                out = cover + under * (1 - cover)
                if out <= 0:
                    continue
                k = under * (1 - cover)
                self.data[at] = int((r * cover + self.data[at] * k) / out)
                self.data[at + 1] = int((g * cover + self.data[at + 1] * k) / out)
                self.data[at + 2] = int((b * cover + self.data[at + 2] * k) / out)
                self.data[at + 3] = int(out * 255)

    def fill_rect(self, x, y, w, h, colour):
        """Replace a rectangle outright."""
        px = bytes(colour)
        for row in range(y * SCALE, (y + h) * SCALE):
            at = (row * self.w + x * SCALE) * 4
            self.data[at:at + w * SCALE * 4] = px * (w * SCALE)

    def write(self, path):
        """Rows bottom first, the order the game keeps its textures in."""
        with open(path, 'wb') as f:
            f.write(struct.pack('<II', self.w, self.h))
            for y in range(self.h - 1, -1, -1):
                f.write(self.data[y * self.w * 4:(y + 1) * self.w * 4])


def rect(x, y, w, h):
    return [(x, y), (x + w, y), (x + w, y + h), (x, y + h)]


def circle(cx, cy, r, steps=72):
    return [(cx + r * math.cos(i * math.tau / steps), cy + r * math.sin(i * math.tau / steps)) for i in range(steps)]


def ring(cx, cy, r_out, r_in, steps=96):
    """One polygon whose hole comes from filling by crossings, joined by an edge taken twice."""
    outer = circle(cx, cy, r_out, steps)
    inner = circle(cx, cy, r_in, steps)
    return [outer + outer[:1] + inner[:1] + inner[:0:-1] + inner[:1] + outer[:1]]


def segment(ax, ay, bx, by, width):
    dx, dy = bx - ax, by - ay
    length = math.hypot(dx, dy) or 1
    nx, ny = -dy / length * width / 2, dx / length * width / 2
    return [(ax + nx, ay + ny), (bx + nx, by + ny), (bx - nx, by - ny), (ax - nx, ay - ny)]


def polyline(points, width):
    """A stroke with round joins and ends, so that corners at any angle stay clean."""
    out = [segment(*points[i], *points[i + 1], width) for i in range(len(points) - 1)]
    if width > 1.2:
        out += [circle(x, y, width / 2, 16) for x, y in points]
    return out


def gradient(y0, y1, stops):
    """Colour by height, between (position, colour) stops."""
    def fn(_x, y):
        t = 0 if y1 == y0 else max(0.0, min(1.0, (y - y0) / (y1 - y0)))
        for (a, ca), (b, cb) in zip(stops, stops[1:]):
            if t <= b:
                k = 0 if b == a else (t - a) / (b - a)
                return tuple(int(ca[i] + (cb[i] - ca[i]) * k) for i in range(len(ca)))
        return stops[-1][1]
    return fn


# ---- letters: squared strokes on a grid 6 wide and 8 high, in the game's style
GLYPHS = {
    '0': [[(0, 1), (1, 0), (5, 0), (6, 1), (6, 7), (5, 8), (1, 8), (0, 7), (0, 1)]],
    '1': [[(2, 1), (3.5, 0), (3.5, 8)]],
    '2': [[(0, 1), (1, 0), (5, 0), (6, 1), (6, 3), (5, 4), (1, 4), (0, 5), (0, 8), (6, 8)]],
    '3': [[(0, 0), (5, 0), (6, 1), (6, 3), (5, 4), (2, 4)], [(5, 4), (6, 5), (6, 7), (5, 8), (0, 8)]],
    '4': [[(5, 8), (5, 0), (0, 5), (0, 6), (6, 6)]],
    '5': [[(6, 0), (0, 0), (0, 4), (5, 4), (6, 5), (6, 7), (5, 8), (0, 8)]],
    '6': [[(6, 0), (1, 0), (0, 1), (0, 7), (1, 8), (5, 8), (6, 7), (6, 5), (5, 4), (0, 4)]],
    '7': [[(0, 0), (6, 0), (6, 1), (2.5, 8)]],
    '8': [[(1, 4), (0, 3), (0, 1), (1, 0), (5, 0), (6, 1), (6, 3), (5, 4), (1, 4), (0, 5), (0, 7), (1, 8), (5, 8), (6, 7), (6, 5), (5, 4)]],
    '9': [[(0, 8), (5, 8), (6, 7), (6, 1), (5, 0), (1, 0), (0, 1), (0, 3), (1, 4), (6, 4)]],
    '+': [[(0, 4), (6, 4)], [(3, 1), (3, 7)]],
    '!': [[(3, 0), (3, 4.2)], [(3, 7.6), (3, 8)]],
    'A': [[(0, 8), (0, 1), (1, 0), (5, 0), (6, 1), (6, 8)], [(0, 5), (6, 5)]],
    'B': [[(0, 0), (5, 0), (6, 1), (6, 3), (5, 4), (0, 4)], [(5, 4), (6, 5), (6, 7), (5, 8), (0, 8), (0, 0)]],
    'C': [[(6, 1), (5, 0), (1, 0), (0, 1), (0, 7), (1, 8), (5, 8), (6, 7)]],
    'D': [[(0, 0), (5, 0), (6, 1), (6, 7), (5, 8), (0, 8), (0, 0)]],
    'E': [[(6, 0), (0, 0), (0, 8), (6, 8)], [(0, 4), (5, 4)]],
    'F': [[(6, 0), (0, 0), (0, 8)], [(0, 4), (5, 4)]],
    'G': [[(6, 1), (5, 0), (1, 0), (0, 1), (0, 7), (1, 8), (5, 8), (6, 7), (6, 4), (3, 4)]],
    'H': [[(0, 0), (0, 8)], [(6, 0), (6, 8)], [(0, 4), (6, 4)]],
    'I': [[(3, 0), (3, 8)]],
    'L': [[(0, 0), (0, 8), (6, 8)]],
    'M': [[(0, 8), (0, 0), (3, 4), (6, 0), (6, 8)]],
    'N': [[(0, 8), (0, 0), (6, 8), (6, 0)]],
    'O': [[(0, 1), (1, 0), (5, 0), (6, 1), (6, 7), (5, 8), (1, 8), (0, 7), (0, 1)]],
    'R': [[(0, 8), (0, 0), (5, 0), (6, 1), (6, 3), (5, 4), (0, 4)], [(3, 4), (6, 8)]],
    'S': [[(6, 1), (5, 0), (1, 0), (0, 1), (0, 3), (1, 4), (5, 4), (6, 5), (6, 7), (5, 8), (1, 8), (0, 7)]],
    'T': [[(0, 0), (6, 0)], [(3, 0), (3, 8)]],
    'W': [[(0, 0), (1, 8), (3, 3), (5, 8), (6, 0)]],
    'U': [[(0, 0), (0, 7), (1, 8), (5, 8), (6, 7), (6, 0)]],
    'P': [[(0, 8), (0, 0), (5, 0), (6, 1), (6, 3), (5, 4), (0, 4)]],
    'K': [[(0, 0), (0, 8)], [(0, 4), (2, 4), (6, 0)], [(2, 4), (6, 8)]],
    'x': [[(0.3, 1.2), (5.7, 6.8)], [(5.7, 1.2), (0.3, 6.8)]],
    '-': [[(1, 4), (5, 4)]],
    'Ü': [[(0, 2.2), (0, 7), (1, 8), (5, 8), (6, 7), (6, 2.2)], [(1.6, 0), (1.7, 0)], [(4.3, 0), (4.4, 0)]],
    'Y': [[(0, 0), (3, 4), (6, 0)], [(3, 4), (3, 8)]],
}
GRID_W, GRID_H = 6, 8


def text_strokes(text, x, y, w, h, stroke):
    """Fit a line of letters inside a box, inset by half a stroke."""
    n = len(text)
    gap = 1.6
    units = n * GRID_W + (n - 1) * gap
    inner_w, inner_h = w - stroke, h - stroke
    ux = inner_w / units
    uy = inner_h / GRID_H
    out = []
    for i, ch in enumerate(text):
        ox = x + stroke / 2 + i * (GRID_W + gap) * ux
        for line in GLYPHS.get(ch, []):
            pts = [(ox + px * ux, y + stroke / 2 + py * uy) for px, py in line]
            out += polyline(pts, stroke)
    return out


def shiny_text(canvas, text, box, stroke, stops, edge=(30, 35, 55), edge_width=1.2, alpha=255):
    x, y, w, h = box
    inner = (x + edge_width, y + edge_width, w - 2 * edge_width, h - 2 * edge_width)
    canvas.paint(text_strokes(text, *inner, stroke + 2 * edge_width), edge, alpha)
    canvas.paint(text_strokes(text, *inner, stroke), gradient(y, y + h, stops), alpha)


# ---- the song screen's sprites
def band_top(c, x, y, tint, alpha_main, alpha_thin):
    c.paint([[(x, y), (x + 278, y), (x + 260, y + 16), (x, y + 16)]], tint, alpha_main)
    c.paint([[(x + 278, y + 1), (x + 480, y + 1), (x + 480, y + 16), (x + 264, y + 16)]], tint, alpha_thin)
    c.paint(polyline([(x, y + 19), (x + 216, y + 19)], 0.8), tint, 230)
    c.paint(polyline([(x + 252, y + 22), (x + 278, y + 1), (x + 480, y + 1)], 0.8), tint, 230)
    c.paint(polyline([(x + 273, y + 8), (x + 301, y + 8)], 1), tint, 255)
    for i in range(10):
        c.paint([rect(x + 218 + i * 3.4, y + 18.6, 1.8, 0.8)], tint, 230)
        c.paint([rect(x + 303 + i * 2.8, y + 7.6, 1.6, 0.8)], tint, 230)
    for i in range(6):
        ox = x + 336 + i * 5
        c.paint(polyline([(ox, y + 5.5), (ox + 2.6, y + 8.3), (ox, y + 11)], 1.1), tint, 255)


# Where the ring around the level is open, in degrees, with 0 to the right and 90 down.
RING_OPENINGS = ((-130, -52), (-40, 0), (20, 60), (70, 125), (135, 160), (170, 217))
RING_X, RING_Y = 28, 29
RING_INNER, RING_BAND, RING_DARK, RING_OPEN, RING_OUTER = 20.5, 22, 24.2, 27.2, 29
SMALL_RING_X, SMALL_RING_Y = 56.5, 33.5


def sector(cx, cy, r_in, r_out, from_deg, to_deg):
    steps = max(2, int(abs(to_deg - from_deg) / 4))
    angles = [math.radians(from_deg + (to_deg - from_deg) * i / steps) for i in range(steps + 1)]
    return ([(cx + r_out * math.cos(a), cy + r_out * math.sin(a)) for a in angles] +
            [(cx + r_in * math.cos(a), cy + r_in * math.sin(a)) for a in reversed(angles)])


def clear_ring(c, cx, cy, r_in, r_out):
    """Nothing at all between these distances."""
    for py in range(int((cy - r_out) * SCALE), int((cy + r_out) * SCALE) + 1):
        for px in range(int((cx - r_out) * SCALE), int((cx + r_out) * SCALE) + 1):
            if r_in <= math.hypot((px + 0.5) / SCALE - cx, (py + 0.5) / SCALE - cy) <= r_out:
                c.data[(py * c.w + px) * 4 + 3] = 0


def band_bottom(c, x, y, tint, inner, band, alpha_main):
    cx, cy = x + RING_X, y + RING_Y
    c.paint([[(x + 57, y + 32), (x + 382, y + 32), (x + 364, y + 50), (x + 57, y + 50)]], tint, THIN_ALPHA)
    c.paint([[(x + 382, y + 32), (x + 480, y + 32), (x + 480, y + 54), (x + 360, y + 54)]], tint, alpha_main)
    c.paint(polyline([(x + 52, y + 32), (x + 382, y + 32)], 0.8), tint, 230)
    c.paint(polyline([(x + 52, y + 50), (x + 362, y + 50)], 1), tint, 255)
    c.paint(polyline([(x + 362, y + 50), (x + 386, y + 25), (x + 480, y + 25)], 0.8), tint, 230)
    c.paint(polyline([(x + 400, y + 32), (x + 404, y + 29), (x + 480, y + 29)], 0.8), tint, 230)

    # All the dark parts at once, so that where they touch they do not darken each other.
    dark = [sector(cx, cy, RING_BAND, RING_DARK, 0, 360), sector(cx, cy, RING_OPEN, RING_OUTER, 0, 360)]
    edges = RING_OPENINGS + ((RING_OPENINGS[0][0] + 360, 0),)
    for (_, after), (before, _) in zip(edges, edges[1:]):
        dark.append(sector(cx, cy, RING_DARK - 0.2, RING_OPEN + 0.2, after, before))
    # The corner to the left and below, the strip along the bottom, and the tab at the top left.
    arc = [(cx + RING_OUTER * math.cos(math.radians(a)), cy + RING_OUTER * math.sin(math.radians(a))) for a in range(172, 90, -6)]
    dark.append([(x, y + 35)] + arc + [(cx, y + 54), (x, y + 54)])
    dark.append([(x + 40, y + 51), (x + 363, y + 51), (x + 360, y + 54), (x + 40, y + 54)])
    dark.append([(x + 56, y + 37), (x + 58, y + 37), (x + 58, y + 50), (x + 47, y + 50)])
    dark.append(circle(x + 4.5, y + 8.5, 3))
    dark.append(sector(x + SMALL_RING_X, y + SMALL_RING_Y, 4, 5.5, 0, 360))
    c.paint(dark, tint, alpha_main, clip=(x, y, x + 480, y + 54))

    clear_ring(c, x + SMALL_RING_X, y + SMALL_RING_Y, 2, 4)
    c.paint([circle(x + SMALL_RING_X, y + SMALL_RING_Y, 2)], tint, alpha_main)

    c.paint([circle(cx, cy, RING_INNER)], inner, alpha_main)
    c.paint(ring(cx, cy, RING_BAND, RING_INNER), band, 223)


def led_pattern(colour):
    """Blue water with the game's lattice of small lights: a darker line every third pixel."""
    def fn(x, y):
        c = colour(x, y)
        if x % 3 < 1 or y % 3 < 1:
            return (max(0, c[0] - 6), max(0, c[1] - 25), max(0, c[2] - 6)) + tuple(c[3:])
        return c
    return fn


WATER = [(0.0, (70, 186, 245)), (0.4, (16, 165, 240)), (0.75, (3, 130, 236)), (1.0, (2, 95, 226))]
FULL_BRIGHT = [(0.0, (150, 225, 250)), (0.5, (120, 205, 248)), (1.0, (92, 170, 245))]
FULL_DIM = [(0.0, (50, 110, 170)), (0.5, (20, 70, 140)), (1.0, (2, 30, 120))]
WARN = [(0.0, (250, 60, 110)), (1.0, (225, 20, 80))]


def level_circle(c, rect_xywh, level, stops):
    x, y, w, h = rect_xywh
    cx, cy, r = x + w / 2, y + h / 2, h / 2 - 0.5
    top = y + h - level
    # The circle below the water line.
    points = []
    for i in range(72):
        a = i * math.tau / 72
        px, py = cx + r * math.cos(a), cy + r * math.sin(a)
        points.append((px, max(py, top)))
    c.paint([points], led_pattern(gradient(top, y + h, stops)))


LED_PITCH = 3
LED_LIGHT = (133, 240, 244)
LED_LIGHT_ALPHA = 215
LED_DIM_ALPHA = 120
LED_EDGE = (114, 215, 247)
LED_EDGE_ALPHA = 112
PINK_LIGHT = (255, 178, 206)
PINK_EDGE = (255, 106, 180)
PINK_EDGE_ALPHA = 141

# The icons beside the level, light by light.
ICON_ARROW = ('.............',
              '......#......',
              '.....###.....',
              '....#####....',
              '...#######...',
              '..#########..',
              '.....###.....',
              '.....###.....',
              '.....###.....',
              '.....###.....')
ICON_NORMAL = ('.............',
               '......#......',
               '......##.....',
               '......###....',
               '..########...',
               '..#########..',
               '..#########..',
               '..########...',
               '......###....',
               '......##.....',
               '......#......')
ICON_NOTE = ('.............',
             '......#......',
             '......##.....',
             '......###....',
             '......####...',
             '......#.##...',
             '......#..#...',
             '......#.#....',
             '....###......',
             '...####......',
             '...###.......')
ICON_DOUBLE = ('............',
               '............',
               '...##..###..',
               '...##.#####.',
               '...##.##.##.',
               '...##....##.',
               '...##..####.',
               '...##..##...',
               '............',
               '...##..##...',
               '...##..##...')
ICON_EXCLAMATION = ('.............',
                    '.....###.....',
                    '.....###.....',
                    '.....###.....',
                    '.....###.....',
                    '.....###.....',
                    '.....###.....',
                    '.....###.....',
                    '.............',
                    '.....###.....',
                    '.....###.....')


def lights(c, x, y, rows, light=LED_LIGHT, edge=LED_EDGE, edge_alpha=LED_EDGE_ALPHA, dim_row=None, left=0):
    """A square of 2 pixels for each light, with a fainter pixel around it."""
    plates, cells, dim = [], [], []
    for j, row in enumerate(rows):
        for i, on in enumerate(row):
            if on != '#':
                continue
            px, py = x + left + i * LED_PITCH, y + 1 + j * LED_PITCH
            plates.append(rect(px - 0.5, py - 0.5, LED_PITCH, LED_PITCH))
            (dim if dim_row is not None and j % 5 == dim_row else cells).append(rect(px, py, 2, 2))
    c.paint(plates, edge, edge_alpha)
    c.paint(cells, light, 255 if light == PINK_LIGHT else LED_LIGHT_ALPHA)
    c.paint(dim, light, LED_DIM_ALPHA)


def sprites(c, r):
    for name, tint, hole in (('pv_eleup.png', DARK, None), ('pv_eleup_warn.png', RED, None)):
        x, y, _, _ = r[name]
        band_top(c, x, y, tint, BAND_ALPHA if tint == DARK else 210, THIN_ALPHA if tint == DARK else 120)
    x, y, _, _ = r['pv_eledn.png']
    band_bottom(c, x, y, DARK, (31, 34, 57), (34, 38, 64), BAND_ALPHA)
    x, y, _, _ = r['pv_eledn_warn.png']
    band_bottom(c, x, y, RED, (70, 14, 34), (120, 18, 50), 210)

    gauges = ['pv_gauge01_min.png'] + ['pv_gauge%02d.png' % n for n in range(2, 15)]
    for n, name in enumerate(gauges):
        level_circle(c, r[name], 4 + n * 2.6, WATER)
    level_circle(c, r['pv_gauge15_max01.png'], 37, WATER)
    level_circle(c, r['pv_gauge16_max02.png'], 37, FULL_BRIGHT)
    level_circle(c, r['pv_gauge17_max03.png'], 37, FULL_DIM)
    for n, name in enumerate(('pv_gauge18_wn01.png', 'pv_gauge19_wn02.png', 'pv_gauge20_wn03.png')):
        level_circle(c, r[name], 4 + n * 3.2, WARN)

    # A darker row climbs the arrow, one row a frame.
    for n in range(1, 6):
        x, y, _, _ = r['pv_elicon_arr%02d.png' % n]
        lights(c, x, y, ICON_ARROW, dim_row=(5 - n + 1) % 5)
    x, y, _, _ = r['pv_elicon_normal.png']
    lights(c, x, y, ICON_NORMAL)
    x, y, _, _ = r['pv_elicon_note.png']
    lights(c, x, y, ICON_NOTE)
    x, y, _, _ = r['pv_elicon_double.png']
    lights(c, x, y, ICON_DOUBLE, left=1)
    x, y, _, _ = r['pv_elicon_exc.png']
    lights(c, x, y, ICON_EXCLAMATION, PINK_LIGHT, PINK_EDGE, PINK_EDGE_ALPHA)

    ranks = (('pv_rk_cool01.png', 'COOL!', [(0, (255, 255, 240)), (0.5, (250, 225, 90)), (1, (225, 165, 10))]),
             ('pv_rk_cool02.png', 'COOL!', [(0, (255, 180, 110)), (0.5, (255, 110, 30)), (1, (235, 70, 10))]),
             ('pv_rk_fine01.png', 'FINE', [(0, (255, 255, 255)), (0.5, (170, 220, 250)), (1, (70, 140, 200))]),
             ('pv_rk_fine02.png', 'FINE', [(0, (110, 170, 180)), (1, (45, 90, 120))]),
             ('pv_rk_safe.png', 'SAFE', [(0, (200, 250, 200)), (0.5, (70, 210, 110)), (1, (20, 140, 70))]),
             ('pv_rk_sad.png', 'SAD', [(0, (190, 220, 250)), (0.5, (80, 140, 220)), (1, (20, 70, 170))]),
             ('pv_rk_worst.png', 'WORST', [(0, (240, 190, 255)), (0.5, (190, 80, 235)), (1, (135, 25, 190))]))
    for name, text, stops in ranks:
        x, y, w, h = r[name]
        shiny_text(c, text, (x + 1, y + 2, w - 2, h - 4), 2.4, stops)

    x, y, w, h = r['pv_titleicon.png']
    c.paint(polyline([(x + 1, y + 1), (x + w - 1, y + 1), (x + w - 1, y + h - 1), (x + 1, y + h - 1), (x + 1, y + 1)], 1), (240, 245, 250))
    c.paint(polyline([(x + 5, y + 8), (x + 5, y + 3), (x + 8, y + 5)], 1.1), (240, 245, 250))
    c.paint([circle(x + 4, y + 8, 1.4, 16)], (240, 245, 250))


# ---- numbers and words
PALETTES = (('combonum', [(0, (255, 255, 200)), (0.5, (255, 225, 40)), (1, (205, 145, 5))]),
            ('scorenum', [(0, (250, 252, 255)), (0.5, (190, 200, 208)), (1, (120, 130, 140))]),
            ('chancenum', [(0, (255, 215, 240)), (0.5, (255, 100, 190)), (1, (215, 40, 150))]),
            ('plusnum', [(0, (245, 250, 250)), (0.5, (185, 200, 205)), (1, (115, 135, 140))]))
PINK = (255, 95, 205)


def numbers(c, r):
    for kind, stops in PALETTES:
        for digit in '0123456789':
            x, y, w, h = r['pv_%s_%s.png' % (kind, digit)]
            shiny_text(c, digit, (x + 3, y + 2, w - 6, h - 4), 1.7, stops, edge_width=0.6)
    x, y, w, h = r['pv_plusnum_plus.png']
    shiny_text(c, '+', (x + 3, y + 2, w - 6, h - 4), 1.7, PALETTES[3][1], edge_width=0.6)

    x, y, w, h = r['pv_combowin.png']
    c.paint([rect(x + 12, y + 2, w - 14, h - 5)], lambda px, py: (150, 170, 175, max(0, int(205 * (1 - (px - x) / w)))))
    white = (250, 252, 255)
    c.paint(polyline([(x, y + h - 2), (x + w - 2, y + h - 2)], 1), white)
    c.paint(polyline([(x + 3, y + 1), (x + 28, y + 1)], 0.8), white)
    c.paint(polyline([(x, y + h - 2), (x + 8, y + 2), (x + 13, y + 2), (x + 13, y + h - 5), (x + 6, y + h - 5)], 1.1), white)

    x, y, w, h = r['pv_combo.png']
    shiny_text(c, 'COMBO', (x + 55, y + 1, 40, 9), 1.4, [(0, white), (1, (205, 215, 225))], (20, 25, 35), 0.6)

    x, y, w, h = r['pv_icon_chance.png']
    c.paint(polyline([(x + 2, y + 2), (x + w - 2, y + 2)], 0.8), PINK)
    c.paint(polyline([(x + 2, y + h - 2), (x + w - 2, y + h - 2)], 0.8), PINK)
    shiny_text(c, 'CHANCE TIME', (x + 6, y + 6, w - 12, h - 12), 1.3, [(0, (255, 160, 230)), (1, PINK)], (60, 10, 60), 0.3)

    for name, text in (('pv_chance_start.png', 'CHANCE TIME START'), ('pv_chance_end.png', 'CHANCE TIME END')):
        x, y, w, h = r[name]

        def band(px, _py, x=x, w=w):
            t = min(1.0, min(px - x, x + w - px) / 42)
            return (135, 25, 120, int(max(0.0, t) * 225))

        c.paint([rect(x, y + 2, w, h - 4)], band)

        def edge_line(px, _py, x=x, w=w):
            t = min(1.0, min(px - x, x + w - px) / 42)
            return PINK + (int(max(0.0, t) * 255),)

        c.paint(polyline([(x, y + 2), (x + w, y + 2)], 0.9), edge_line)
        c.paint(polyline([(x, y + h - 2), (x + w, y + h - 2)], 0.9), edge_line)
        tw = len(text) * 10
        shiny_text(c, text, (x + (w - tw) / 2, y + 6, tw, h - 12), 2, [(0, (255, 170, 235)), (1, PINK)], (70, 10, 70), 0.5)
    for name in ('pv_chance_blkup.png', 'pv_chance_blkdn.png'):
        x, y, w, h = r[name]
        c.fill_rect(x, y, w, h, (0, 0, 0, 255))


# ---- the four symbols
# The game shows only the middle of these textures, so a symbol has to keep to the same small
# area of it as the original, about 27 texels wide and a little off centre.
SHADOW = (0, 0, 0)
SHADOW_ALPHA = 90
SHADOW_OFFSET = 1.0


def note(path, draw):
    c = Canvas(NOTE_SIDE, NOTE_SIDE)
    draw(c)
    c.write(path)


# All four fill the same square around the same point, outline included, so none looks larger.
NOTE_MIDDLE_X = 32.5
NOTE_MIDDLE_Y = 34.0
NOTE_REACH = 12.5


def draw_circle(c):
    x, y = NOTE_MIDDLE_X, NOTE_MIDDLE_Y
    c.paint(ring(x + SHADOW_OFFSET, y + SHADOW_OFFSET, NOTE_REACH + 0.3, 6.5), SHADOW, SHADOW_ALPHA)
    c.paint(ring(x, y, NOTE_REACH, 5.6), (15, 10, 20))
    c.paint(ring(x, y, NOTE_REACH - 0.9, 6.6), gradient(y - 11, y + 11, [(0, (255, 190, 205)), (0.45, (255, 95, 130)), (1, (215, 15, 70))]))


def draw_cross(c):
    x, y = NOTE_MIDDLE_X, NOTE_MIDDLE_Y
    # Two straight bars, so that the ends of the arms are square. Corners of the bars reach past
    # their ends by half the width over the root of two, which the arm leaves room for.
    width = 6.4
    arm = NOTE_REACH - width / 2 / math.sqrt(2) + 0.25
    def bars(arm, width, shift=0.0):
        return [segment(x - arm + shift, y - arm + shift, x + arm + shift, y + arm + shift, width),
                segment(x + arm + shift, y - arm + shift, x - arm + shift, y + arm + shift, width)]
    c.paint(bars(arm, width, SHADOW_OFFSET), SHADOW, SHADOW_ALPHA)
    c.paint(bars(arm, width), (10, 15, 35))
    c.paint(bars(arm - 0.7, width - 2.0), gradient(y - 11, y + 11, [(0, (170, 240, 255)), (0.45, (70, 190, 255)), (1, (25, 110, 235))]))


def draw_square(c):
    x, y = NOTE_MIDDLE_X, NOTE_MIDDLE_Y
    width = 6.2
    half = NOTE_REACH - width / 2
    box = [(x - half, y - half), (x + half, y - half), (x + half, y + half), (x - half, y + half), (x - half, y - half)]
    shadow = [(a + SHADOW_OFFSET, b + SHADOW_OFFSET) for a, b in box]
    c.paint(polyline(shadow, width), SHADOW, SHADOW_ALPHA)
    c.paint(polyline(box, width), (25, 10, 30))
    c.paint(polyline(box, width - 2.0), gradient(y - 11, y + 11, [(0, (255, 195, 250)), (0.45, (250, 120, 235)), (1, (205, 55, 195))]))


def draw_triangle(c):
    x, y = NOTE_MIDDLE_X, NOTE_MIDDLE_Y
    # The outline takes these 2.75 across and 2.9 down, on each side, measured from what is drawn.
    half_across, half_down = NOTE_REACH - 2.75, NOTE_REACH - 2.9
    tri = [(x, y - half_down), (x + half_across, y + half_down), (x - half_across, y + half_down), (x, y - half_down)]
    shadow = [(a + SHADOW_OFFSET, b + SHADOW_OFFSET) for a, b in tri]
    c.paint(polyline(shadow, 5.8), SHADOW, SHADOW_ALPHA)
    c.paint(polyline(tri, 5.8), (5, 25, 20))
    c.paint(polyline(tri, 3.9), gradient(y - 11, y + 10, [(0, (190, 255, 235)), (0.45, (70, 240, 190)), (1, (15, 170, 130))]))


# ---- where the notes fly to: four shapes and the hand of the clock, on one sheet
TARGET_EDGE = (185, 182, 215)
TARGET_FILL = (40, 38, 86)
TARGET_RIM = (245, 245, 252)
HAND_EDGE = (16, 16, 34)


def draw_targets(c):
    # The game cuts these out with shapes of its own and draws them with no transparency, so
    # whatever is around a drawing can show at its edge. Around each goes its own edge colour.
    c.fill_rect(0, 0, TARGETS_SIDE, TARGETS_SIDE, TARGET_EDGE + (255,))
    c.fill_rect(63, 41, 20, 44, HAND_EDGE + (255,))
    # circle
    c.paint([circle(32, 32, 29.5, 96)], TARGET_RIM)
    c.paint([circle(32, 32, 27, 96)], TARGET_FILL)
    # triangle
    c.paint([[(65, 7), (125, 7), (95, 60)]], TARGET_RIM)
    c.paint([[(69.5, 9.5), (120.5, 9.5), (95, 55)]], TARGET_FILL)
    # square
    c.paint([rect(8, 72, 48, 48)], TARGET_RIM)
    c.paint([rect(10.5, 74.5, 43, 43)], TARGET_FILL)
    # plus
    plus = [(88, 64), (106, 64), (106, 87), (127, 87), (127, 104), (106, 104), (106, 127), (88, 127), (88, 104), (65, 104), (65, 87), (88, 87)]
    c.paint([plus], TARGET_RIM)
    inner = [(90.5, 66.5), (103.5, 66.5), (103.5, 89.5), (124.5, 89.5), (124.5, 101.5), (103.5, 101.5), (103.5, 124.5), (90.5, 124.5),
             (90.5, 101.5), (67.5, 101.5), (67.5, 89.5), (90.5, 89.5)]
    c.paint([inner], TARGET_FILL)
    # the hand: a wide base, a short neck, a wide head and its point
    hand = [(73, 49), (76, 54.5), (77, 55.5), (78, 57.5), (78, 59), (76, 59), (76, 70), (78, 70), (78, 80.5),
            (68, 80.5), (68, 70), (70, 70), (70, 59), (68, 59), (68, 57.5), (69, 55.5), (70, 54.5)]
    c.paint([hand], TARGET_RIM)
    c.paint([rect(72, 56.5, 2, 9.5)], HAND_EDGE)
    c.paint([rect(71, 73, 4, 4)], HAND_EDGE)
    c.paint([rect(72, 74, 2, 2)], TARGET_RIM)


# ---- notes and targets in other styles: letters as on Xbox and Nintendo pads, or arrow keys
def letter_strokes(letter, cx, cy, size, width):
    """A, B, X or Y, `size` high around (cx, cy), in strokes with round ends."""
    def at(px, py):
        return (cx + px * size, cy + py * size)
    if letter == 'A':
        return polyline([at(-0.36, 0.5), at(0, -0.5), at(0.36, 0.5)], width) + polyline([at(-0.2, 0.17), at(0.2, 0.17)], width * 0.9)
    if letter == 'X':
        return polyline([at(-0.33, -0.5), at(0.33, 0.5)], width) + polyline([at(0.33, -0.5), at(-0.33, 0.5)], width)
    if letter == 'Y':
        return polyline([at(-0.34, -0.5), at(0, 0.02), at(0.34, -0.5)], width) + polyline([at(0, 0.02), at(0, 0.5)], width)
    out = polyline([at(-0.32, -0.5), at(-0.32, 0.5)], width)
    for top, reach in ((-0.5, 0.03), (0.0, 0.06)):
        bowl = [at(-0.32, top)]
        bowl += [at(reach + 0.25 * math.sin(i * math.pi / 12), top + 0.25 - 0.25 * math.cos(i * math.pi / 12)) for i in range(13)]
        bowl.append(at(-0.32, top + 0.5))
        out += polyline(bowl, width)
    return out


# An arrow pointing up, as a shaft and a head, `size` long; turned for the other ways.
ARROW_TURNS = {'up': 0, 'right': 1, 'down': 2, 'left': 3}


def arrow_strokes(way, cx, cy, size, width):
    """Its strokes' middles fill a square `size` across, as wide_letter_strokes' do."""
    turn = ARROW_TURNS[way]

    def at(px, py):
        for _ in range(turn):
            px, py = -py, px
        return (cx + px * size, cy + py * size)
    return polyline([at(0, 0.5), at(0, -0.46)], width) + polyline([at(-0.5, 0.0), at(0, -0.5), at(0.5, 0.0)], width)


def wide_letter_strokes(letter, cx, cy, size, width):
    """A, X or Y with their strokes' middles reaching the sides of a square `size` across, so
    that with their strokes they fill the same square as the game's shapes do, and B as high but
    narrower, all in one width of stroke. The buttons in the hints keep the narrower ones of letter_strokes."""
    def at(px, py):
        return (cx + px * size, cy + py * size)
    if letter == 'A':
        return polyline([at(-0.5, 0.5), at(0, -0.5), at(0.5, 0.5)], width) + polyline([at(-0.28, 0.12), at(0.28, 0.12)], width)
    if letter == 'X':
        return polyline([at(-0.5, -0.5), at(0.5, 0.5)], width) + polyline([at(0.5, -0.5), at(-0.5, 0.5)], width)
    if letter == 'Y':
        return polyline([at(-0.5, -0.5), at(0, 0.04), at(0.5, -0.5)], width) + polyline([at(0, 0.04), at(0, 0.5)], width)
    # B is narrower than the others, as letters are: about 22 of their 25 across with its stroke.
    out = polyline([at(-0.42, -0.5), at(-0.42, 0.5)], width)
    # The upper bowl a little smaller than the lower, which reaches the right side.
    for top, reach, across in ((-0.5, 0.08, 0.30), (0.0, 0.10, 0.32)):
        bowl = [at(-0.42, top)]
        bowl += [at(reach + across * math.sin(i * math.pi / 12), top + 0.25 - 0.25 * math.cos(i * math.pi / 12)) for i in range(13)]
        bowl.append(at(-0.42, top + 0.5))
        out += polyline(bowl, width)
    return out


# X in every style is PlayStation's cross, as Mega Mix has it: two straight bars with square ends,
# reaching as far as draw_cross's do from a note's middle (10.5 of a sign 18.6 across).
CROSS_REACH = 0.564


def sign_strokes(sign, cx, cy, size, width, outermost=None):
    """`outermost` is the width of the widest layer drawn of the same sign: X's square ends come in
    by as much as its sides, so the edge round it is as thick at its ends as along them."""
    if sign == 'X':
        a = size * CROSS_REACH - ((outermost or width) - width) / 2 / math.sqrt(2)
        return [segment(cx - a, cy - a, cx + a, cy + a, width), segment(cx + a, cy - a, cx - a, cy + a, width)]
    return wide_letter_strokes(sign, cx, cy, size, width) if len(sign) == 1 else arrow_strokes(sign, cx, cy, size, width)


# Each colour from light to deep, as the game's own notes are shaded.
NOTE_GREEN = ((205, 255, 175), (95, 205, 60), (35, 140, 25))
RED_NOTE = ((255, 195, 185), (240, 70, 60), (170, 20, 25))
NOTE_BLUE = ((185, 225, 255), (55, 135, 240), (20, 70, 185))
NOTE_YELLOW = ((255, 248, 175), (252, 205, 45), (205, 140, 0))
NOTE_PINK = ((255, 195, 250), (250, 120, 235), (205, 55, 195))
TEAL_NOTE = ((190, 255, 235), (70, 240, 190), (15, 170, 130))
NOTE_CORAL = ((255, 190, 205), (255, 95, 130), (215, 15, 70))
NOTE_SKY = ((170, 240, 255), (70, 190, 255), (25, 110, 235))
# For each style, what each of the game's notes becomes, by where its button is, and its colour:
# Circle on the right, Cross below, Square on the left, Triangle above.
NOTE_STYLES = {
    'xbox': {'circle': ('B', RED_NOTE), 'cross': ('A', NOTE_GREEN), 'square': ('X', NOTE_BLUE), 'triangle': ('Y', NOTE_YELLOW)},
    'nintendo': {'circle': ('A', RED_NOTE), 'cross': ('B', NOTE_YELLOW), 'square': ('Y', NOTE_GREEN), 'triangle': ('X', NOTE_BLUE)},
    'keyboard': {'circle': ('right', NOTE_CORAL), 'cross': ('down', NOTE_SKY), 'square': ('left', NOTE_PINK), 'triangle': ('up', TEAL_NOTE)},
}
NOTE_FILES = {'circle': NOTE_CIRCLE, 'cross': NOTE_CROSS, 'square': NOTE_SQUARE, 'triangle': NOTE_TRIANGLE}
# Where each target is on the targets sheet, by quarter: column and row.
TARGET_QUARTERS = {'circle': (0, 0), 'triangle': (1, 0), 'square': (0, 1), 'cross': (1, 1)}
STYLE_TARGETS_SIDE = 256     # in the units of its texture coordinates; four game texels to each


def draw_styled_note(c, sign, colours):
    x, y = NOTE_MIDDLE_X, NOTE_MIDDLE_Y
    size, width = 2 * NOTE_REACH - 6.4, 6.4
    light, middle, deep = colours
    edge = tuple(v // 7 for v in deep)
    c.paint(sign_strokes(sign, x + SHADOW_OFFSET, y + SHADOW_OFFSET, size, width + 0.6, width + 0.6), SHADOW, SHADOW_ALPHA)
    c.paint(sign_strokes(sign, x, y, size, width, width + 0.6), edge)
    c.paint(sign_strokes(sign, x, y, size, width - 2.0, width + 0.6), gradient(y - 11, y + 11, [(0, light), (0.45, middle), (1, deep)]))


def draw_styled_targets(c, signs):
    """The targets: each sign drawn wide, a white rim round a dark inside, on a clear ground."""
    quarter = STYLE_TARGETS_SIDE // 2
    for button, (col, row) in TARGET_QUARTERS.items():
        sign = signs[button][0]
        x, y = col * quarter + quarter / 2, row * quarter + quarter / 2
        size, width = quarter * 0.66, quarter * 0.17
        c.paint(sign_strokes(sign, x, y, size, width + 13), HAND_EDGE, 170)
        c.paint(sign_strokes(sign, x, y, size, width + 9, width + 13), TARGET_RIM)
        c.paint(sign_strokes(sign, x, y, size, width, width + 13), TARGET_FILL)


def write_styled_targets(c, path):
    """Its size, its side in texture units, then its rows, bottom first."""
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', c.w, c.h, STYLE_TARGETS_SIDE))
        for y in range(c.h - 1, -1, -1):
            f.write(c.data[y * c.w * 4:(y + 1) * c.w * 4])


def note_styles(out):
    for style, signs in NOTE_STYLES.items():
        folder = os.path.join(out, 'notes', style)
        os.makedirs(folder, exist_ok=True)
        for button, (sign, colours) in signs.items():
            c = Canvas(NOTE_SIDE, NOTE_SIDE)
            draw_styled_note(c, sign, colours)
            c.write(os.path.join(folder, NOTE_FILES[button] + '.rgba'))
        c = Canvas(STYLE_TARGETS_SIDE, STYLE_TARGETS_SIDE)
        draw_styled_targets(c, signs)
        write_styled_targets(c, os.path.join(folder, 'targets.rgba'))
        print('notes: %s' % style)


# ---- music notes that drift across the room and menus
MUSIC_BLUE = (0, 190, 250)


def ellipse(cx, cy, rx, ry, turn=-0.35, steps=48):
    cos, sin = math.cos(turn), math.sin(turn)
    out = []
    for i in range(steps):
        a = i * math.tau / steps
        x, y = rx * math.cos(a), ry * math.sin(a)
        out.append((cx + x * cos - y * sin, cy + x * sin + y * cos))
    return out


def music_quaver(c, size=1.0, x=0.0, y=0.0):
    p = lambda a, b: (x + a * size, y + b * size)
    c.paint([ellipse(*p(22, 52), 11 * size, 7.5 * size),
             [p(30, 8), p(34, 8), p(34, 50), p(30, 52)],
             [p(30, 6), p(36, 6), p(40, 13), p(50, 22), p(54, 33), p(49, 42), p(50, 33), p(44, 24), p(34, 20)]], MUSIC_BLUE)


def music_crotchet(c):
    c.paint([ellipse(26, 53, 11, 7.5), [(34, 4), (37.5, 4), (37.5, 52), (34, 53)]], MUSIC_BLUE)


def music_pair(c, beams=1, size=1.0, x=0.0, y=0.0):
    p = lambda a, b: (x + a * size, y + b * size)
    shapes = [ellipse(*p(14, 53), 10 * size, 6.8 * size), ellipse(*p(44, 49), 10 * size, 6.8 * size),
              [p(20.5, 14), p(24, 14), p(24, 52), p(20.5, 54)], [p(50.5, 8), p(54, 8), p(54, 48), p(50.5, 50)],
              [p(20.5, 12), p(54, 5), p(54, 13), p(20.5, 20)]]
    if beams == 2:
        shapes.append([p(20.5, 23), p(54, 16), p(54, 23), p(20.5, 30)])
    c.paint(shapes, MUSIC_BLUE)


def draw_arc(c, colour):
    cx, cy, r_out, r_in = ARC_W / 2, ARC_H + 2, 62, 50
    outer, inner = [], []
    steps = 64
    for i in range(steps + 1):
        a = math.pi + i * math.pi / steps
        outer.append((cx + r_out * math.cos(a), cy + r_out * math.sin(a)))
        inner.append((cx + r_in * math.cos(a), cy + r_in * math.sin(a)))
    c.paint([outer + inner[::-1]], colour)
    # The small gap near the top, where the game's arc starts and ends.
    gap_at = math.pi * 1.53
    for r in (r_out + 1,):
        px, py = cx + r * math.cos(gap_at), cy + r * math.sin(gap_at)
        qx, qy = cx + (r_in - 1) * math.cos(gap_at), cy + (r_in - 1) * math.sin(gap_at)
        erase(c, px, py, qx, qy, 3)


def erase(c, ax, ay, bx, by, width):
    """Clear a thick line outright."""
    steps = int(math.hypot(bx - ax, by - ay) * SCALE) + 1
    half = int(width * SCALE / 2)
    for i in range(steps + 1):
        x = int((ax + (bx - ax) * i / steps) * SCALE)
        y = int((ay + (by - ay) * i / steps) * SCALE)
        for yy in range(max(0, y - half), min(c.h, y + half)):
            at = (yy * c.w + max(0, x - half)) * 4
            c.data[at:at + (min(c.w, x + half) - max(0, x - half)) * 4] = bytes((min(c.w, x + half) - max(0, x - half)) * 4)


# ---- NOW LOADING, a letter at a time
LOADING_LETTERS = 'ADGILNOW'
LOADING_CELL = 16


def draw_loading(c, _r):
    for i, letter in enumerate(LOADING_LETTERS):
        x, y = (i % 4) * LOADING_CELL, (i // 4) * LOADING_CELL
        c.paint([rect(x + 0.5, y + 0.5, LOADING_CELL - 1, LOADING_CELL - 1)], (12, 34, 40), 235)
        shiny_text(c, letter, (x + 3, y + 2.5, LOADING_CELL - 6, LOADING_CELL - 5), 1.6,
                   [(0, (235, 255, 250)), (1, (130, 225, 210))], (10, 30, 35), 0.3)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    archive = Archive(os.path.join(sys.argv[1], ARCHIVE))
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    for picture, draw in (('pv_sprite01.png', sprites), ('pv_num.png', numbers), ('loading.png', draw_loading)):
        w, h, pixels = decode_png(archive.read(picture))
        canvas = Canvas(w, h)
        draw(canvas, layout_of(archive, picture))
        name = '%08X.rgba' % colour_hash(pixels)
        canvas.write(os.path.join(out, name))
        print('%s: %s' % (picture, name))

    def sized(name, w, h, draw):
        c = Canvas(w, h)
        draw(c)
        c.write(os.path.join(out, name + '.rgba'))
        print('drawn: %s' % name)

    sized(TARGETS, TARGETS_SIDE, TARGETS_SIDE, draw_targets)
    sized(MUSIC_QUAVER, NOTE_SIDE, NOTE_SIDE, music_quaver)
    sized(MUSIC_CROTCHET, NOTE_SIDE, NOTE_SIDE, music_crotchet)
    sized(MUSIC_SMALL, NOTE_SIDE, NOTE_SIDE, lambda c: music_quaver(c, 0.5, 16, 16))
    sized(MUSIC_PAIR, NOTE_SIDE, NOTE_SIDE, music_pair)
    sized(MUSIC_BEAMED, NOTE_SIDE, NOTE_SIDE, lambda c: music_pair(c, 2))
    sized(MUSIC_BEAMED_SMALL, NOTE_SIDE, NOTE_SIDE, lambda c: music_pair(c, 2, 0.72, 9, 9))
    sized(ARC_BLUE, ARC_W, ARC_H, lambda c: draw_arc(c, (60, 205, 250)))
    sized(ARC_WHITE, ARC_W, ARC_H, lambda c: draw_arc(c, (250, 252, 255)))
    for name, draw in ((NOTE_CIRCLE, draw_circle), (NOTE_CROSS, draw_cross), (NOTE_SQUARE, draw_square), (NOTE_TRIANGLE, draw_triangle)):
        note(os.path.join(out, name + '.rgba'), draw)
        print('note: %s' % name)
    note_styles(out)


if __name__ == '__main__':
    main()
