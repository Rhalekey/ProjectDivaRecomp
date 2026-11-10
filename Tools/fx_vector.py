"""Draw the song screen's hit effects from code, at four times the size of the game's own.

    python Tools/fx_vector.py <folder of large textures>

The pictures a press sets off: rings of light, a gold flare, bubbles, sparks, stars, a crescent,
lightning. Each is defined below as shapes and gradients, the way an SVG would be, and written
under the hash of the game's own texture (see DIVA_DUMP_TEXTURES in src/host/main.c and
DIVA_LOG_DRAWS in the renderer, which told them apart), so the game draws these in their place.
Nothing is read from the game's pictures.

Most are drawn added to what is behind them, on black; those with see-through parts are marked.
They are drawn as the texture lies in memory, row by row from its first, as the dumps show them.
"""
import math
import os
import random
import struct
import sys

SIDE = 64
SCALE = 4
N = SIDE * SCALE
CENTRE = SIDE / 2


def smooth(t):
    t = 0.0 if t < 0 else 1.0 if t > 1 else t
    return t * t * (3 - 2 * t)


def stops_at(stops, r):
    """A radial gradient: (radius, (r, g, b, strength)) stops, eased between."""
    if r <= stops[0][0]:
        return stops[0][1]
    for (r0, c0), (r1, c1) in zip(stops, stops[1:]):
        if r <= r1:
            t = smooth((r - r0) / (r1 - r0))
            return tuple(a + (b - a) * t for a, b in zip(c0, c1))
    return stops[-1][1]


class Effect:
    """A 64 by 64 picture drawn at four times: light added on black, or colour over clear."""

    def __init__(self, clear=False):
        self.clear = clear
        self.px = [[0.0, 0.0, 0.0, 0.0] for _ in range(N * N)]

    def each(self, fn):
        """fn(x, y) in the game's pixels, from the top left, gives (r, g, b, strength) or None."""
        for j in range(N):
            y = (j + 0.5) / SCALE
            for i in range(N):
                v = fn((i + 0.5) / SCALE, y)
                if v:
                    self.add(j * N + i, v)

    def add(self, k, v):
        r, g, b, s = v
        if s <= 0:
            return
        p = self.px[k]
        if self.clear:
            # Over what is there, so overlapping parts do not go past full.
            a = min(1.0, s)
            out = a + p[3] * (1 - a)
            if out > 0:
                for c, x in enumerate((r, g, b)):
                    p[c] = (x * a + p[c] * p[3] * (1 - a)) / out
            p[3] = out
        else:
            p[0] += r * s
            p[1] += g * s
            p[2] += b * s

    def write(self, path):
        with open(path, 'wb') as f:
            f.write(struct.pack('<II', N, N))
            for j in range(N):
                row = bytearray()
                for i in range(N):
                    p = self.px[j * N + i]
                    if self.clear:
                        row += bytes((int(min(255, p[0])), int(min(255, p[1])), int(min(255, p[2])), int(min(1.0, p[3]) * 255)))
                    else:
                        row += bytes((int(min(255, p[0])), int(min(255, p[1])), int(min(255, p[2])), 255))
                f.write(row)


def polar(x, y):
    dx, dy = x - CENTRE, y - CENTRE
    return math.hypot(dx, dy), math.atan2(dy, dx)


def radial(e, stops, mask=None):
    def fn(x, y):
        r, a = polar(x, y)
        c = stops_at(stops, r)
        if mask:
            c = c[:3] + (c[3] * mask(r, a),)
        return c
    e.each(fn)


def glow(cx, cy, size, colour, strength=1.0):
    """A soft round light."""
    def fn(x, y):
        d2 = ((x - cx) ** 2 + (y - cy) ** 2) / (size * size)
        return colour + (strength * math.exp(-d2),) if d2 < 9 else None
    return fn


