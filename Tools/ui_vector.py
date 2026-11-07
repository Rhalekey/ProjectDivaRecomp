"""Draw the menus again, clean, at four times the size of the game's own.

    python Tools/ui_vector.py <folder holding PSP_GAME> <font file> <lang/xx_images.txt> <output folder>

The game's art is the mould. The pieces seen most are traced over it by hand, in straight lines,
with what pixels could not show put in. What is not traced yet is enlarged by following its edges,
with the dotted texture of its plates taken out (see smooth_enlarge in lang_images.py).

Each piece stays where the game's layout files say it goes, so the game goes on placing and
animating it as ever, and the renderer shows these in place of its pictures.

Everything is made twice: the output folder has the pictures for Circle confirming, and the
same folder with _x after its name has those that differ when Cross does.
"""
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hd_font import Drawer, FREE_FONT, GGO_METRICS_ADVANCE, load_font  # noqa: E402
from hud_vector import SCALE, Canvas, circle, polyline, rect, ring, segment  # noqa: E402
from lang_images import ARCHIVE, Archive, colour_hash, decode_png, layout_of, plate_rects, smooth_enlarge  # noqa: E402

TEAL = (0, 137, 147)
GREY = (100, 100, 100)
BAND = (247, 241, 251)
BAND_EDGE = (133, 139, 142)
WHITE = (255, 255, 255)
BUTTON_TEAL = (56, 175, 148)
WORD = (232, 234, 244)
WORD_EDGE = (42, 46, 66)
HINT_H = 11
SHADOW = (0, 0, 0)

ORNAMENT = (90, 90, 91)
ORNAMENT_ALPHA = 225
WINDOW_FILL = (26, 30, 34, 190)
WINDOW_LEFT = (102, 104, 109, 219)
WINDOW_RIGHT = (13, 15, 16, 219)
WINDOW_TOP = ((7, 105, 100, 255), (21, 157, 145, 255), (28, 216, 201, 255), (84, 226, 214, 255),
              (28, 216, 201, 255), (21, 157, 145, 255), (37, 42, 45, 195), (30, 33, 37, 191))
WINDOW_BOTTOM = ((30, 33, 37, 191), (37, 42, 45, 195), (21, 157, 145, 255), (28, 216, 201, 255),
                 (84, 226, 214, 255), (28, 216, 201, 255), (21, 157, 145, 255), (7, 105, 100, 255))
WINDOW_SIZE = 60
WINDOW_X, WINDOW_Y = 150, 139
BUTTON_WORD_AT = 18    # where a button's word starts, after its sign and colon
WINDOW_CUT = 3

# Letters on a grid of three by six, so that all of them on any sign are the same size.
LETTERS = {
    'S': ('###', '#.#', '#..', '###', '..#', '###'),
    'E': ('###', '#..', '#..', '###', '#..', '###'),
    'L': ('#..', '#..', '#..', '#..', '#..', '###'),
    'C': ('###', '#.#', '#..', '#..', '#.#', '###'),
    'T': ('###', '.#.', '.#.', '.#.', '.#.', '.#.'),
    'A': ('###', '#.#', '#.#', '###', '#.#', '#.#'),
    'R': ('###', '#.#', '#.#', '##.', '#.#', '#.#'),
}
LETTER_W, LETTER_H = 3, 6

# Pictures of the menus with no words in them here, enlarged and cleaned.
CLEAN_ONLY = ('menu_common.png', 'menu_common2.png', 'menu_basic03.png', 'menu_basic04.png', 'result_win_nm.png', 'result_win_edit.pn')


# ---- drawing
def paste(sheet, piece, x, y):
    """Over what is there, straight: pieces do not overlap."""
    for row in range(piece.h):
        src = row * piece.w * 4
        dst = ((y * SCALE + row) * sheet.w + x * SCALE) * 4
        sheet.data[dst:dst + piece.w * 4] = piece.data[src:src + piece.w * 4]


def punch(canvas, polys):
    """Take these shapes out of what is drawn."""
    mask = Canvas(canvas.w // SCALE, canvas.h // SCALE)
    mask.paint(polys, WHITE)
    for i in range(canvas.w * canvas.h):
        cover = mask.data[i * 4 + 3]
        if cover:
            canvas.data[i * 4 + 3] = canvas.data[i * 4 + 3] * (255 - cover) // 255


def letter_rects(word, x, y):
    out = []
    for n, ch in enumerate(word):
        for row, line in enumerate(LETTERS[ch]):
            for col, mark in enumerate(line):
                if mark == '#':
                    out.append(rect(x + n * (LETTER_W + 1) + col, y + row, 1, 1))
    return out


def tapered(a, b, width_a, width_b):
    """A piece of line that changes thickness along its way, with round joints at both ends."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = math.hypot(dx, dy) or 1
    nx, ny = -dy / length, dx / length
    return [[(a[0] + nx * width_a / 2, a[1] + ny * width_a / 2), (b[0] + nx * width_b / 2, b[1] + ny * width_b / 2),
             (b[0] - nx * width_b / 2, b[1] - ny * width_b / 2), (a[0] - nx * width_a / 2, a[1] - ny * width_a / 2)],
            circle(a[0], a[1], width_a / 2, 16), circle(b[0], b[1], width_b / 2, 16)]


def write_text(canvas, drawer, text, box, colour, centred=False, edge=None):
    """Letters at their own widths, narrowed together only if they would not fit. Says how far."""
    x, y, w, h = box
    pen, marks = 0, []
    for ch in text:
        glyph = drawer.glyph(ch)
        if glyph:
            marks.append((pen, glyph))
        pen += GGO_METRICS_ADVANCE(drawer, ch)
    margin = 2
    room = w * SCALE - 2 * margin
    squeeze = min(1.0, room / pen) if pen else 1.0
    left = x * SCALE + margin + ((room - int(pen * squeeze)) // 2 if centred else 0)
    top = y * SCALE + (h * SCALE - drawer.height) // 2
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
    x0, y0, x1, y1 = x * SCALE, y * SCALE, (x + w) * SCALE, (y + h) * SCALE
    if edge:
        reach = 3
        for px, py in [k for k, v in coverage.items() if v > 96]:
            for dy in range(-reach, reach + 1):
                for dx in range(-reach, reach + 1):
                    qx, qy = px + dx, py + dy
                    if x0 <= qx < x1 and y0 <= qy < y1:
                        at = (qy * canvas.w + qx) * 4
                        canvas.data[at:at + 4] = bytes(edge) + b'\xff'
    for (px, py), v in coverage.items():
        if x0 <= px < x1 and y0 <= py < y1:
            at = (py * canvas.w + px) * 4
            under = canvas.data[at + 3]
            k = under * (255 - v) // 255
            out = v + k
            if not out:
                continue
            for i in range(3):
                canvas.data[at + i] = (colour[i] * v + canvas.data[at + i] * k) // out
            canvas.data[at + 3] = out
    return squeeze


# ---- the buttons' signs, as hints have them: in teal, ten pixels high, from the top
def sign_circle(c, x):
    c.paint(ring(x + 5, 5, 5, 3.1), TEAL)


def sign_cross(c, x):
    c.paint([segment(x + 1, 0.8, x + 9, 9.2, 2.7), segment(x + 9, 0.8, x + 1, 9.2, 2.7)], TEAL)


def sign_square(c, x):
    c.paint([rect(x, 0, 10, 2), rect(x, 8, 10, 2), rect(x, 0, 2, 10), rect(x + 8, 0, 2, 10)], TEAL)


def sign_triangle(c, x):
    outer = [(x + 5, -0.2), (x + 10, 10), (x, 10)]
    inner = [(x + 5, 3.6), (x + 2.6, 8), (x + 7.4, 8)]
    c.paint([outer + outer[:1] + inner[:1] + inner[:0:-1] + inner[:1] + outer[:1]], TEAL)


def sign_dpad(c, x):
    c.paint([circle(x + 5, 1.8, 1.8), circle(x + 5, 8.2, 1.8), circle(x + 1.8, 5, 1.8), circle(x + 8.2, 5, 1.8)], TEAL)


def sign_left_right(c, x):
    c.paint([[(x, 5), (x + 4, 1.7), (x + 4, 8.3)], [(x + 10, 5), (x + 6, 1.7), (x + 6, 8.3)]], TEAL)


def sign_up_down(c, x):
    c.paint([[(x + 3, 0), (x + 6, 3.1), (x + 6, 4), (x, 4), (x, 3.1)],
             [(x + 3, 10), (x + 6, 6.9), (x + 6, 6), (x, 6), (x, 6.9)]], TEAL)


def sign_stick(c, x):
    c.paint([circle(x + 5, 5, 5)], TEAL)
    holes = ((3, 1), (5, 1), (1, 3), (3, 3), (5, 3), (7, 3), (1, 5), (3, 5), (5, 5), (7, 5), (3, 7), (5, 7), (7, 7))
    punch(c, [circle(x + hx + 0.5, hy + 0.5, 0.5, 16) for hx, hy in holes])


def sign_l(c, x):
    c.paint([[(x + 5.5, 0), (x + 12, 0), (x + 12, 10), (x, 10), (x, 7.5)]], TEAL)
    punch(c, letter_rects('L', x + 6.5, 2))


def sign_r(c, x):
    c.paint([[(x, 0), (x + 6.5, 0), (x + 12, 7.5), (x + 12, 10), (x, 10)]], TEAL)
    punch(c, letter_rects('R', x + 2.5, 2))


def tag(word, width):
    """A word on a plate with half-oval ends."""
    def sign(c, x):
        end = 3.5
        arc = [(x + end - end * math.cos(i * math.pi / 16), 5 - 5 * math.sin(i * math.pi / 16)) for i in range(9)]
        arc += [(x + width - end + end * math.sin(i * math.pi / 16), 5 - 5 * math.cos(i * math.pi / 16)) for i in range(9)]
        arc += [(2 * x + width - px, 10 - py) for px, py in arc]
        c.paint([arc], TEAL)
        letters = len(word) * (LETTER_W + 1) - 1
        punch(c, letter_rects(word, x + (width - letters) // 2, 2))
    return sign


sign_select, sign_start = tag('SELECT', 28), tag('START', 24)


def sign_plus(c, x):
    c.paint([rect(x + 2, 3, 1, 5), rect(x, 5, 5, 1)], TEAL)


def sign_slash(c, x):
    c.paint([segment(x + 4, 1, x + 0.5, 9, 1.0)], GREY)


def signs_of(name, cross_confirms):
    """What a hint starts with, as (x, sign) pairs, and where its words start."""
    # Named for what they do, not for the sign they have, so those go by what the picture shows.
    left_right = ('basic_cross_4idou', 'basic_cross_rhythm', 'basic_cross_preset', 'basic_randl_change',
                  'basic_crolr_bpm', 'basic_crolr_camerasp', 'basic_crolr_bank')
    up_down = ('basic_cross_koumoku', 'basic_cross_zoom')
    confirm, cancel = (sign_cross, sign_circle) if cross_confirms else (sign_circle, sign_cross)
    if name in left_right:
        return ((0, sign_left_right),), 15
    if name in up_down:
        return ((0, sign_up_down),), 11
    if name == 'basic_4btn_target':
        # These are the notes' own buttons, which never change.
        return ((0, sign_circle), (12, sign_cross), (24, sign_triangle), (36, sign_square)), 51
    if name == 'basic_lcro_centeridou':
        return ((1, sign_l), (14, sign_plus), (21, sign_up_down)), 32
    if name == 'basic_cursor_kidou':
        return ((0, sign_stick), (12, sign_slash), (19, sign_left_right)), 34
    if name == 'basic_cursor_idou':
        return ((0, sign_stick),), 15
    if name.startswith('basic_lr_'):
        return ((0, sign_l), (14, sign_r)), 31
    if name.startswith('basic_l_'):
        return ((0, sign_l),), 18
    if name.startswith('basic_r_') and name != 'basic_randl_change':
        return ((0, sign_r),), 18
    if name.startswith('basic_sele_'):
        return ((0, sign_select),), 34
    if name.startswith('basic_start_'):
        return ((0, sign_start),), 30
    if name.startswith('basic_cross_'):
        return ((0, sign_dpad),), 15
    if name.startswith('basic_sp_'):
        return ((0, confirm),), 15
    if name.startswith('basic_batsu_'):
        return ((0, cancel),), 14
    if name.startswith('basic_shikaku_'):
        return ((0, sign_square),), 15
    if name.startswith('basic_tri_'):
        return ((0, sign_triangle),), 15
    return None, 0


def hint(sheet, name, box, start, text, drawer, cross_confirms):
    x, y, w, h = box
    signs, usual_start = signs_of(name, cross_confirms)
    if not signs:
        return 1.0
    start = start if start is not None else usual_start
    piece = Canvas(w, h)
    for sign_x, sign in signs:
        sign(piece, sign_x)
    piece.paint([rect(start - 3, 3, 2, 2), rect(start - 3, 6, 2, 2)], TEAL)
    squeeze = write_text(piece, drawer, text, (start, 0, w - start, h), GREY)
    paste(sheet, piece, x, y)
    return squeeze


# ---- the bands along the bottom, and the line art on them
def band(c, y, top_line_x, inner_x, body_top, has_upper_line):
    """Slanted on the left: the edge of the band starts at body_top and goes down to the left."""
    bottom = y + (25 if has_upper_line else 21)
    width = 512
    left_top = bottom - body_top - 1 + 4 if has_upper_line else bottom - body_top - 1 + 3
    left_bottom = left_top - (bottom - body_top)
    c.paint([[(left_top, body_top), (width, body_top), (width, bottom), (left_bottom, bottom)]], BAND)
    c.paint([[(left_top - 3, body_top), (left_top, body_top), (left_bottom, bottom), (left_bottom - 3, bottom)]], BAND_EDGE)
    c.paint([[(left_top - 4, body_top), (left_top - 3, body_top), (left_bottom - 3, bottom), (left_bottom - 4, bottom)]], SHADOW, 45)
    if has_upper_line:
        c.paint([rect(top_line_x, y, width - top_line_x, 1)], BAND)
        c.paint([[(top_line_x, y), (top_line_x + 2, y), (top_line_x - 5, y + 7), (top_line_x - 7, y + 7)]], BAND)
        for row, alpha in ((1, 37), (2, 66), (3, 37)):
            c.paint([rect(top_line_x + 2, y + row, width - top_line_x - 2, 1)], SHADOW, alpha)
    line_y = y + 4 if has_upper_line else y
    c.paint([rect(inner_x, line_y, width - inner_x, 1)], BAND)
    c.paint([[(inner_x, line_y), (inner_x + 2, line_y), (inner_x - 1, line_y + 3), (inner_x - 3, line_y + 3)]], BAND)
    for row, alpha in ((1, 37), (2, 66)):
        c.paint([rect(inner_x + 2, line_y + row, width - inner_x - 2, 1)], SHADOW, alpha)


# The long line, as (x, y, thickness) at each turn. It changes thickness on the way into a bend.
LONG_LINE = ((47, 4.5, 1.0), (86.5, 4.5, 1.0), (89.5, 7.5, 1.0), (92, 6, 1.4), (96, 6, 2.0), (128, 6, 2.0), (132, 5.5, 1.2),
             (137.5, 0.5, 1.0), (144.5, 7.5, 1.0), (148, 4.5, 1.4), (152, 4, 2.0), (216, 4, 2.0), (220, 4.5, 1.2),
             (223.5, 7.5, 1.0), (226, 5.5, 1.0), (294.5, 5.5, 1.0), (298.5, 1.5, 1.0), (301, 3.5, 1.4), (305, 4, 2.0), (480, 4, 2.0))


def line_shapes(x, y):
    shapes = [rect(x, y + 11, 480, 2)]
    shapes.append([(x + 2, y + 4.5), (x + 5, y + 2.2), (x + 5, y + 6.8)])
    for chevron in range(7, 37, 5):
        shapes += polyline([(x + chevron + 2.5, y + 2.5), (x + chevron + 0.5, y + 4.5), (x + chevron + 2.5, y + 6.5)], 1.0)
    shapes.append([(x + 37, y + 4.5), (x + 40, y + 2.2), (x + 40, y + 6.8)])
    # The arrow the long line ends in, larger than those before it.
    shapes.append([(x + 42, y + 4.5), (x + 47.5, y + 0.7), (x + 47.5, y + 8.3)])
    for a, b in zip(LONG_LINE, LONG_LINE[1:]):
        shapes += tapered((x + a[0], y + a[1]), (x + b[0], y + b[1]), a[2], b[2])
    return shapes


def line_art(c, x, y):
    """The small marks and the long line that runs along the bottom, with their shadow."""
    for dash in (0, 4, 8, 12, 16):
        c.paint([rect(x + dash + 0.6, y + 1, 2, 1)], SHADOW, 90)
        c.paint([rect(x + dash, y, 2, 1)], WHITE)
    c.paint([rect(x + 21.6, y + 1, 458, 1)], SHADOW, 90)
    c.paint([rect(x + 21, y, 459, 1)], WHITE)
    y += 5
    shapes = line_shapes(x, y)
    c.paint([[(px + 0.7, py + 1.1) for px, py in shape] for shape in shapes], SHADOW, 110)
    c.paint(shapes, WHITE)


def write_sheet(c, pixels, folder):
    c.write(os.path.join(folder, '%08X.rgba' % colour_hash(pixels)))


def sheet_of_hints(archive, labels, drawer, out, cross_confirms):
    picture = 'menu_basic02.png'
    w, h, pixels = decode_png(archive.read(archive.find(picture)))
    rects = layout_of(archive, picture)
    c = enlarged(archive, picture, pixels, w, h)
    tight = []
    for name, box in rects.items():
        if name in labels:
            start, text = labels[name]
            wipe(c, box)
            squeeze = hint(c, name.split('.')[0], box, start, text, drawer, cross_confirms)
            if squeeze < 0.8:
                tight.append('%s %d%%' % (text, squeeze * 100))
    btm, edit_btm = rects['basic_btm.png'], rects['edit_btm.png']
    wipe(c, btm)
    band(c, btm[1], 31, 46, btm[1] + 7, True)
    wipe(c, edit_btm)
    band(c, edit_btm[1], 45, 45, edit_btm[1] + 3, False)
    wipe(c, (32, 174, 480, 22))
    line_art(c, 32, 174)
    write_sheet(c, pixels, out)
    if tight and not cross_confirms:
        print('narrowed: ' + ', '.join(tight))


# ---- windows and the words on them
def window_marks(c, turned):
    """A quarter of what is in a window: the other is the same, turned around."""
    def at(px, py):
        return (WINDOW_SIZE - px, WINDOW_SIZE - py) if turned else (px, py)

    def line(points, width=1.0):
        c.paint([segment(*at(*a), *at(*b), width) for a, b in zip(points, points[1:])], ORNAMENT, ORNAMENT_ALPHA)

    def solid(points):
        c.paint([[at(*p) for p in points]], ORNAMENT, ORNAMENT_ALPHA)

    def box(x0, y0, x1, y1):
        line(((x0 - 0.5, y0), (x1 + 0.5, y0)))
        line(((x0 - 0.5, y1), (x1 + 0.5, y1)))
        line(((x0, y0), (x0, y1)))
        line(((x1, y0), (x1, y1)))

    box(3.5, 9.5, 6.5, 12.5)
    box(54.5, 9.5, 56.5, 11.5)
    for x0, y0 in ((9, 9), (12, 9), (3, 15), (3, 18)):
        solid(((x0, y0), (x0 + 2, y0), (x0 + 2, y0 + 2), (x0, y0 + 2)))
    # The line along the top, starting from a point, and the one down the side.
    line(((16, 9.5), (52, 9.5)))
    solid(((16, 9), (20, 9), (18.2, 12)))
    solid(((3, 22), (5, 22), (5, 46), (3, 46)))
    # The little arrow in the corner, with straight sides.
    line(((10.5, 14.5), (7.5, 14.5), (7.5, 18.5), (14.5, 18.5)))
    box(10.5, 13.5, 13.5, 15.5)
    solid(((11, 19), (14.5, 19), (11.5, 22.2)))


def window(sheet):
    c = Canvas(WINDOW_SIZE, WINDOW_SIZE)
    for row in range(8, WINDOW_SIZE - 8):
        c.fill_rect(1, row, WINDOW_SIZE - 2, 1, WINDOW_FILL)
    for row, colour in enumerate(WINDOW_TOP):
        c.fill_rect(0, row, WINDOW_SIZE, 1, colour)
    for row, colour in enumerate(WINDOW_BOTTOM):
        c.fill_rect(0, WINDOW_SIZE - 8 + row, WINDOW_SIZE, 1, colour)
    c.fill_rect(0, 6, 1, WINDOW_SIZE - 12, WINDOW_LEFT)
    c.fill_rect(WINDOW_SIZE - 1, 6, 1, WINDOW_SIZE - 12, WINDOW_RIGHT)
    window_marks(c, False)
    window_marks(c, True)
    s, n = WINDOW_SIZE, WINDOW_CUT
    punch(c, [[(0, 0), (n, 0), (0, n)], [(s, 0), (s - n, 0), (s, n)], [(0, s), (n, s), (0, s - n)], [(s, s), (s - n, s), (s, s - n)]])
    paste(sheet, c, WINDOW_X, WINDOW_Y)


def centred_on_button(drawer, text, button_width):
    """How far along a button its sign and word start, so that together they are in its middle."""
    word = sum(GGO_METRICS_ADVANCE(drawer, ch) for ch in text) / SCALE
    return max(0, int(button_width - BUTTON_WORD_AT - word) // 2)


def button_sign(c, x, y, circle_sign):
    """A sign with a white edge all the way round, twelve pixels across, and the colon after it."""
    if circle_sign:
        c.paint(ring(x + 6, y + 6.5, 6, 2), WHITE)
        c.paint(ring(x + 6, y + 6.5, 5, 3), BUTTON_TEAL)
    else:
        arms = ((x + 1.6, y + 2.1, x + 10.4, y + 10.9), (x + 10.4, y + 2.1, x + 1.6, y + 10.9))
        # Longer than the teal, so that the white goes round the ends as well.
        white = ((x + 0.9, y + 1.4, x + 11.1, y + 11.6), (x + 11.1, y + 1.4, x + 0.9, y + 11.6))
        c.paint([segment(*arm, 4.4) for arm in white], WHITE)
        c.paint([segment(*arm, 2.4) for arm in arms], BUTTON_TEAL)
    c.paint([rect(x + 15, y + 4, 1.4, 2), rect(x + 15, y + 8, 1.4, 2)], WHITE)


def sheet_of_windows(archive, labels, drawer, out, cross_confirms):
    picture = 'menu_basic.png'
    w, h, pixels = decode_png(archive.read(archive.find(picture)))
    rects = layout_of(archive, picture)
    c = enlarged(archive, picture, pixels, w, h)
    wipe(c, (WINDOW_X, WINDOW_Y, WINDOW_SIZE, WINDOW_SIZE))
    window(c)
    # Yes and No are placed to keep their row in the middle, and the others on their own buttons.
    # Back comes first: the edge of its sign reaches into OK, whose wipe then clears it.
    for name, confirm, moved in (('basic_btninfo_modoru.png', False, None), ('basic_btninfo_kettei.png', True, None),
                                 ('basic_btninfo_yes.png', True, 7), ('basic_btninfo_no.png', False, 24),
                                 ('basic_btninfo_saisikou.png', True, None)):
        if name not in labels:
            continue
        x, y, bw, bh = rects[name]
        if moved is None:
            moved = centred_on_button(drawer, labels[name][1], bw)
        wipe(c, rects[name])
        button_sign(c, x + moved, y, confirm != cross_confirms)
        write_text(c, drawer, labels[name][1], (x + moved + BUTTON_WORD_AT, y, bw - moved - BUTTON_WORD_AT, bh), WORD, False, WORD_EDGE)
    for name, word in (('basic_flwin_resume.png', 'RESUME'), ('basic_flwin_retry.png', 'RETRY'), ('basic_flwin_return.png', 'RETURN')):
        wipe(c, rects[name])
        write_text(c, drawer, word, rects[name], WHITE, True, WORD_EDGE)
    write_sheet(c, pixels, out)


# ---- the rest
def enlarged(archive, picture, pixels, w, h):
    """What is not traced yet: the game's own, following its edges, with its plates cleaned."""
    rows = smooth_enlarge(pixels, plate_rects(archive, picture), SCALE)
    c = Canvas(w, h)
    for y, row in enumerate(rows):
        c.data[y * c.w * 4:(y + 1) * c.w * 4] = b''.join(bytes(p) for p in row)
    return c


def wipe(c, box):
    x, y, w, h = box
    c.fill_rect(x, y, w, h, (0, 0, 0, 0))


def sheet_clean(archive, picture, out):
    entry = archive.find(picture)
    w, h, pixels = decode_png(archive.read(entry))
    write_sheet(enlarged(archive, entry, pixels, w, h), pixels, out)


def result_hints(archive, labels, drawer, out, made_in, cross_confirms):
    """The two hints the results screen has in its own picture, in line with those elsewhere."""
    picture = 'menu_result.png'
    w, h, pixels = decode_png(archive.read(picture))
    name = '%08X.rgba' % colour_hash(pixels)
    path = os.path.join(made_in, name)
    if not os.path.exists(path):
        return
    with open(path, 'rb') as f:
        fw, fh = struct.unpack('<II', f.read(8))
        data = f.read()
    if fw != w * SCALE:
        return
    c = Canvas(w, h)
    for y in range(c.h):
        c.data[y * c.w * 4:(y + 1) * c.w * 4] = data[(c.h - 1 - y) * c.w * 4:(c.h - y) * c.w * 4]
    item = labels.get('@375,54,53,13', (0, 'Item'))[1]
    pick = labels.get('@444,54,32,13', (0, 'Pick'))[1]
    y0, x0 = 54, 354
    plate = tuple(pixels[y0 + 1][x0 + 1])
    c.fill_rect(x0, y0, 480 - x0, 13, plate)
    piece = Canvas(480 - x0, 13)
    piece.paint([[(0, 6.5), (4, 3.2), (4, 9.8)], [(10, 6.5), (6, 3.2), (6, 9.8)]], WHITE)
    piece.paint([rect(12, 4.5, 2, 2), rect(12, 7.5, 2, 2)], WHITE)
    write_text(piece, drawer, item, (15, 0, 50, 13), WHITE)
    at = 68
    if cross_confirms:
        piece.paint([segment(at + 1, 2.3, at + 9, 10.7, 2.7), segment(at + 9, 2.3, at + 1, 10.7, 2.7)], WHITE)
    else:
        piece.paint(ring(at + 5, 6.5, 5, 3.1), WHITE)
    piece.paint([rect(at + 12, 4.5, 2, 2), rect(at + 12, 7.5, 2, 2)], WHITE)
    write_text(piece, drawer, pick, (at + 15, 0, 480 - x0 - at - 15, 13), WHITE)
    for row in range(piece.h):
        for col in range(piece.w):
            a = piece.data[(row * piece.w + col) * 4 + 3]
            if a:
                at_px = ((y0 * SCALE + row) * c.w + x0 * SCALE + col) * 4
                for i in range(3):
                    c.data[at_px + i] = (piece.data[(row * piece.w + col) * 4 + i] * a + c.data[at_px + i] * (255 - a)) // 255
                c.data[at_px + 3] = max(c.data[at_px + 3], a)
    c.write(os.path.join(out, name))


def write_card_swaps(archive, out):
    """Which pictures are cards before a song, whose two buttons change places when Cross confirms."""
    with open(os.path.join(out, 'swap_regions.txt'), 'w', encoding='ascii') as f:
        for entry in sorted({e[0] for e in archive.entries if e[0].startswith('pv_start_') and '.p' in e[0]}):
            w, _, pixels = decode_png(archive.read(entry))
            rects = layout_of(archive, entry)
            o, x = rects['button_o.png'], rects['button_x.png']
            f.write('%08X %d %d %d %d %d %d %d\n' % (colour_hash(pixels), o[0], o[1], x[0], x[1], o[2], o[3], w))


def read_labels(path):
    """The words for each picture's pieces, as lang_images.py reads them."""
    labels = {}
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            if not line.strip() or line.startswith('#'):
                continue
            picture, name, start, style, text = [part.strip() for part in line.split('|')]
            if style == 'hint' or style.startswith('button') or picture == 'menu_result.png':
                labels.setdefault(picture, {})[name] = (int(start), text)
    return labels


def main():
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    disc, font, table, out = sys.argv[1:]
    archive = Archive(os.path.join(disc, ARCHIVE))
    labels = read_labels(table)
    family = load_font(font)
    probe = Drawer(family, 44)
    words = {ch for pieces in labels.values() for _, text in pieces.values() for ch in text}
    if any(ch != ' ' and not probe.draws(ch) for ch in words):
        family = load_font(FREE_FONT)
    os.makedirs(out, exist_ok=True)
    os.makedirs(out + '_x', exist_ok=True)
    drawer = Drawer(family, HINT_H * SCALE)
    for picture in CLEAN_ONLY:
        sheet_clean(archive, picture, out)
    result_labels = labels.get('menu_result.png', {})
    for folder, cross in ((out, False), (out + '_x', True)):
        sheet_of_hints(archive, labels.get('menu_basic02.png', {}), drawer, folder, cross)
        sheet_of_windows(archive, labels.get('menu_basic.png', {}), drawer, folder, cross)
        result_hints(archive, result_labels, drawer, folder, out, cross)
    write_card_swaps(archive, out)
    print('%s: done' % out)


if __name__ == '__main__':
    main()