def star_arms(cx, cy, reach, width, colour, strength=1.0, turn=0.0):
    """Four thin points of light, tapering to nothing at `reach`."""
    ct, st = math.cos(turn), math.sin(turn)
    def fn(x, y):
        dx, dy = x - cx, y - cy
        u, v = dx * ct + dy * st, -dx * st + dy * ct
        best = 0.0
        for along, across in ((u, v), (v, u)):
            if abs(along) < reach:
                taper = (1 - abs(along) / reach) ** 1.6
                w = width * (0.35 + 0.65 * taper)
                best = max(best, taper * math.exp(-(across / w) ** 2))
        return colour + (strength * best,) if best > 0.002 else None
    return fn


def segment_light(points, width, colour, strength=1.0):
    """Light along a line through the points, soft at its edges."""
    segs = list(zip(points, points[1:]))
    def fn(x, y):
        best = 1e9
        for (ax, ay), (bx, by) in segs:
            vx, vy = bx - ax, by - ay
            t = max(0.0, min(1.0, ((x - ax) * vx + (y - ay) * vy) / (vx * vx + vy * vy)))
            best = min(best, math.hypot(x - ax - vx * t, y - ay - vy * t))
        return colour + (strength * math.exp(-(best / width) ** 2),) if best < width * 3 else None
    return fn


# A quaver drawn on a box 686 wide and 1280 high, as it shows (head at the bottom left): a tilted
# oval head, a thin stem, and a flag that leaves the stem's top thick and curls down to a point.
QUAVER_STEM = (330, 384, 0, 1015)
QUAVER_HEAD = (188, 1122, 205, 132, math.radians(-28))
QUAVER_FLAG = [(336, 0), (380, 95), (430, 190), (500, 290), (565, 375), (640, 470), (676, 560), (685, 650), (670, 760), (625, 860),
               (560, 960), (600, 870), (632, 770), (645, 660), (636, 570), (600, 490), (545, 425), (470, 365), (376, 315)]


def inside_polygon(poly, x, y):
    inside = False
    for (ax, ay), (bx, by) in zip(poly, poly[1:] + poly[:1]):
        if (ay > y) != (by > y) and x < ax + (y - ay) * (bx - ax) / (by - ay):
            inside = not inside
    return inside


def in_quaver(x, y):
    x0, x1, y0, y1 = QUAVER_STEM
    if x0 <= x <= x1 and y0 <= y <= y1:
        return True
    cx, cy, ra, rb, turn = QUAVER_HEAD
    dx, dy = x - cx, y - cy
    u, v = dx * math.cos(turn) + dy * math.sin(turn), -dx * math.sin(turn) + dy * math.cos(turn)
    if (u / ra) ** 2 + (v / rb) ** 2 <= 1:
        return True
    return inside_polygon(QUAVER_FLAG, x, y)


def quaver(colour, left, top, height):
    """The quaver `height` pixels high from (left, top) as it shows, its edges smoothed by looking
    at four points in each pixel. Drawn upside down, as the texture lies in memory."""
    k = 1280 / height
    def fn(x, y):
        shown_y = SIDE - y
        rx, ry = (x - left) * k, (shown_y - top) * k
        if rx < -20 or rx > 706 or ry < -20 or ry > 1300:
            return None
        step = k / SCALE / 4
        hits = sum(in_quaver(rx + (i - 1.5) * step, ry + (j - 1.5) * step) for i in range(4) for j in range(4))
        return colour + (hits / 16,) if hits else None
    return fn


# ---- the effects, by the hash of the texture each replaces --------------------------------------
def gold_ring():
    """A warm ring of light with a faint glow filling it."""
    e = Effect()
    radial(e, [(0, (255, 225, 160, 0.26)), (8, (255, 225, 160, 0.30)), (12, (255, 228, 165, 0.46)), (15, (255, 232, 170, 0.75)),
               (18, (255, 248, 185, 0.95)), (20, (255, 225, 150, 0.30)), (23, (255, 215, 140, 0.10)), (27, (255, 210, 130, 0.0))])
    return e


def thin_ring():
    """A thin pale ring, sharp outside and fading in."""
    e = Effect()
    radial(e, [(12, (250, 245, 200, 0.0)), (17, (250, 245, 200, 0.12)), (20, (250, 245, 200, 0.40)), (22.5, (250, 248, 205, 0.80)),
               (24, (250, 245, 200, 0.06)), (25.5, (250, 245, 200, 0.0))])
    return e


def blue_ring():
    """A blue ring, two bands at its edge."""
    e = Effect()
    radial(e, [(9, (140, 170, 250, 0.0)), (16, (140, 160, 250, 0.05)), (20, (130, 150, 245, 0.16)), (22.5, (125, 160, 245, 0.42)),
               (24, (130, 175, 250, 0.62)), (25.5, (125, 160, 240, 0.34)), (27, (130, 170, 250, 0.42)), (28.5, (130, 170, 250, 0.0))])
    return e


def rainbow_ring():
    """A soft ring with the rainbow across it: violet inside, pink, then green outside."""
    e = Effect()
    radial(e, [(7, (120, 90, 140, 0.0)), (10, (130, 100, 160, 0.20)), (13, (190, 150, 190, 0.48)), (15, (235, 175, 180, 0.68)),
               (17, (150, 210, 200, 0.58)), (19, (120, 200, 90, 0.42)), (22, (110, 190, 80, 0.16)), (26, (110, 190, 80, 0.0))])
    return e


def flame_ring():
    """A ring of gold fire, its flames pointing in."""
    e = Effect()
    rnd = random.Random(7)
    phase = [rnd.uniform(0, math.tau) for _ in range(3)]
    def mask(r, a):
        if r > 16.5:
            return 1.0
        flick = 0.5 + 0.5 * math.cos(22 * a + 1.3 * math.sin(3 * a + phase[0]) + phase[1])
        depth = 16.5 - r
        return max(0.0, 1.0 - depth / 10.0) * (0.45 + 0.55 * flick) + (0.15 if depth < 1.5 else 0.0)
    radial(e, [(5, (255, 150, 20, 0.0)), (9, (255, 160, 25, 0.12)), (12, (255, 175, 40, 0.45)), (14.5, (255, 200, 60, 0.85)),
               (16.5, (250, 235, 110, 0.95)), (17.6, (250, 240, 120, 0.05)), (18.2, (250, 240, 120, 0.0))], mask)
    return e


def gold_flare():
    """A dark middle edged with gold, and rays of light out from it."""
    e = Effect()
    rnd = random.Random(11)
    rays = [(rnd.uniform(0, math.tau), rnd.uniform(0.6, 1.0), rnd.uniform(0.08, 0.16)) for _ in range(20)]
    def mask(r, a):
        if r < 19:
            return 1.0
        best = 0.3
        for at, length, w in rays:
            d = math.atan2(math.sin(a - at), math.cos(a - at))
            # Each ray narrows as it goes out, and reaches as far as its length.
            reach = 19 + 14 * length
            narrow = w * max(0.2, 1 - (r - 19) / (reach - 19)) if r < reach else 0
            if narrow:
                best = max(best, 1.6 * length * math.exp(-(d / narrow) ** 2))
        return best
    radial(e, [(16, (230, 190, 70, 0.0)), (17.2, (240, 200, 75, 0.45)), (18.2, (245, 205, 80, 0.95)), (20, (230, 170, 50, 0.70)),
               (23, (220, 150, 40, 0.38)), (27, (210, 130, 20, 0.14)), (31, (200, 120, 10, 0.03)), (33, (200, 120, 10, 0.0))], mask)
    return e


def orange_dots_ring():
    """A ring of soft orange lights (see-through)."""
    e = Effect(clear=True)
    radial(e, [(8, (255, 205, 150, 0.0)), (11, (255, 205, 150, 0.30)), (14, (255, 205, 160, 0.70)), (16.5, (255, 200, 145, 0.68)),
               (19, (255, 190, 130, 0.36)), (22, (255, 185, 125, 0.12)), (25, (255, 180, 120, 0.0))],
           lambda r, a: 0.55 + 0.45 * math.cos(9 * a))
    return e


def blue_crescent():
    """A soft blue crescent along the bottom of a circle (see-through)."""
    e = Effect(clear=True)
    def mask(r, a):
        return smooth((math.sin(a) + 0.15) / 0.55)
    radial(e, [(12, (90, 150, 230, 0.0)), (14, (80, 145, 235, 0.35)), (17, (60, 140, 240, 0.80)), (19.5, (200, 230, 255, 0.95)),
               (21, (70, 150, 245, 0.85)), (24, (80, 140, 230, 0.45)), (26, (100, 140, 210, 0.10)), (27, (100, 140, 210, 0.0))], mask)
    return e


def target_rings():
    """Two pale rings that do not quite close, round a soft light (see-through)."""
    e = Effect(clear=True)
    def fn(x, y):
        r, a = polar(x, y)
        s = 0.30 * math.exp(-(r / 4.5) ** 2)
        for radius, width, start, alpha in ((15.5, 1.4, 0.4, 0.30), (22.5, 1.6, 3.6, 0.40)):
            gap = math.atan2(math.sin(a - start), math.cos(a - start))
            open_part = smooth((abs(gap) - 0.35) / 0.5)
            s = max(s, alpha * open_part * math.exp(-((r - radius) / width) ** 2))
        return (255, 255, 250, s)
    e.each(fn)
    return e


def three_sparkles():
    """A small four-pointed sparkle and two soft lights (see-through)."""
    e = Effect(clear=True)
    white = (255, 255, 248)
    for fn in (glow(20, 26, 2.6, white, 0.75), glow(39, 40, 3.2, white, 0.85), glow(39, 40, 7.5, white, 0.25),
               star_arms(40, 17, 6, 0.6, white, 0.9), glow(40, 17, 1.6, white, 0.8)):
        e.each(fn)
    return e


def white_star():
    """A white four-pointed star with a soft middle."""
    e = Effect()
    white = (240, 240, 240)
    for fn in (glow(32, 32, 3.6, white, 1.0), glow(32, 32, 9, (240, 225, 180), 0.22), star_arms(32, 32, 31, 2.4, white, 1.0)):
        e.each(fn)
    return e


def gold_star():
    """A gold four-pointed star, white in the middle (see-through)."""
    e = Effect(clear=True)
    for fn in (glow(32, 32, 10, (255, 205, 80), 0.35), star_arms(32, 32, 31, 2.5, (255, 215, 90), 1.0),
               glow(32, 32, 4.2, (255, 255, 240), 1.0)):
        e.each(fn)
    return e


def burst():
    """A bright middle with fine rays all round."""
    e = Effect()
    rnd = random.Random(5)
    white = (200, 195, 185)
    fns = [glow(32, 32, 5.5, (255, 240, 210), 0.75), glow(32, 32, 13, white, 0.12)]
    for k in range(20):
        a = k * math.tau / 20 + rnd.uniform(-0.12, 0.12)
        length = rnd.uniform(14, 30)
        fns.append(segment_light([(32 + 3 * math.cos(a), 32 + 3 * math.sin(a)), (32 + length * math.cos(a), 32 + length * math.sin(a))],
                                 0.45, white, rnd.uniform(0.25, 0.5)))
    for fn in fns:
        e.each(fn)
    return e


def streak():
    """A flat streak of orange light, white along its middle."""
    e = Effect()
    def fn(x, y):
        dx, dy = (x - 32) / 26, (y - 32)
        if abs(dx) >= 1:
            return None
        along = (1 - dx * dx) ** 2
        s = along * math.exp(-(dy / 1.6) ** 2)
        core = along ** 2 * math.exp(-(dy / 0.7) ** 2)
        return (255, 150 + 90 * core, 120 + 110 * core, s)
    e.each(fn)
    return e


def blue_dots():
    """Eight small blue lights in a ring."""
    e = Effect()
    for k in range(8):
        a = k * math.tau / 8 + 0.2
        e.each(glow(32 + 10 * math.cos(a), 32 + 10 * math.sin(a), 0.9, (90, 173, 255), 0.6))
    return e


def pink_sparks():
    """Small pink sparks scattered round (see-through)."""
    e = Effect(clear=True)
    rnd = random.Random(3)
    for _ in range(18):
        a, r = rnd.uniform(0, math.tau), rnd.uniform(12, 30)
        e.each(glow(32 + r * math.cos(a), 32 + r * math.sin(a), rnd.uniform(0.6, 1.2), (255, 205, 230), rnd.uniform(0.5, 0.9)))
    return e


def lightning(seed, reach, strength):
    """A jagged orange line of lightning."""
    e = Effect()
    rnd = random.Random(seed)
    points, x = [], 32 - reach
    while x < 32 + reach:
        points.append((x, 32 + rnd.uniform(-5, 5)))
        x += rnd.uniform(2.5, 5)
    points.append((32 + reach, 32))
    e.each(segment_light(points, 0.55, (240, 120, 70), strength))
    e.each(segment_light(points, 0.25, (255, 220, 200), strength * 0.8))
    return e


def note_bubble():
    """A glossy blue bubble with a music note in it (see-through outside it)."""
    e = Effect(clear=True)
    def body(x, y):
        r, a = polar(x, y)
        if r > 22.6:
            return None
        edge = smooth((22.6 - r) / 0.8)
        depth = (32 - y) / 22   # darker at the bottom as it shows, the top here
        base = (75 - 20 * depth, 50 - 15 * depth, 185 + 45 * (1 - abs(depth)))
        rim = smooth((r - 19.5) / 2.5)
        c = tuple(b + (t - b) * rim for b, t in zip(base, (110, 230, 240)))
        return c + (edge,)
    e.each(body)
    e.each(quaver((110, 235, 225), 25.5, 15.5, 28.0))
    for fn in (glow(24, 42, 4.5, (255, 255, 255), 0.55), glow(22.5, 43.5, 1.8, (255, 255, 255), 0.8), glow(38, 23, 2.5, (255, 255, 255), 0.25)):
        e.each(fn)
    return e


def purple_bubble():
    """A see-through bubble with a purple edge and a small blue ring in its middle."""
    e = Effect(clear=True)
    radial(e, [(0, (10, 40, 90, 0.30)), (8, (5, 40, 100, 0.36)), (14, (25, 45, 95, 0.45)), (20, (75, 60, 105, 0.58)),
               (24, (115, 80, 125, 0.62)), (27, (130, 85, 140, 0.68)), (28.3, (60, 40, 60, 0.12)), (29, (0, 0, 0, 0.0))])
    def ring(x, y):
        r, _ = polar(x, y)
        return (0, 120, 215, 0.85 * math.exp(-((r - 6) / 1.3) ** 2))
    e.each(ring)
    for fn in (glow(22, 21, 4, (255, 240, 255), 0.45), glow(20.5, 19.5, 1.5, (255, 255, 255), 0.7)):
        e.each(fn)
    return e


EFFECTS = {
    '8AE83035': gold_ring, '7257E193': thin_ring, 'D30C9FF3': blue_ring, '9475B341': rainbow_ring, 'FA6E02F3': flame_ring,
    'BEC17C17': gold_flare, 'B17ED4AC': orange_dots_ring, '086C1B5D': blue_crescent, 'CE3717D7': target_rings,
    'C1AFBC9F': three_sparkles, 'E1D55099': white_star, '0BC90FB1': gold_star, 'CE195847': burst, '7EF4133D': streak,
    '50AB07F7': blue_dots, 'E15DC97C': pink_sparks, '8DE115D3': lambda: lightning(1, 12, 0.9),
    'EA901619': lambda: lightning(2, 28, 0.8), '59474425': lambda: lightning(4, 20, 0.6), '415A9C4A': note_bubble,
    '104D5C9E': purple_bubble,
}


def main():
    if len(sys.argv) not in (2, 3):
        raise SystemExit(__doc__)
    out = sys.argv[1]
    only = sys.argv[2].split(',') if len(sys.argv) == 3 else None
    os.makedirs(out, exist_ok=True)
    for name, draw in EFFECTS.items():
        if only and name not in only:
            continue
        draw().write(os.path.join(out, name + '.rgba'))
        print('effect: %s' % name)


if __name__ == '__main__':
    main()
