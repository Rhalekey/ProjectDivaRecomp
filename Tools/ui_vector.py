"""Draw the menus again, clean, at four times the size of the game's own.

    python Tools/ui_vector.py <folder holding PSP_GAME> <font file> <lang/xx_images.txt> <output folder>

The game's art is the mould. The pieces seen most are traced over it by hand, in straight lines,
with what pixels could not show put in. What is not traced yet is enlarged by following its edges,
with the dotted texture of its plates taken out (see smooth_enlarge in lang_images.py).

Each piece stays where the game's layout files say it goes, so the game goes on placing and
animating it as ever, and the renderer shows these in place of its pictures.

What has the buttons' signs is made six times: the output folder has the pictures for Circle
confirming, and the same folder with _x after its name has those that differ when Cross does;
with _xa, _xb, _na and _nb, those for the styles with letters, Xbox or Nintendo with A or B
confirming, with the card's buttons.
"""
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hd_font import Drawer, FREE_FONT, GGO_METRICS_ADVANCE, load_font  # noqa: E402
from hud_vector import SCALE, Canvas, circle, ellipse, polyline, rect, ring, segment, text_strokes, GLYPHS, letter_strokes  # noqa: E402
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
BLADE_X, BLADE_Y, BLADE_W, BLADE_H = 0, 0, 164, 17
BLADE_SEAM_X = 105     # where the plate's second part starts, at the top
BLADE_SEAM_LEAN = 0.8
BLADE_CLEAR = 2.5      # how far from an edge its colours are taken, to miss the old steps
TITLE_PLATE_H = 21     # the plate with a menu's name, which ends at the picture's right edge
TITLE_RIGHT = 512
TITLE_SHADOW_ROWS = 2  # of shadow under a plate with no lines under it
TITLE_TAIL = 10
TITLE_EDGE = 3
TITLE_EDGE_STRIP = 3   # how far left of the plate's cut edge it is redrawn, past any words
TITLE_BAND_REDRAWN = 8  # under a plate with nothing under it, its band is redrawn from this far left of the edge
TITLE_BAND_ROWS = 2     # the dark band under a plate
TITLE_BAND_ROWS_LINES = 4   # and under one with lines: two dark rows, the dotted line's, and a shadow
TITLE_TAIL_SHADE = 9.5  # the white diagonal's shadow, this far right of the lines' end, a row under the plate
TITLE_TAIL_SHADE_ROWS = 9
TITLE_EDGE_TOP = 4     # where that edge starts, under the white band
TITLE_GREY = (133, 136, 147)
TITLE_LINES_END = 480  # where the lines under that plate end, and go up to it
TITLE_LINE = (247, 241, 251)
ARROW_GLOW_REACH = 2.6  # how far an arrow's glow spreads, in the game's pixels
ARROW_RIM = 0.6        # how thick its lighter rim is
# The arrows inside a row's bar, as the columns they fill: either side of a value.
BAR_ARROWS = ((113, 117, 'left'), (167, 171, 'right'))
BAR_ARROW_TOP, BAR_ARROW_H = 12, 7   # from the glow's top; in the plain bar they start 5 from its top
BAR_ARROW_MARGIN = 2   # around one, where the old halo is cleared
BAR_ARROW_HALO = 1.2   # and how far the new one spreads
SYMBOL_GLOW_REACH = 5.5   # the rhythm screen's shapes glow this far
SYMBOL_GLOW_ALPHA = 200   # and this strongly at their edge
STREAK_REACH = 2       # the lists' bars and glows are smoothed along their rows three times this far
ICON_PINK_TOP, ICON_PINK_LOW = (236, 30, 155), (160, 0, 96)
ICON_BLUE_TOP, ICON_BLUE_LOW = (60, 150, 212), (1, 98, 166)
ICON_FOLDER_LIGHT = (200, 226, 245)
ICON_RING_DARK = (65, 75, 90)
ICON_RING_A, ICON_RING_B = (1, 214, 166), (40, 250, 240)
PLATE_ICON_X, PLATE_ICON_Y = 9, 3     # where a title plate's icon is, from the plate's corner
PLATE_ICON_W, PLATE_ICON_H = 16, 15
DIGIT_SHADOW_ALPHA = 225
MARKER_EDGE = (0x6b, 0xfe, 0xff)      # the scroll marker's edge and middle
MARKER_FILL = (0x55, 0xec, 0xf1)
SLIDER_ROUND = 1.6
RESULT_HEADER = (0, 0, 480, 69)       # the top of the results picture, with its line art
RESULT_ICON = (9, 2, 16, 15)
RESULT_WORDS = (36, 1, 110, 19)       # where its title's words are, kept as written
RESULT_STAR_MIDDLE, RESULT_STAR_R = 7.0, 6.4
# The song card's star in its 10 by 10: middle, the blue's reach, the white beyond it, how far in its
# waist comes (the picture's star is a plump one).
CARD_STAR_MIDDLE, CARD_STAR_R, CARD_STAR_EDGE, CARD_STAR_WAIST = 5.4, 3.7, 0.85, 0.5
CARD_ICON_EDGE = 0.9   # the white round the card's other icons
RESULT_WORD_EDGE = 0.28          # the dark edge round the window's words: how dark, how wide, how solid
RESULT_WORD_EDGE_W = 0.45
RESULT_WORD_STROKE = 2.3       # the letters' strokes, their dark edge inside
RESULT_WORD_EDGE_ALPHA = 230
RESULT_WORD_LEAST = 40           # letter pixels a word needs to be there at all
LINE_WIDTH = 1.0                 # one pixel lines drawn through their middles
TEXTURE_FROM = (120, 108, 132, 96)  # how far right, on the same row, the window's bare ground is copied from
BUTTON_WORD_AT = 18    # where a button's word starts, after its sign and colon
WINDOW_ROUND = 3.5       # the message window's corners, round

def ui_words(language):
    """{key: text} of lang/<language>_ui.txt, the project's words in that language."""
    path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'lang', '%s_ui.txt' % language)
    words = {}
    if not os.path.exists(path):
        return words
    with open(path, encoding='utf-8-sig') as f:
        for line in f:
            line = line.rstrip('\r\n')
            if not line or line.startswith('#') or '=' not in line:
                continue
            key, text = line.split('=', 1)
            words[key.strip()] = text[1:] if text.startswith(' ') else text
    return words


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
CLEAN_ONLY = ('menu_common2.png',)


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


def write_text(canvas, drawer, text, box, colour, centred=False, edge=None, below=0):
    """Letters at their own widths, narrowed together only if they would not fit. Says how far.
    `below` lets what hangs under the letters run that far past the box."""
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
    x0, y0, x1, y1 = x * SCALE, y * SCALE, (x + w) * SCALE, (y + h + below) * SCALE
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


def letter_sign(letter):
    """A teal disc with the letter taken out of it, as the stick's sign is."""
    def sign(c, x):
        c.paint([circle(x + 5, 5, 5)], TEAL)
        punch(c, letter_strokes(letter, x + 5, 5, 6, 1.35))
    return sign


# For each set of pictures, the signs of: what confirms, what goes back, Square and Triangle. Xbox
# has A below, B right, X left and Y above; Nintendo B below, A right, Y left and X above.
SIGN_SETS = {
    'o': ('circle', 'cross', 'square', 'triangle'), 'x': ('cross', 'circle', 'square', 'triangle'),
    'xa': ('A', 'B', 'X', 'Y'), 'xb': ('B', 'A', 'X', 'Y'),
    'na': ('A', 'B', 'Y', 'X'), 'nb': ('B', 'A', 'Y', 'X'),
}
# The folder for each, after the language's own name.
SET_FOLDERS = (('', 'o'), ('_x', 'x'), ('_xa', 'xa'), ('_xb', 'xb'), ('_na', 'na'), ('_nb', 'nb'))


def hint_sign(kind):
    """A hint's sign by its kind: a letter, or one of the PSP's own signs."""
    return letter_sign(kind) if len(kind) == 1 else globals()['sign_' + kind]


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


def signs_of(name, sign_set):
    """What a hint starts with, as (x, sign) pairs, and where its words start."""
    # Named for what they do, not for the sign they have, so those go by what the picture shows.
    left_right = ('basic_cross_4idou', 'basic_cross_rhythm', 'basic_cross_preset', 'basic_randl_change',
                  'basic_crolr_bpm', 'basic_crolr_camerasp', 'basic_crolr_bank')
    up_down = ('basic_cross_koumoku', 'basic_cross_zoom')
    confirm, cancel, square, triangle = (hint_sign(kind) for kind in SIGN_SETS[sign_set])
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
        return ((0, square),), 15
    if name.startswith('basic_tri_'):
        return ((0, triangle),), 15
    return None, 0


def hint(sheet, name, box, start, text, drawer, sign_set, below=0):
    x, y, w, h = box
    signs, usual_start = signs_of(name, sign_set)
    if not signs:
        return 1.0
    start = start if start is not None else usual_start
    piece = Canvas(w, h + below)
    for sign_x, sign in signs:
        sign(piece, sign_x)
    piece.paint([rect(start - 3, 3, 2, 2), rect(start - 3, 6, 2, 2)], TEAL)
    squeeze = write_text(piece, drawer, text, (start, 0, w - start, h), GREY, below=below)
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


# The long line: thin runs that bend, and thick flat runs between them, as the titles' top wave is.
# Each thin run starts or ends on the middle of a thick run's end; ('thick', from x, to x, y).
LONG_LINE = (
    ('thin', ((47, 4.5), (86.5, 4.5), (89.5, 7.5), (91, 6))),
    ('thick', 91, 132, 6),
    ('thin', ((132, 6), (137.5, 0.5), (144.5, 7.5), (148, 4))),
    ('thick', 148, 220, 4),
    ('thin', ((220, 4), (223.5, 7.5), (226, 5.5), (294.5, 5.5), (298.5, 1.5), (301, 4))),
    ('thick', 301, 480, 4),
)
LONG_LINE_THIN, LONG_LINE_THICK = 1.0, 2.0


def thin_into_thick(points, width, start_y, end_y, half):
    """The two sides of a thin stroke going left to right, its top side first. Where an end meets a
    thick flat line centred at start_y or end_y, each side runs on to that line's top or bottom, so
    the two meet corner to corner with no step, as the titles' top wave does."""
    poly = thick(points, width)
    n = len(points)
    bottom, top = poly[:n], poly[n:][::-1]
    for index, cy in ((0, start_y), (n - 1, end_y)):
        if cy is None:
            continue
        for side in (top, bottom):
            p = side[index]
            q = side[1] if index == 0 else side[n - 2]
            target = cy - half if p[1] < cy else cy + half
            t = (target - p[1]) / ((q[1] - p[1]) or 1e-9)
            side[index] = (p[0] + (q[0] - p[0]) * t, target)
    return top, bottom


def long_line(x, y):
    """The whole long line as one outline, its thin and thick runs joined, so no seam shows."""
    half = LONG_LINE_THICK / 2
    upper, lower = [], []
    for k, run in enumerate(LONG_LINE):
        if run[0] == 'thin':
            before = LONG_LINE[k - 1] if k > 0 else None
            after = LONG_LINE[k + 1] if k + 1 < len(LONG_LINE) else None
            top, bottom = thin_into_thick([(x + px, y + py) for px, py in run[1]], LONG_LINE_THIN,
                                          y + before[3] if before else None, y + after[3] if after else None, half)
            upper += top
            lower += bottom
        else:
            _, x0, x1, cy = run
            if k == 0:
                upper.append((x + x0, y + cy - half))
                lower.append((x + x0, y + cy + half))
            if k == len(LONG_LINE) - 1:
                upper.append((x + x1, y + cy - half))
                lower.append((x + x1, y + cy + half))
    return upper + lower[::-1]


def line_shapes(x, y):
    shapes = [rect(x, y + 11, 480, 2)]
    shapes.append([(x + 2, y + 4.5), (x + 5, y + 2.2), (x + 5, y + 6.8)])
    for chevron in range(7, 37, 5):
        shapes += polyline([(x + chevron + 2.5, y + 2.5), (x + chevron + 0.5, y + 4.5), (x + chevron + 2.5, y + 6.5)], 1.0)
    shapes.append([(x + 37, y + 4.5), (x + 40, y + 2.2), (x + 40, y + 6.8)])
    # The arrow the long line ends in, larger than those before it.
    shapes.append([(x + 42, y + 4.5), (x + 47.5, y + 0.7), (x + 47.5, y + 8.3)])
    shapes.append(long_line(x, y))
    return shapes


def line_art(c, x, y):
    """The small marks and the long line that runs along the bottom, over their shadow, cast down
    and to the right as the game's is. Drawn on a layer of their own, then laid over the band."""
    ox, oy = x - 1, y - 1
    layer = Canvas(LINE_ART_W, LINE_ART_H)
    shapes = [rect(x + dash, y, 2, 1) for dash in (0, 4, 8, 12, 16)] + [rect(x + 21, y, 459, 1)] + line_shapes(x, y + 5)
    layer.paint([[(px - ox, py - oy) for px, py in shape] for shape in shapes], WHITE)
    blend(c, drop_shadow(layer, LINE_SHADOW_ALPHA, LINE_SHADOW_COLOUR), ox, oy)
    blend(c, layer, ox, oy)


def sampled(pixels, x, y, box):
    """The game's picture between its pixels, at a point kept inside a box of it."""
    x0, y0, x1, y1 = box
    fx = min(max(x - 0.5, x0), x1 - 1.001)
    fy = min(max(y - 0.5, y0), y1 - 1.001)
    ix, iy = int(fx), int(fy)
    tx, ty = fx - ix, fy - iy
    out = []
    for i in range(3):
        top = pixels[iy][ix][i] * (1 - tx) + pixels[iy][ix + 1][i] * tx
        low = pixels[iy + 1][ix][i] * (1 - tx) + pixels[iy + 1][ix + 1][i] * tx
        out.append(int(top * (1 - ty) + low * ty))
    return tuple(out)


def blade(sheet, pixels):
    """The plate under each menu's title. Its metal is the game's own: only its second plate's edge and
    its cut end become straight, each side taking its colour from a little way off the old steps."""
    x, y, w, h = BLADE_X, BLADE_Y, BLADE_W, BLADE_H
    box = (x, y + 1, x + w, y + h - 1)
    c = Canvas(w, h)

    def seam(row):
        return BLADE_SEAM_X - BLADE_SEAM_LEAN * row

    def left_plate(px, py):
        return sampled(pixels, x + min(px, seam(py) - BLADE_CLEAR), y + py, box)

    def right_plate(px, py):
        return sampled(pixels, x + min(max(px, seam(py) + BLADE_CLEAR), w - py - BLADE_CLEAR), y + py, box)

    c.paint([[(0, 0), (seam(0), 0), (seam(h), h), (0, h)]], left_plate)
    c.paint([[(seam(0), 0), (w, 0), (w - h, h), (seam(h), h)]], right_plate)
    # What catches the light: the lines along its top and bottom, and the cut.
    c.paint([[(0, 0), (w, 0), (w - 1, 1), (0, 1)]], lambda px, py: sampled(pixels, x + min(px, w - 3), y + 0.5, (x, y, x + w, y + 1.002)))
    c.paint([[(0, h - 1), (w - h + 1, h - 1), (w - h, h), (0, h)]], lambda px, py: sampled(pixels, x + min(px, w - h - 2), y + h - 0.5, (x, y + h - 1, x + w, y + h + 0.002)))
    c.paint([[(w, 0), (w - h, h), (w - h - 1, h), (w - 1, 0)]], WHITE)
    paste(sheet, c, x, y)


def title_plate_edge(sheet, pixels, y, lines_below):
    """The cut end of the plate with a menu's name: a grey edge along it, a shadow beside that, the
    dark band under the plate running on to the same diagonal, and, where two lines run under the
    plate, the white diagonal that takes them up to meet that edge, with its shadow."""
    h, tail = TITLE_PLATE_H, TITLE_TAIL
    x0 = TITLE_RIGHT - h - tail - 8
    c = Canvas(TITLE_RIGHT - x0, h + tail)
    plate = tuple(pixels[y + 1][x0 - 20])
    under = tail if lines_below else TITLE_SHADOW_ROWS
    tail_x = TITLE_LINES_END - x0

    def edge(row):
        return TITLE_RIGHT - row - x0

    def tail_right(row):
        """The right side of the white diagonal, under the plate, and of the lines' end below it."""
        return tail_x + 6 - min(row - h, 6)

    # It starts from what is there: the plate's words, and under it the game's band and lines,
    # which run straight. Right of the cut edge, where its old steps and their shadow were, it is
    # cleared; under the plate, from where the band and lines end on, every diagonal is drawn anew.
    for row in range(c.h):
        src = ((y * SCALE + row) * sheet.w + x0 * SCALE) * 4
        c.data[row * c.w * 4:(row + 1) * c.w * 4] = sheet.data[src:src + c.w * 4]
    before = bytes(c.data)
    band_rows = TITLE_BAND_ROWS_LINES if lines_below else TITLE_BAND_ROWS
    # Under a plate with nothing under it, everything is redrawn from a column a little left of
    # where the band meets the edge, straight down, so that what is kept and what is drawn meet
    # on a pixel's side.
    band_left = edge(h + band_rows) - TITLE_BAND_REDRAWN
    for row in range(c.h):
        game_row = row / SCALE
        if game_row < h:
            clear_from = edge(game_row) - TITLE_EDGE_STRIP
        elif lines_below:
            clear_from = tail_right(game_row)
        else:
            clear_from = band_left
        start = max(0, int(clear_from * SCALE))
        c.data[(row * c.w + start) * 4:(row + 1) * c.w * 4] = bytes((c.w - start) * 4)

    def along(colour, top, bottom, left=None):
        """Rows from top to bottom of the band under the plate, ending at the cut edge's diagonal."""
        if left is None:
            shape = [(band_left, top), (edge(top) + TITLE_EDGE, top), (edge(bottom) + TITLE_EDGE, bottom), (band_left, bottom)]
        else:
            shape = [(left(top), top), (edge(top) + TITLE_EDGE, top), (edge(bottom) + TITLE_EDGE, bottom), (left(bottom), bottom)]
        c.paint([shape], colour)

    # The dark band runs on as it is a little to the left, every row of it, so that no seam shows.
    column = int((tail_x - 6 if lines_below else band_left - 1) * SCALE)

    def band(_px, py):
        at = (min(int(py * SCALE), c.h - 1) * c.w + column) * 4
        return tuple(before[at:at + 4])
    along(band, h, h + band_rows, tail_right if lines_below else None)
    # With no lines under it, a soft shadow; then the shadow beside the edge, the plate and its edge.
    if not lines_below:
        c.paint([[(0, h + band_rows), (edge(h + band_rows) + TITLE_EDGE, h + band_rows),
                  (edge(h + under) + TITLE_EDGE, h + under), (0, h + under)]], (42, 42, 42), 32)
    # Its first rows are the white band above, which runs on to the edge of the picture. The shadow
    # beside the edge runs its whole length, from where the edge starts on down past the band, as
    # the game's does.
    top = TITLE_EDGE_TOP
    end = h + band_rows
    c.paint([[(edge(top), top), (edge(top) + 5, top), (edge(h) + 5, h), (edge(h), h)]], (20, 20, 20), 80)
    c.paint([[(edge(h) + TITLE_EDGE, h), (edge(h) + 5, h), (edge(end) + 5, end), (edge(end) + TITLE_EDGE, end)]], (20, 20, 20), 80)
    c.paint([[(0, 0), (c.w / SCALE, 0), (c.w / SCALE, top), (edge(top), top), (edge(h), h), (0, h)]], plate)
    c.paint([[(edge(top), top), (edge(top) + TITLE_EDGE, top), (edge(h) + TITLE_EDGE, h), (edge(h), h)]], TITLE_GREY)
    if lines_below:
        # The white diagonal's shadow: a soft line beside it, from the band down past the lines.
        def shade(width, alpha):
            x_at = lambda row: tail_x + TITLE_TAIL_SHADE - (row - h)
            first, last = h + band_rows - 1, h + TITLE_TAIL_SHADE_ROWS
            c.paint([[(x_at(first) - width / 2, first), (x_at(first) + width / 2, first),
                      (x_at(last) + width / 2, last), (x_at(last) - width / 2, last)]], (20, 20, 20), alpha)
        shade(3.0, 40)
        shade(1.2, 70)
        c.paint([[(tail_x + 6, h), (tail_x + 3, h), (tail_x - 3, h + 6), (tail_x, h + 6)]], plate)
        c.paint([rect(tail_x - 3, h + 5, 3, 1)], plate)
        c.paint([rect(tail_x - 3, h + 2, 1, 1)], plate)    # the last dot of the line above it
    # On the plate's rows only a narrow strip by its edge is laid down, where the old steps were: a long
    # name runs nearly to the edge, and must not be painted over. Under the plate, from the tail on.
    keep = tail_x - 3 if lines_below else 0
    paste_over(sheet, c, x0, y, keep_left=keep, plate_rows=h,
               strip=lambda row: edge(row) - TITLE_EDGE_STRIP)


def paste_over(sheet, piece, x, y, keep_left, plate_rows=0, strip=None):
    """Over what is there, from a column on, as the left of a piece may hold words. On its first
    `plate_rows` rows it starts from `strip(row)` instead, if that is further right."""
    for row in range(piece.h):
        start = keep_left
        if strip and row < plate_rows * SCALE:
            start = max(keep_left, strip(row / SCALE))
        start_px = int(start * SCALE)
        src = (row * piece.w + start_px) * 4
        dst = ((y * SCALE + row) * sheet.w + x * SCALE + start_px) * 4
        width = (piece.w - start_px) * 4
        sheet.data[dst:dst + width] = piece.data[src:src + width]


class LineArt:
    """Thin white lines with the game's soft shadow under them, built from straight pieces, all the
    shadows first so that no shadow falls on a line. Points are given in the sheet's pixels."""

    def __init__(self, x, y, w, h):
        self.c = Canvas(w, h)
        self.ox, self.oy = x, y
        self.whites, self.shadows = [], []

    def at(self, *points):
        return [(px - self.ox, py - self.oy) for px, py in points]

    def shape(self, polygon):
        self.whites.append(polygon)
        self.shadows.append([(px + 1, py + 1) for px, py in polygon])

    def stroke(self, points, width):
        self.whites.extend(polyline(points, width))
        self.shadows.extend(polyline([(px + 1, py + 1) for px, py in points], width))

    def bar(self, x0, x1, y0, rows=1):
        self.shape(rect(x0 - self.ox, y0 - self.oy, x1 - x0 + 1, rows))

    def ledge(self, x0, x1, y0, under):
        """A row on the top or bottom of a thicker line, with slanted ends."""
        wide, short = (y0 + 1, y0) if under else (y0, y0 + 1)
        self.shape(self.at((x0, wide), (x1 + 1, wide), (x1, short), (x0 + 1, short)))

    def box(self, x0, y0, w, h):
        """A square outline of single pixels."""
        for x, y, bw, bh in ((x0, y0, w, 1), (x0, y0 + h - 1, w, 1), (x0, y0, 1, h), (x0 + w - 1, y0, 1, h)):
            self.shape(rect(x - self.ox, y - self.oy, bw, bh))

    def finish(self, sheet):
        """The lines over their shadow, cast down and to the right as the game's is."""
        layer = Canvas(self.c.w // SCALE, self.c.h // SCALE)
        layer.paint(self.whites, TITLE_LINE)
        self.c.data[:] = drop_shadow(layer, LINE_SHADOW_ALPHA, LINE_SHADOW_COLOUR).data
        blend(self.c, layer, 0, 0)
        paste(sheet, self.c, self.ox, self.oy)


def title_top_row(k, dx, left):
    """The first row of every title: the thick line, its rise and dip, its arrow, and the marks that run
    on from it. The titles with a small plate have it all 3 pixels to the left of the Home one.
    The wave rises out of the thick line's middle, its two edges meeting the thick line's two
    corners, so neither side has a step. Its piece starts at row 29, which is as high as anything
    can go, and the arrow's ends stay within it."""
    k.shape(k.at((left, 32), (261.3 + dx, 32), (260.94 + dx, 34), (left, 34)))
    k.stroke(k.at(*[(px + dx, py) for px, py in ((260.8, 33.3), (264.8, 29.8), (268, 32.5), (306, 32.5), (309.5, 35.5), (313, 32.5), (357.5, 32.5))]), 1.25)
    k.stroke(k.at((353.6 + dx, 29.7), (357.5 + dx, 32.5), (353.6 + dx, 35.3)), 1.4)
    for tip in (361, 396):
        k.shape(k.at((tip + dx, 29.5), (tip + dx + 3.5, 32.5), (tip + dx, 35.5)))
    for start in range(366, 392, 5):
        k.stroke(k.at((start + dx, 30), (start + dx + 2.6, 32.5), (start + dx, 35)), 0.9)
    for x0, x1 in ((405, 407), (409, 411), (413, 415), (417, 420), (422, 424), (426, 428), (430, 432), (434, 436), (438, 441), (443, 445),
                   (447, 449), (451, 454), (456, 458), (460, 463), (465, 468), (470, 471), (473, 475), (477, 479), (481, 483), (485, 487),
                   (489, 491), (493, 495), (497, 499), (501, 504), (506, 508), (510, 511)):
        k.bar(x0 + dx, x1 + dx, 32)


def title_home_lines(sheet):
    """The lines of the Home title, and Miku's Room's, which are the same: every one straight, with the
    same ends and bends as the game's."""
    k = LineArt(32, 29, 480, 47)
    title_top_row(k, 0, 32)
    # Second line, with its gaps, and the dashes that follow.
    for x0, x1 in ((32, 239), (247, 285), (295, 329), (340, 377)):
        k.ledge(x0, x1, 40, True)
    k.bar(32, 377, 41)
    for x0 in range(379, 396, 4):
        k.bar(x0, x0 + 2, 40, 2)
    # The bracket that takes the top line down to the row of squares.
    k.stroke(k.at((405, 32.5), (397.5, 40)), 1)
    k.stroke(k.at((397.5, 41.5), (403.5, 47.5), (409.5, 47.5), (409.5, 50.5), (486.5, 50.5)), 1)
    for dot in range(489, 511, 3):
        k.bar(dot, dot, 50)
    k.stroke(k.at((411.5, 41), (411.5, 48.5), (512, 48.5)), 1)
    # The squares, one of them full.
    for x0 in (415, 429, 436, 443, 450, 457, 464):
        k.box(x0, 43, 4, 4)
    k.bar(422, 425, 43, 4)
    # Below them, two small squares and a line with gaps.
    for x0 in (403, 409):
        k.box(x0, 54, 4, 3)
    k.bar(415, 511, 54, 2)
    for x0, x1 in ((415, 426), (448, 460), (480, 492), (510, 511)):
        k.ledge(x0, x1, 56, False)
    # The two lines at the bottom, stepping down to the fork at the left.
    k.stroke(k.at((512, 60.5), (433.5, 60.5), (429.5, 64.5), (402, 64.5)), 1)
    k.bar(433, 511, 64, 2)
    # The fork: both its arms come to the point where the thick line starts.
    k.stroke(k.at((433.5, 65), (430.5, 68)), 2)
    k.shape(k.at((430.5, 67), (333, 67), (332.4, 68), (333, 69), (430.5, 69)))
    k.stroke(k.at((347.5, 55.5), (336, 55.5), (328.5, 63.5), (324, 63.5)), 1)
    k.stroke(k.at((328.5, 63.5), (332.6, 68)), 1.2)
    k.stroke(k.at((332.6, 68), (328.6, 72)), 1.2)
    k.finish(sheet)


def title_short_lines(sheet):
    """The lines of the titles with a small plate, Rhythm Game Edit, Ad Hoc and the gallery's, which
    have only two rows: the first as Home's, and a thick one under it that leads to a row of squares."""
    k = LineArt(29, 29, 483, 17)
    title_top_row(k, -3, 29)
    k.bar(29, 374, 40, 2)
    for x0 in (378, 383, 388, 393):
        k.bar(x0, x0 + 2, 40, 2)
    # A small mark pointing left, and the line from it that runs under the squares.
    k.stroke(k.at((402.5, 36), (402.5, 41.5), (509, 41.5)), 1)
    k.stroke(k.at((402.5, 36.6), (400.6, 38.5), (402.5, 40.4)), 1)
    for x0 in (406, 420, 427, 434, 441, 448, 455):
        k.box(x0, 36, 4, 4)
    k.bar(413, 416, 36, 4)
    k.finish(sheet)


# Each title's picture: its lines, where they are, and where its plate is and whether lines run under it.
TITLES = (
    ('menu_title_home.png', title_home_lines, (32, 29, 480, 47), 76, True),
    ('menu_title_myroom.png', title_home_lines, (32, 29, 480, 47), 76, True),
    ('menu_title_edit.png', title_short_lines, (29, 29, 483, 17), 46, False),
    ('menu_title_adhok.png', title_short_lines, (29, 29, 483, 17), 46, False),
    ('menu_title_gallery.png', title_short_lines, (29, 29, 483, 17), 46, False),
)


# ---- the title screen: its menu's words and PRESS START drawn in each language in the game's
# squared letters, the notice below them in the font, and the logo traced from its own pixels.
TITLE_WORDS = ('pictures.title.new_game', 'pictures.title.load_game', 'pictures.title.adhoc', 'pictures.title.press_start')
TITLE_NOTICE = '©SEGA / ©Crypton Future Media, Inc.'
TITLE_NAVY = (36, 67, 85)
TITLE_DIM = (70, 100, 118)


def title_word(c, text, box, chosen):
    """A word of the title's menu: white going grey inside a navy edge when chosen, navy when not."""
    x, y, w, h = box
    stroke, edge = 2.3, 2.2
    units = len(text) * 6 + (len(text) - 1) * 1.6
    ux = min(1.7, (w - 8 - stroke - edge) / units)
    width = units * ux + stroke
    left, top, height = x + 6, y + 2, 12
    c.paint(text_strokes(text, left, top, width, height, stroke + edge), TITLE_NAVY)
    fill = gradient_fill(top, top + height, ((0, (255, 255, 255)), (0.55, (225, 228, 232)), (1, (150, 176, 190)))) if chosen else TITLE_DIM
    c.paint(text_strokes(text, left + edge / 2, top + edge / 2, width - edge, height - edge, stroke), fill)


def gradient_fill(y0, y1, stops):
    def fn(_x, y):
        t = min(1.0, max(0.0, (y - y0) / ((y1 - y0) or 1)))
        for (a, ca), (b, cb) in zip(stops, stops[1:]):
            if t <= b:
                k = 0 if b == a else (t - a) / (b - a)
                return tuple(int(ca[i] + (cb[i] - ca[i]) * k) for i in range(3))
        return stops[-1][1]
    return fn


def logo_white(p):
    return p[3] > 200 and lum(p) > 200 and max(p[:3]) - min(p[:3]) < 40


def title_sheet(archive, drawer, folder, words):
    picture = 'title.png'
    w, h, pixels = decode_png(archive.read(picture))
    rects = layout_of(archive, picture)
    c = enlarged(archive, picture, pixels, w, h)
    for k, name in enumerate(('new_game', 'load_game', 'adhoc_mode')):
        for state in ('00', '01'):
            box = rects['%s%s.png' % (name, state)]
            wipe(c, box)
            title_word(c, words[k], box, state == '00')
    x, y, pw, ph = rects['press01.png']
    wipe(c, rects['press01.png'])
    text = words[3]
    units = len(text) * 6 + (len(text) - 1) * 1.6
    ux = min(1.25, (pw - 24) / units)
    width = units * ux + 1.8
    c.paint(text_strokes(text, x + 106 - width / 2, y + 4, width, 9, 1.8), TITLE_NAVY)
    c.paint([rect(x + 15, y + 16, pw - 15, 1)], TITLE_NAVY)
    # The notice in the free font if the one chosen has no copyright sign.
    notice = drawer if all(drawer.draws(ch) for ch in TITLE_NOTICE if ch != ' ') else Drawer(load_font(FREE_FONT), HINT_H * SCALE)
    for name in ('press01.png', 'press02.png'):
        px0, py0, _, _ = rects[name]
        if name == 'press02.png':
            wipe(c, rects[name])
        write_text(c, notice, TITLE_NOTICE, (px0 + 20, py0 + 17, 180, 12), TITLE_NAVY)
    # The logo, traced smooth: its white outline over all of it, then its colours over that.
    box = rects['maintitle.png']
    wipe(c, box)
    smooth_layer(c, pixels, box, lambda p, i, j: p[3] / 255, logo_white, WHITE + (255,))
    smooth_layer(c, pixels, box, ink_amount(pixels, box), lambda p: p[3] > 200, colour_pixels=logo_inside(pixels, box),
                 least=LOGO_SPECK)
    write_sheet(c, pixels, folder)


# ---- the demo after the title, with PRESS START in its corner. The makers' logos before it are left as
# the game has them: traced, they came out with too many flaws.
DEMO_WORD = 'pictures.title.press_start'
DEMO_LINE = 'Project DIVA Start...'
DEMO_SHADOW = (40, 40, 40)


def demo_start_sheet(archive, family, folder, words):
    """PRESS START, its line, and the words under it: white, with a soft shadow below."""
    picture = 'start.png'
    w, h, pixels = decode_png(archive.read(picture))
    c = Canvas(w, h)
    units = len(words) * 6 + (len(words) - 1) * 1.6
    stroke = 1.5
    width = min(110.0, units * 1.0 + stroke)
    c.paint(text_strokes(words, 5.7, 7.9, width, 6, stroke), DEMO_SHADOW, 170)
    c.paint(text_strokes(words, 5, 7, width, 6, stroke), WHITE)
    c.paint([rect(0, 16, 120, 1)], WHITE)
    small = Drawer(family, 7 * SCALE)
    write_text(c, small, DEMO_LINE, (6, 18, 92, 8), WHITE, False, DEMO_SHADOW)
    write_sheet(c, pixels, folder)


def logo_colour(p):
    """Part of a logo: anything not white, or of a colour of its own."""
    return lum(p) < 215 or max(p[:3]) - min(p[:3]) > 50


def iso_loops(field, w, h, level=0.5):
    """Where a field, given at the middles of pixels, crosses `level`: closed loops of points, found
    square by square between four middles and placed between them by how far each is from it, so
    that an edge the picture smoothed comes out smooth. Past the field is nothing."""
    def at(i, j):
        return field[j][i] if 0 <= i < w and 0 <= j < h else 0.0

    def cut(p, q, vp, vq):
        t = (level - vp) / (vq - vp) if vq != vp else 0.5
        return (p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t)
    links = {}

    def link(a, b):
        a, b = (round(a[0], 4), round(a[1], 4)), (round(b[0], 4), round(b[1], 4))
        links.setdefault(a, []).append(b)
        links.setdefault(b, []).append(a)
    for j in range(-1, h):
        for i in range(-1, w):
            v = (at(i, j), at(i + 1, j), at(i + 1, j + 1), at(i, j + 1))
            inside = [x >= level for x in v]
            if all(inside) or not any(inside):
                continue
            corners = ((i + 0.5, j + 0.5), (i + 1.5, j + 0.5), (i + 1.5, j + 1.5), (i + 0.5, j + 1.5))
            crossings = []
            for k in range(4):
                if inside[k] != inside[(k + 1) % 4]:
                    crossings.append((k, cut(corners[k], corners[(k + 1) % 4], v[k], v[(k + 1) % 4])))
            if len(crossings) == 2:
                link(crossings[0][1], crossings[1][1])
            else:
                # A saddle: which way it joins by the middle of the square.
                middle = sum(v) / 4 >= level
                p = [c[1] for c in crossings]
                if middle == inside[0]:
                    link(p[0], p[1])
                    link(p[2], p[3])
                else:
                    link(p[0], p[3])
                    link(p[1], p[2])
    loops = []
    while links:
        start = next(iter(links))
        loop, here, before = [start], start, None
        while True:
            ends = links.get(here, [])
            there = next((e for e in ends if e != before), None)
            if there is None:
                break
            ends.remove(there)
            links[there].remove(here)
            if not ends:
                del links[here]
            if not links.get(there):
                links.pop(there, None)
            before, here = here, there
            if here == start:
                break
            loop.append(here)
        if len(loop) >= 3:
            loops.append(loop)
    return loops


LOGO_SPECK = 1.5   # square pixels: less than this, traced from a logo, is a speck


def logo_inside(pixels, box):
    """The pixels inside a logo, with all four neighbours of it too; the rest see-through. Its edges
    were smoothed by mixing with the ground, and coloured from those they would come out in steps."""
    x0, y0, w, h = box
    out = [list(row) for row in pixels]

    def logo(i, j):
        return 0 <= i < w and 0 <= j < h and pixels[y0 + j][x0 + i][3] > 20 and logo_colour(pixels[y0 + j][x0 + i])
    for j in range(h):
        for i in range(w):
            if not (logo(i, j) and logo(i + 1, j) and logo(i - 1, j) and logo(i, j + 1) and logo(i, j - 1)):
                out[y0 + j][x0 + i] = (0, 0, 0, 0)
    return out


def ink_amount(pixels, box):
    """How much of each pixel is a logo's colour rather than white: its own opacity, times how far it
    has come from white towards the colour inside the logo nearest it."""
    core = nearest_colours(pixels, box, lambda p: p[3] > 200 and logo_colour(p) and lum(p) < 200)

    def amount(p, i, j):
        if p[3] <= 20:
            return 0.0
        inside = core.get((i, j), (0, 0, 0))
        towards = min(1.0, max(0.0, (255 - lum(p)) / max(1.0, 255 - lum(inside))))
        saturated = min(1.0, (max(p[:3]) - min(p[:3])) / max(1.0, max(inside[:3]) - min(inside[:3])))
        return p[3] / 255 * max(towards, saturated)
    return amount


def smoothed_loop(loop, passes=10, reach=3, corner=50):
    """A traced loop with the stairs of its pixels taken out: each point moved towards its
    neighbours and back a little less (so the shape does not shrink), over and over, but corners,
    where the way the loop goes turns by more than `corner` degrees over `reach` points, stay put."""
    n = len(loop)
    if n < 2 * reach + 3:
        return loop

    def turn(i):
        ax, ay = loop[(i - reach) % n]
        bx, by = loop[i]
        cx, cy = loop[(i + reach) % n]
        a1, a2 = math.atan2(by - ay, bx - ax), math.atan2(cy - by, cx - bx)
        return abs((math.degrees(a2 - a1) + 180) % 360 - 180)
    pinned = [turn(i) > corner for i in range(n)]
    pts = list(loop)
    for _ in range(passes):
        for k in (0.5, -0.53):
            pts = [p if pinned[i] else (p[0] + k * ((pts[i - 1][0] + pts[(i + 1) % n][0]) / 2 - p[0]),
                                        p[1] + k * ((pts[i - 1][1] + pts[(i + 1) % n][1]) / 2 - p[1]))
                   for i, p in enumerate(pts)]
    return pts


def loop_area(loop):
    """The area a loop closes, whichever way round it goes."""
    return abs(sum(ax * by - bx * ay for (ax, ay), (bx, by) in zip(loop, loop[1:] + loop[:1]))) / 2


def smooth_layer(c, pixels, box, amount, colour_from, fallback=(0, 0, 0, 255), colour_pixels=None, least=0.0):
    """Paint into the canvas what of the box is more than half `amount`, traced smooth, each point in
    the colour of the nearest pixel that `colour_from` accepts (of `colour_pixels`, if given). Loops
    closing less than `least` square pixels are specks, and left out."""
    x0, y0, bw, bh = box
    field = [[amount(pixels[y0 + j][x0 + i], i, j) for i in range(bw)] for j in range(bh)]
    loops = [loop for loop in iso_loops(field, bw, bh) if loop_area(loop) >= least]
    poly = as_one_polygon([[(px + x0, py + y0) for px, py in smoothed_loop(loop)] for loop in loops])
    if not poly:
        return
    near = nearest_colours(colour_pixels or pixels, box, colour_from)

    def colour(px, py):
        return near.get((min(bw - 1, max(0, int(px - x0))), min(bh - 1, max(0, int(py - y0)))), fallback)
    c.paint([poly], colour)


def titles(archive, out):
    """Takes the pictures already made with each name in this language, and redoes their lines."""
    for name, lines, box, plate_y, lines_below in TITLES:
        picture = archive.find(name)
        w, h, pixels = decode_png(archive.read(picture))
        path = os.path.join(out, '%08X.rgba' % colour_hash(pixels))
        if os.path.exists(path):
            with open(path, 'rb') as f:
                made_w, made_h = struct.unpack('<II', f.read(8))
                data = f.read()
            c = Canvas(made_w // SCALE, made_h // SCALE)
            for row in range(made_h):
                c.data[(made_h - 1 - row) * made_w * 4:(made_h - row) * made_w * 4] = data[row * made_w * 4:(row + 1) * made_w * 4]
        else:
            c = enlarged(archive, picture, pixels, w, h)
        wipe(c, box)
        lines(c)
        title_plate_edge(c, pixels, plate_y, lines_below)
        plate = layout_of(archive, picture)['title_%s_title.png' % name[len('menu_title_'):-len('.png')]]
        plate_icon(c, pixels, plate[0] + PLATE_ICON_X, plate[1] + PLATE_ICON_Y, PLATE_ICONS[name])
        write_sheet(c, pixels, out)


def arrow(direction, left, top, right, bottom):
    """A triangle filling a box given by its pixels' edges, pointing up, down, left or right."""
    mx, my = (left + right) / 2, (top + bottom) / 2
    return {'up': [(mx, top), (right, bottom), (left, bottom)], 'down': [(left, top), (right, top), (mx, bottom)],
            'left': [(left, my), (right, top), (right, bottom)], 'right': [(left, top), (right, my), (left, bottom)]}[direction]


def shrunk(poly, by):
    """The same triangle with its sides moved in by `by` pixels."""
    (ax, ay), (bx, by_), (cx, cy) = poly
    sides = [math.hypot(bx - cx, by_ - cy), math.hypot(ax - cx, ay - cy), math.hypot(ax - bx, ay - by_)]
    area = abs((bx - ax) * (cy - ay) - (cx - ax) * (by_ - ay)) / 2
    middle = ((sides[0] * ax + sides[1] * bx + sides[2] * cx) / sum(sides), (sides[0] * ay + sides[1] * by_ + sides[2] * cy) / sum(sides))
    keep = max(0.0, 1 - by / (2 * area / sum(sides)))
    return [(middle[0] + (px - middle[0]) * keep, middle[1] + (py - middle[1]) * keep) for px, py in poly]


def blurred(c, reach):
    """The canvas's coverage spread out, three box blurs one after another, as a list of alphas."""
    w, h = c.w, c.h
    a = [c.data[i * 4 + 3] for i in range(w * h)]
    r = max(1, int(reach * SCALE / 3))
    for _ in range(3):
        rows = [0] * (w * h)
        for y in range(h):
            for x in range(w):
                lo, hi = max(0, x - r), min(w - 1, x + r)
                rows[y * w + x] = sum(a[y * w + lo:y * w + hi + 1]) / (2 * r + 1)
        cols = [0] * (w * h)
        for x in range(w):
            for y in range(h):
                lo, hi = max(0, y - r), min(h - 1, y + r)
                cols[y * w + x] = sum(rows[k * w + x] for k in range(lo, hi + 1)) / (2 * r + 1)
        a = cols
    return a


def core_box(pixels, box, strong=0xE0):
    """The edges of the solid part of a piece."""
    x, y, w, h = box
    solid = [(i, j) for j in range(y, y + h) for i in range(x, x + w) if pixels[j][i][3] >= strong]
    return min(i for i, _ in solid), min(j for _, j in solid), max(i for i, _ in solid) + 1, max(j for _, j in solid) + 1


def glowing_arrow(sheet, pixels, box, direction):
    """An arrow of the lists: a light rim, a bright middle and the soft teal glow around it, all in
    the piece's own colours. One with no glow, the greyed out ones, is a single flat colour."""
    x, y, w, h = box
    left, top, right, bottom = core_box(pixels, box)
    shape = [(px - x, py - y) for px, py in arrow(direction, left, top, right, bottom)]
    solid = [pixels[j][i] for j in range(top, bottom) for i in range(left, right) if pixels[j][i][3] >= 0xE0]
    rim = max(solid, key=lambda p: p[0])[:3]
    middle = min(solid, key=lambda p: p[0])[:3]
    faint = [pixels[j][i] for j in range(y, y + h) for i in range(x, x + w) if 0 < pixels[j][i][3] < 0xE0]
    c = Canvas(w, h)
    if faint:
        mask = Canvas(w, h)
        mask.paint([shape], WHITE)
        glow = max(faint, key=lambda p: p[3])
        spread = blurred(mask, ARROW_GLOW_REACH)
        for i, cover in enumerate(spread):
            alpha = int(min(1.0, cover / 255 * 2.4) * glow[3])
            if alpha:
                c.data[i * 4:i * 4 + 4] = bytes(glow[:3]) + bytes([alpha])
        c.paint([shape], rim)
        c.paint([shrunk(shape, ARROW_RIM)], middle)
    else:
        c.paint([shape], middle)
    wipe(sheet, box)
    paste(sheet, c, x, y)


def arrow_in_bar(sheet, pixels, core, direction):
    """An arrow drawn into a row's bar: the bar runs on under it, then the arrow in its colour, with the
    soft halo of the colour the game put around it."""
    left, top, right, bottom = core
    colour = pixels[(top + bottom) // 2][(left + right) // 2][:3]
    around = [pixels[j][i] for j in range(top - 1, bottom + 1) for i in (left - 1, right)]
    halo = tuple(sum(p[k] for p in around) // len(around) for k in range(3))
    m = BAR_ARROW_MARGIN
    x0, x1 = (left - m) * SCALE, (right + m) * SCALE
    for row in range((top - m) * SCALE, (bottom + m) * SCALE):
        a = sheet.data[(row * sheet.w + x0 - 1) * 4:(row * sheet.w + x0) * 4]
        b = sheet.data[(row * sheet.w + x1) * 4:(row * sheet.w + x1 + 1) * 4]
        for col in range(x0, x1):
            t = (col - x0 + 0.5) / (x1 - x0)
            sheet.data[(row * sheet.w + col) * 4:(row * sheet.w + col + 1) * 4] = bytes(int(a[k] + (b[k] - a[k]) * t) for k in range(4))
    pw, ph = right - left + 2 * m, bottom - top + 2 * m
    piece = Canvas(pw, ph)
    for row in range(piece.h):
        src = (((top - m) * SCALE + row) * sheet.w + (left - m) * SCALE) * 4
        piece.data[row * piece.w * 4:(row + 1) * piece.w * 4] = sheet.data[src:src + piece.w * 4]
    shape = [(px - left + m, py - top + m) for px, py in arrow(direction, left, top, right, bottom)]
    mask = Canvas(pw, ph)
    mask.paint([shape], WHITE)
    for i, cover in enumerate(blurred(mask, BAR_ARROW_HALO)):
        k = min(1.0, cover / 255 * 1.8) * 0.75
        if k > 0:
            at = i * 4
            piece.data[at:at + 3] = bytes(int(piece.data[at + j] + (halo[j] - piece.data[at + j]) * k) for j in range(3))
    piece.paint([shape], colour)
    paste(sheet, piece, left - m, top - m)


def streaked(sheet, pixels, box, skip=()):
    """A bar or glow of the lists, which runs along in streaks: each of its rows smoothed along its
    length, so that the game's dithering and the steps where it fades out become one even blend,
    and the rows joined smoothly up and down. Boxes in `skip` (left, top, right, bottom), as arrows
    drawn into it afterwards, are left out of the blend."""
    x, y, w, h = box
    r = STREAK_REACH
    rows = []
    for j in range(h):
        sums = []
        for i in range(w):
            p = pixels[y + j][x + i]
            a = p[3]
            gone = any(left - 1 <= x + i <= right and top <= y + j < bottom for left, top, right, bottom in skip)
            k = 0 if gone else 1
            sums.append([p[0] * a * k, p[1] * a * k, p[2] * a * k, a * k, k])
        for _ in range(3):
            run = []
            for i in range(w):
                lo, hi = max(0, i - r), min(w - 1, i + r)
                run.append([sum(sums[n][c] for n in range(lo, hi + 1)) for c in range(5)])
            sums = run
        rows.append([(v[0] / v[3], v[1] / v[3], v[2] / v[3], v[3] / v[4]) if v[3] and v[4] else (0, 0, 0, 0) for v in sums])
    for oy in range(h * SCALE):
        gy = min(max((oy + 0.5) / SCALE - 0.5, 0), h - 1)
        j0 = int(gy)
        j1, ty = min(j0 + 1, h - 1), gy - j0
        at = ((y * SCALE + oy) * sheet.w + x * SCALE) * 4
        for ox in range(w * SCALE):
            gx = min(max((ox + 0.5) / SCALE - 0.5, 0), w - 1)
            i0 = int(gx)
            i1, tx = min(i0 + 1, w - 1), gx - i0
            out = [0.0] * 4
            for (jj, wy) in ((j0, 1 - ty), (j1, ty)):
                for (ii, wx) in ((i0, 1 - tx), (i1, tx)):
                    q = rows[jj][ii]
                    weight = wx * wy
                    out[3] += q[3] * weight
                    for c in range(3):
                        out[c] += q[c] * q[3] * weight
            alpha = out[3]
            px = bytes((min(255, int(out[c] / alpha + 0.5)) for c in range(3))) if alpha > 0 else bytes(3)
            sheet.data[at + ox * 4:at + ox * 4 + 4] = px + bytes([min(255, int(alpha + 0.5))])


def sheet_of_arrows(archive, out):
    """The arrows and cursors of the lists, and the slider's handle."""
    picture = 'menu_basic04.png'
    w, h, pixels = decode_png(archive.read(archive.find(picture)))
    rects = layout_of(archive, picture)
    c = enlarged(archive, picture, pixels, w, h)
    # The selected row's glows, before the arrows that go into some of them.
    for name in ('basic_glow_arw_a.png', 'basic_glow_arw.png'):
        x, y, bw, bh = rects[name]
        streaked(c, pixels, rects[name], [(left, y + BAR_ARROW_TOP - 1, right, y + BAR_ARROW_TOP + BAR_ARROW_H + 1) for left, right, _ in BAR_ARROWS])
    for name in ('basic_glow_menu_a.png', 'basic_glow_menu.png', 'basic_glow_fwin_a.png', 'basic_glow_fwin.png'):
        streaked(c, pixels, rects[name])
    for row in ('_a', ''):
        left, right = rects['basic_glow_win_l%s.png' % row], rects['basic_glow_win_r%s.png' % row]
        streaked(c, pixels, (left[0], left[1], right[0] + right[2] - left[0], left[3]))
    for name, direction in (('basic_yajirusi_upon.png', 'up'), ('basic_yajirusi_dnon.png', 'down'),
                            ('basic_yajirusi_upoff.png', 'up'), ('basic_yajirusi_dnoff.png', 'down'),
                            ('basic_cursor_l_on.png', 'left'), ('basic_cursor_r_on.png', 'right'),
                            ('basic_cursor_l_off.png', 'left'), ('basic_cursor_r_off.png', 'right'),
                            ('basic_cursor_up_on.png', 'up'), ('basic_cursor_dn_on.png', 'down'),
                            ('basic_cursor_up_off.png', 'up'), ('basic_cursor_dn_off.png', 'down')):
        glowing_arrow(c, pixels, rects[name], direction)
    # The pink arrows inside the selected row's glow, which the rows with values show.
    for name in ('basic_glow_arw_a.png', 'basic_glow_arw.png'):
        x, y, bw, bh = rects[name]
        for left, right, direction in BAR_ARROWS:
            arrow_in_bar(c, pixels, (left, y + BAR_ARROW_TOP, right, y + BAR_ARROW_TOP + BAR_ARROW_H), direction)
    marker_end(c, rects['basic_marker_up.png'], True)
    marker_end(c, rects['basic_marker_dn.png'], False)
    write_sheet(c, pixels, out)


def outlined(c, strokes, fill, outline=WHITE, border=1.0, square_ends=True):
    """Coloured strokes inside a white border, the look of the game's small icons. Each stroke is a
    list of points and a width; the border is the same strokes made wider, under them."""
    def shapes(extra):
        polys = []
        for points, width in strokes:
            w = width + 2 * extra
            for i in range(len(points) - 1):
                (ax, ay), (bx, by) = points[i], points[i + 1]
                if square_ends:
                    length = math.hypot(bx - ax, by - ay) or 1
                    ux, uy = (bx - ax) / length * w / 2, (by - ay) / length * w / 2
                    ax, ay, bx, by = ax - ux, ay - uy, bx + ux, by + uy
                polys.append(segment(ax, ay, bx, by, w))
        return polys
    c.paint(shapes(border), outline)
    c.paint(shapes(0), fill)


def magenta_down(top, bottom):
    """The icons' pink, lighter at the top."""
    return lambda x, y: tuple(int(a + (b - a) * min(1.0, max(0.0, (y - top) / (bottom - top)))) for a, b in zip(ICON_PINK_TOP, ICON_PINK_LOW))


def blue_down(top, bottom):
    return lambda x, y: tuple(int(a + (b - a) * min(1.0, max(0.0, (y - top) / (bottom - top)))) for a, b in zip(ICON_BLUE_TOP, ICON_BLUE_LOW))


def white_ground(c, pixels, box):
    """The white the game's icon sits on, square pixel for square pixel: its steps are the letters'
    own, going down one after another, not steps of a slanted line."""
    x, y, w, h = box
    c.paint([rect(i, j, 1, 1) for j in range(h) for i in range(w) if pixels[y + j][x + i][3] > 40], WHITE)


def icon_new(sheet, pixels, box):
    """NEW, its letters going down in steps, N into E into W, each line straight."""
    x, y, w, h = box
    c = Canvas(w, h)
    white_ground(c, pixels, box)
    outlined(c, [
        ([(1.5, 1.2), (1.5, 6)], 1), ([(1.5, 1.5), (4.5, 4.5)], 1), ([(4.5, 1.2), (4.5, 7.5)], 1),
        ([(4.5, 3.5), (6.5, 3.5)], 1), ([(4.5, 5.5), (6.5, 5.5)], 1), ([(4.5, 7.5), (6.5, 7.5)], 1),
        ([(6.5, 7.5), (6.5, 9.6), (7.5, 10.4), (8.5, 9.6), (8.5, 7.8)], 1),
        ([(8.5, 9.6), (9.5, 10.4), (10.5, 9.6), (10.5, 5.2)], 1),
    ], magenta_down(1, 10.5), border=0)
    wipe(sheet, box)
    paste(sheet, c, x, y)


def icon_note(sheet, box):
    """Two notes joined by a beam that rises on a straight slant."""
    x, y, w, h = box
    c = Canvas(w, h)
    heads = [(2.2, 9.2), (9.2, 8.2)]
    border = [circle(hx, hy, 2.15, 24) for hx, hy in heads]
    c.paint(border, WHITE)
    outlined(c, [([(3.5, 9), (3.5, 3.6)], 1), ([(10.5, 8), (10.5, 1.6)], 1), ([(3.5, 3.4), (10.5, 1.4)], 2)], magenta_down(1, 10.5))
    c.paint([[(hx + 1.25 * math.cos(t / 12 * math.tau) * 1.15 - 0.4 * math.sin(t / 12 * math.tau),
               hy + 1.0 * math.sin(t / 12 * math.tau)) for t in range(12)] for hx, hy in heads], magenta_down(1, 10.5))
    wipe(sheet, box)
    paste(sheet, c, x, y)


def icon_folder(sheet, box):
    """A blue plate with a white border, and a white folder on it."""
    x, y, w, h = box
    c = Canvas(w, h)
    # The plate is a folder too: its top steps down on a slant after the tab, as the game's does.
    c.paint([[(0, 0), (6.6, 0), (8.6, 2), (12, 2), (12, 11), (11, 12), (1, 12), (0, 11)]], WHITE)
    c.paint([[(1, 1), (6.2, 1), (8.2, 3), (11, 3), (11, 10.6), (10.6, 11), (1.4, 11), (1, 10.6)]], blue_down(1, 11))
    c.paint([[(3, 2), (5, 2), (5.6, 3.6), (9.2, 3.6), (9.2, 8.4), (3, 8.4)]], ICON_FOLDER_LIGHT)
    c.paint([[(3.7, 4.4), (8.5, 4.4), (8.5, 7.8), (3.7, 7.8)]], WHITE)
    wipe(sheet, box)
    paste(sheet, c, x, y)


def icon_set(sheet, pixels, box):
    """SET, its letters going down in steps as NEW's do."""
    x, y, w, h = box
    c = Canvas(w, h)
    white_ground(c, pixels, box)
    outlined(c, [
        ([(4.5, 1.5), (1.5, 1.5), (1.5, 3.5), (4.5, 3.5), (4.5, 5.5), (1.5, 5.5)], 1),
        ([(4.5, 3.5), (4.5, 7.5)], 1), ([(4.5, 3.5), (6.5, 3.5)], 1), ([(4.5, 5.5), (6.5, 5.5)], 1), ([(4.5, 7.5), (6.5, 7.5)], 1),
        ([(7.5, 6.5), (10.5, 6.5)], 1), ([(8.5, 6.5), (8.5, 10.5)], 1),
    ], blue_down(1, 10.5), border=0)
    wipe(sheet, box)
    paste(sheet, c, x, y)


def icon_ring(sheet, pixels, box):
    """The ring beside the titles: a teal band round a dark middle, inside a dark border."""
    x, y, w, h = box
    c = Canvas(w, h)
    cx, cy = w / 2, h / 2
    dark = pixels[y + 4][x + 4][:3] if pixels[y + 4][x + 4][3] else (65, 75, 90)
    c.paint([circle(cx, cy, 6, 72)], ICON_RING_DARK)
    c.paint(ring(cx, cy, 5, 2.1), lambda px, py: tuple(int(a + (b - a) * min(1.0, max(0.0, (px + py) / (w + h)))) for a, b in zip(ICON_RING_A, ICON_RING_B)))
    wipe(sheet, box)
    paste(sheet, c, x, y)


BADGE_INK = (55, 55, 62)
BADGE_LINE = 0.85


def through(points, closed=False, steps=8):
    """A smooth curve through the points (Catmull-Rom), as many short steps."""
    pts = list(points)
    n = len(pts)
    out = []
    last = n if closed else n - 1
    for i in range(last):
        p0 = pts[(i - 1) % n] if closed or i > 0 else pts[0]
        p1, p2 = pts[i], pts[(i + 1) % n]
        p3 = pts[(i + 2) % n] if closed or i + 2 < n else pts[-1]
        for k in range(steps):
            t = k / steps
            out.append(tuple(0.5 * (2 * p1[d] + (-p0[d] + p2[d]) * t + (2 * p0[d] - 5 * p1[d] + 4 * p2[d] - p3[d]) * t * t
                                    + (-p0[d] + 3 * p1[d] - 3 * p2[d] + p3[d]) * t ** 3) for d in (0, 1)))
    out.append(pts[0] if closed else pts[-1])
    return out


def pen(points, width=BADGE_LINE):
    """A line with round joins and ends at any width."""
    return polyline(points, width) + [circle(x, y, width / 2, 12) for x, y in points]


def mirrored(points):
    """The same points, the other way round across the badge."""
    return [(32 - x, y) for x, y in points]


def project_badge(c, family, box):
    """The Project DIVA badge (in the demo's corner and the lists): drawn anew, as the game's is too
    small to trace: Miku's head and twin tails in lines, Project DIVA on her front."""
    wipe(c, box)
    x0, y0, _, _ = box

    def at(points):
        return [(x0 + x, y0 + y) for x, y in points]
    c.paint([rounded(x0 + 0.5, y0 + 0.5, 31, 31, 3.5)], (255, 255, 255))
    lines = []
    # the twin tails, down each side behind the head and body
    tail = [(8.6, 5.2), (5.6, 9.5), (4.4, 16), (4.6, 23), (6.2, 28.2), (9.2, 30.4), (8.6, 26), (8.2, 20.5), (8.6, 16.5)]
    for t in (tail, mirrored(tail)):
        lines += pen(at(through(t)))
    # the head, and the fringe across it
    head = [(16, 2.2), (21.6, 3.0), (24.3, 7.2), (24.5, 12), (22.6, 15.8), (16, 17.0), (9.4, 15.8), (7.5, 12), (7.7, 7.2), (10.4, 3.0)]
    c.paint([at(through(head, True))], (255, 255, 255))
    lines += pen(at(through(head, True)))
    fringe = [(7.9, 10.5), (10.6, 6.4), (12.6, 4.6), (13.4, 9.2), (15.2, 5.0), (16.4, 4.4), (18.6, 9.2), (19.4, 4.6), (21.4, 6.4), (24.1, 10.5)]
    lines += pen(at(fringe))
    # the hair ties on top
    for cx in (9.4, 22.6):
        lines.append(at([(cx, 1.9), (cx + 1.3, 3.2), (cx, 4.5), (cx - 1.3, 3.2)]))
    # the body, with Project DIVA on it
    body = [(11.4, 16.6), (10.2, 25.8), (21.8, 25.8), (20.6, 16.6)]
    c.paint([at(body)], (255, 255, 255))
    lines += pen(at(body + body[:1]))
    lines += pen(at(through([(16 - 4.8 * math.cos(a), 23.0 + 1.9 * math.sin(a)) for a in [i * math.tau / 24 for i in range(24)]], True, 2)), 0.6)
    # the feet
    for cx in (13.2, 18.8):
        lines += pen(at(through([(cx - 2.2, 25.9), (cx - 2.3, 27.6), (cx, 28.4), (cx + 2.3, 27.6), (cx + 2.2, 25.9)])))
    c.paint(lines, BADGE_INK)
    # the face
    face = []
    for cx in (12.7, 19.3):
        face.append(at(ellipse(cx, 12.1, 1.05, 1.5, 0, 24)))
    face += pen(at(through([(15.0, 14.5), (16, 15.1), (17.0, 14.5)])), 0.65)
    c.paint(face, BADGE_INK)
    c.paint([at(circle(12.95, 11.5, 0.42, 12)), at(circle(19.55, 11.5, 0.42, 12))], (255, 255, 255))
    write_text(c, Drawer(family, int(3.2 * SCALE)), 'DIVA', (x0 + 12, y0 + 21, 8, 4), BADGE_INK, True)
    write_text(c, Drawer(family, int(3.4 * SCALE)), 'Project', (x0 + 12, y0 + 17, 8, 4), BADGE_INK, True)


def sheet_of_icons(archive, family, out):
    """The small icons of the lists, on the sheet that also has the Project DIVA badge and the digits."""
    picture = 'menu_basic03.png'
    w, h, pixels = decode_png(archive.read(archive.find(picture)))
    rects = layout_of(archive, picture)
    c = enlarged(archive, picture, pixels, w, h)
    icon_new(c, pixels, rects['basic_icon_new.png'])
    icon_note(c, rects['basic_icon_note.png'])
    icon_folder(c, rects['basic_icon_folder.png'])
    icon_set(c, pixels, rects['basic_icon_set.png'])
    project_badge(c, family, rects['basic_icon_pjd.png'])
    for digit, strokes in DIGITS.items():
        shadowed(c, rects['basic_num_%s.png' % digit], strokes)
    percent_sign(c, rects['basic_num_percent.png'])
    x, y, bw, bh = rects['basic_achivement.png']
    a = Canvas(bw, bh)
    a.paint(pixel_strokes(ACHIEVEMENT), WHITE)
    wipe(c, rects['basic_achivement.png'])
    paste(c, a, x, y)
    write_sheet(c, pixels, out)


def rounded(x, y, w, h, r, corners='tl tr br bl', steps=8):
    """A rectangle with the named corners rounded, as one polygon."""
    out = []
    for name, cx, cy, start in (('tl', x + r, y + r, 180), ('tr', x + w - r, y + r, 270),
                                ('br', x + w - r, y + h - r, 0), ('bl', x + r, y + h - r, 90)):
        if name in corners.split():
            out += [(cx + r * math.cos(math.radians(start + 90 * k / steps)), cy + r * math.sin(math.radians(start + 90 * k / steps)))
                    for k in range(steps + 1)]
        else:
            out.append({'tl': (x, y), 'tr': (x + w, y), 'br': (x + w, y + h), 'bl': (x, y + h)}[name])
    return out


def with_hole(outer, inner):
    """One polygon with a hole, joined by an edge taken twice, as ring() does."""
    return outer + outer[:1] + inner[:1] + inner[:0:-1] + inner[:1] + outer[:1]


def pixel_strokes(paths, width=1.0, dx=0.0, dy=0.0):
    """Lines through the middles of the game's pixels, given by column and row, with square ends,
    so that straight runs cover their pixels exactly and the steps become slants."""
    polys = []
    for points in paths:
        points = [(px + 0.5 + dx, py + 0.5 + dy) for px, py in points]
        if len(points) == 1:
            px, py = points[0]
            polys.append(rect(px - width / 2, py - width / 2, width, width))
            continue
        for i in range(len(points) - 1):
            (ax, ay), (bx, by) = points[i], points[i + 1]
            length = math.hypot(bx - ax, by - ay) or 1
            ux, uy = (bx - ax) / length * width / 2, (by - ay) / length * width / 2
            polys.append(segment(ax - ux, ay - uy, bx + ux, by + uy, width))
    return polys


def arc_band(cx, cy, r, width, from_deg, to_deg, steps=24):
    """A curved band, `width` thick, along a circle."""
    def at(radius, k):
        a = math.radians(from_deg + (to_deg - from_deg) * k / steps)
        return cx + radius * math.cos(a), cy + radius * math.sin(a)
    return [at(r + width / 2, k) for k in range(steps + 1)] + [at(r - width / 2, k) for k in range(steps, -1, -1)]


# The icons on the title plates: a frame with soft corners and a picture inside, in the plate's teal.
def plate_icon_home(c, colour):
    """A head: the hair's cap on top, the hair down the left side shaded with fine slanted lines,
    the cheek and chin on the right."""
    cap = [(8.5 + 4.5 * math.cos(math.radians(a)), 4.5 + 1.55 * math.sin(math.radians(a))) for a in range(0, 360, 10)]
    c.paint([cap, rounded(7, 2, 2, 2, 0.6, 'tl tr')], colour)
    c.paint(pixel_strokes([[(3, 5), (3, 9.6), (4.4, 11), (8, 11)], [(9, 6), (8, 7.2), (8, 10.6)],
                           [(12, 5), (12, 9.4), (10.2, 11.6), (9.2, 12), (7, 12)]]), colour)
    c.paint([segment(k - 6.5, 12.5, k + 0.5, 5.5, 0.62) for k in range(5, 17, 2)], colour, clip=(4, 6, 8, 11))


def plate_icon_film(c, colour):
    """A frame of film: holes down both sides, a window in the middle."""
    c.paint([with_hole(rect(3, 4, 10, 7), rect(6, 5, 4, 5)[::-1])], colour)
    punch(c, [rounded(hx, hy, 1, 1, 0.3) for hx in (4, 11) for hy in (5, 7, 9)])


def plate_icon_folder(c, colour):
    """A folder seen a little from the side: its left and bottom thicker."""
    c.paint([[(3, 4.2), (4.2, 3), (5, 3), (5, 10), (13, 10), (13, 11), (12, 12), (4, 12), (3, 11)]], colour)
    c.paint(pixel_strokes([[(4, 3), (7, 3), (8.5, 5), (11, 5), (12, 6), (12, 10)]]), colour)


def plate_icon_door(c, colour):
    """A door standing on the frame's bottom edge, with its handle."""
    c.paint([rect(4, 4, 1, 9.5), rect(4, 4, 8, 1), rect(10, 4, 2, 9.5)], colour)
    c.paint([rounded(9, 8, 1.6, 2, 0.5, 'tl bl')], colour)


def plate_icon_waves(c, colour):
    """Three waves going out, for playing together nearby."""
    # Each ends flat, on the rows where the game's ends were.
    for cx, r, top, bottom in ((2.5, 2.5, 5, 10), (5.8, 3.2, 4, 11), (7.14, 4.86, 3, 12)):
        reach = math.degrees(math.asin(min(1.0, (7.5 - top - 0.6) / r)))
        c.paint([arc_band(cx, 7.5, r, 1.8, -reach, reach)], colour, clip=(0, top, PLATE_ICON_W, bottom))


PLATE_ICONS = {
    'menu_title_home.png': plate_icon_home, 'menu_title_myroom.png': plate_icon_door,
    'menu_title_edit.png': plate_icon_folder, 'menu_title_adhok.png': plate_icon_waves,
    'menu_title_gallery.png': plate_icon_film,
}


def blend(sheet, piece, x, y):
    """Lay a piece over what is there, by its alpha."""
    for row in range(piece.h):
        src = row * piece.w * 4
        dst = ((y * SCALE + row) * sheet.w + x * SCALE) * 4
        for i in range(0, piece.w * 4, 4):
            cover = piece.data[src + i + 3] / 255
            if not cover:
                continue
            under = sheet.data[dst + i + 3] / 255
            out = cover + under * (1 - cover)
            for k in range(3):
                sheet.data[dst + i + k] = int((piece.data[src + i + k] * cover + sheet.data[dst + i + k] * under * (1 - cover)) / out)
            sheet.data[dst + i + 3] = int(out * 255)


def plate_icon(sheet, pixels, x, y, draw):
    """An icon in a frame, as on the title plates, redrawn over the ground beside it."""
    colour = pixels[y + 7][x][:3]
    c = Canvas(PLATE_ICON_W, PLATE_ICON_H)
    for row in range(PLATE_ICON_H):
        ground = tuple(pixels[y + row][x - 1])
        c.paint([rect(0, row, PLATE_ICON_W, 1)], ground[:3], ground[3])
    picture = Canvas(PLATE_ICON_W, PLATE_ICON_H)
    draw(picture, colour)
    blend(c, picture, 0, 0)
    frame = with_hole(rounded(0, 0, PLATE_ICON_W, PLATE_ICON_H, 1.5), rounded(2, 2, PLATE_ICON_W - 4, PLATE_ICON_H - 4, 0.8)[::-1])
    c.paint([frame], colour)
    paste(sheet, c, x, y)


# The game's small digits, white with a dark shadow down and to the right; each list is one line
# through the pixels' middles, by column and row.
DIGITS = {
    '1': [[(1, 0), (2, 0), (2, 7)]],
    '2': [[(0, 1), (1, 0), (3, 0), (4, 1), (4, 2), (0, 6), (0, 7), (4, 7)]],
    '3': [[(0, 1), (1, 0), (3, 0), (4, 1), (4, 2), (3, 3), (2, 3)], [(3, 3), (4, 4), (4, 6), (3, 7), (1, 7), (0, 6)]],
    '4': [[(3, 0), (0, 4.2), (0, 5), (4, 5)], [(3, 0), (3, 7)]],
    '5': [[(4, 0), (0, 0), (0, 3), (3, 3), (4, 4), (4, 6), (3, 7), (1, 7), (0, 6)]],
    '6': [[(4, 1), (3, 0), (1, 0), (0, 1), (0, 6), (1, 7), (3, 7), (4, 6), (4, 4), (3, 3), (0, 3)]],
    '7': [[(0, 0), (4, 0), (4, 1.5), (2, 5.5), (2, 7)]],
    '8': [[(1, 3), (0, 2), (0, 1), (1, 0), (3, 0), (4, 1), (4, 2), (3, 3), (1, 3), (0, 4), (0, 6), (1, 7), (3, 7), (4, 6), (4, 4), (3, 3)]],
    '9': [[(4, 4), (1, 4), (0, 3), (0, 1), (1, 0), (3, 0), (4, 1), (4, 6), (3, 7), (1, 7), (0, 6)]],
    '0': [[(1, 0), (3, 0), (4, 1), (4, 6), (3, 7), (1, 7), (0, 6), (0, 1), (1, 0)]],
}
ACHIEVEMENT = [
    [(0, 4), (0, 1), (1, 0), (2, 0), (3, 1), (3, 4)], [(0, 2), (3, 2)],
    [(7, 0), (6, 0), (5, 1), (5, 3), (6, 4), (7, 4)],
    [(9, 0), (9, 4)], [(12, 0), (12, 4)], [(9, 2), (12, 2)],
    [(14, 0), (14, 4)],
    [(18, 0), (16, 0), (16, 4), (18, 4)], [(16, 2), (18, 2)],
    [(20, 0), (20, 2), (21, 3), (22, 2), (22, 0)], [(21, 3), (21, 4)],
    [(26, 0), (24, 0), (24, 4), (26, 4)], [(24, 2), (26, 2)],
    [(28, 4), (28, 0), (29, 0), (30, 1), (31, 0), (32, 0), (32, 4)], [(30, 1), (30, 2)],
    [(36, 0), (34, 0), (34, 4), (36, 4)], [(34, 2), (36, 2)],
    [(38, 4), (38, 0), (41, 3)], [(41, 0), (41, 4)],
    [(43, 0), (45, 0)], [(44, 0), (44, 4)],
]


def shadowed(sheet, box, strokes, extra=()):
    """White strokes over a dark shadow one pixel down and to the right."""
    x, y, w, h = box
    c = Canvas(w, h)
    c.paint(pixel_strokes(strokes, dx=1, dy=1) + [[(px + 1, py + 1) for px, py in poly] for poly in extra], SHADOW, DIGIT_SHADOW_ALPHA)
    c.paint(pixel_strokes(strokes) + list(extra), WHITE)
    wipe(sheet, box)
    paste(sheet, c, x, y)


def percent_sign(sheet, box):
    """Two small rings and the slash between them."""
    rings = ring(1.5, 1.5, 1.45, 0.55, 24) + ring(6.5, 6.5, 1.45, 0.55, 24)
    shadowed(sheet, box, [[(5, 0), (2, 7)]], rings)


def marker_end(sheet, box, up):
    """An end of the scroll marker: round, its edge lighter than its middle."""
    x, y, w, h = box
    c = Canvas(w, h)
    cy, sign = (h, -1) if up else (0, 1)
    def half(rx, ry):
        return [(w / 2 + rx * math.cos(math.radians(a)), cy + sign * ry * math.sin(math.radians(a))) for a in range(0, 181, 10)]
    c.paint([half(w / 2, h)], MARKER_EDGE)
    c.paint([half(w / 2 - 0.9, h - 0.9)], MARKER_FILL, 254)
    wipe(sheet, box)
    paste(sheet, c, x, y)


def slider_end(sheet, pixels, box, top):
    """The slider's track with its end rounded off, the edge one pixel wide in the game's colours."""
    x, y, w, h = box
    edge, fill = pixels[y + h // 2][x][:3], pixels[y + h // 2][x + w // 2][:3]
    c = Canvas(w, h)
    corners = 'tl tr' if top else 'bl br'
    c.paint([rounded(0, 0, w, h, SLIDER_ROUND, corners)], edge)
    c.paint([rounded(1, 1 if top else 0, w - 2, h - 1, SLIDER_ROUND - 0.7, corners)], fill)
    wipe(sheet, box)
    paste(sheet, c, x, y)


# ---- tracing the game's pixel art
def pixel_loops(mask):
    """The edges of the set pixels, as closed loops of corner points, the set side on the right.
    Pixels touching only at a corner count as joined, so that one pixel wide slanted lines hold."""
    out_of = {}
    for (i, j) in mask:
        for side, (a, b) in enumerate((((i, j), (i + 1, j)), ((i + 1, j), (i + 1, j + 1)),
                                       ((i + 1, j + 1), (i, j + 1)), ((i, j + 1), (i, j)))):
            other = ((i, j - 1), (i + 1, j), (i, j + 1), (i - 1, j))[side]
            if other not in mask:
                out_of.setdefault(a, []).append(b)
    loops = []
    while out_of:
        start = next(iter(out_of))
        loop, here, heading = [start], start, None
        while True:
            ends = out_of[here]
            if len(ends) > 1 and heading:
                # Where four edges meet, turn left: that keeps the pixels across the corner together.
                left = (heading[1], -heading[0])
                there = next((e for e in ends if (e[0] - here[0], e[1] - here[1]) == left), ends[0])
            else:
                there = ends[0]
            ends.remove(there)
            if not ends:
                del out_of[here]
            heading = (there[0] - here[0], there[1] - here[1])
            here = there
            if here == start:
                break
            loop.append(here)
        loops.append(loop)
    return loops


def slanted(loop):
    """A loop of pixel corners with its steps cut into slants. A corner is a step when the next or
    the last corner turns the other way one pixel from it; any other corner stays sharp."""
    corners = []
    n = len(loop)
    for k in range(n):
        a, b, c = loop[k - 1], loop[k], loop[(k + 1) % n]
        d1, d2 = (b[0] - a[0], b[1] - a[1]), (c[0] - b[0], c[1] - b[1])
        if d1 != d2:
            corners.append((b, d1[0] * d2[1] - d1[1] * d2[0]))
    m = len(corners)
    if m < 3:
        return [p for p, _ in corners]
    def run(p, q):
        return abs(p[0] - q[0]) + abs(p[1] - q[1])
    out = []
    for k in range(m):
        (p, turn), (q, last_turn), (r, next_turn) = corners[k], corners[k - 1], corners[(k + 1) % m]
        step = (last_turn != turn and run(q, p) == 1) or (next_turn != turn and run(p, r) == 1)
        if not step:
            out.append(p)
            continue
        back = (p[0] + (q[0] - p[0]) * 0.5 / run(q, p), p[1] + (q[1] - p[1]) * 0.5 / run(q, p))
        on = (p[0] + (r[0] - p[0]) * 0.5 / run(p, r), p[1] + (r[1] - p[1]) * 0.5 / run(p, r))
        for point in (back, on):
            if not out or out[-1] != point:
                out.append(point)
    if len(out) > 1 and out[0] == out[-1]:
        out.pop()
    return out


def as_one_polygon(loops):
    """Loops joined into one polygon by edges taken twice, so filling by crossings makes holes."""
    if not loops:
        return []
    anchor = loops[0][0]
    out = []
    for loop in loops:
        out += [anchor] + loop + [loop[0]]
    return out + [anchor]


def nearest_colours(pixels, box, wanted):
    """For each pixel of the box, the colour of the nearest pixel that `wanted` accepts."""
    from collections import deque
    x, y, w, h = box
    near = {}
    queue = deque()
    for j in range(h):
        for i in range(w):
            p = pixels[y + j][x + i]
            if wanted(p):
                near[(i, j)] = tuple(p)
                queue.append((i, j))
    while queue:
        i, j = queue.popleft()
        for a, b in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1)):
            if 0 <= a < w and 0 <= b < h and (a, b) not in near:
                near[(a, b)] = near[(i, j)]
                queue.append((a, b))
    return near


def traced_layer(c, pixels, box, inside, colour_from=None, at=(0, 0), grow=0.0):
    """Paint into the canvas, at `at`, the pixels of the box that `inside` accepts, traced; each
    point takes the colour of the nearest pixel that `colour_from` accepts (the same by default)."""
    x, y, w, h = box
    mask = {(i, j) for j in range(h) for i in range(w) if inside(pixels[y + j][x + i])}
    if not mask:
        return
    near = nearest_colours(pixels, box, colour_from or inside)
    loops = [slanted(loop) for loop in pixel_loops(mask)]
    if grow:
        loops = [grown(loop, grow) for loop in loops]
    poly = as_one_polygon([[(px + at[0], py + at[1]) for px, py in loop] for loop in loops if len(loop) >= 3])
    def colour(px, py):
        return near.get((min(w - 1, max(0, int(px - at[0]))), min(h - 1, max(0, int(py - at[1])))), (0, 0, 0, 0))
    c.paint([poly], colour)


def grown(loop, by):
    """The loop's edges moved out by `by`, the set side being on the right as it goes."""
    n = len(loop)
    out = []
    for k in range(n):
        a, b, c = loop[k - 1], loop[k], loop[(k + 1) % n]
        def normal(p, q):
            dx, dy = q[0] - p[0], q[1] - p[1]
            length = math.hypot(dx, dy) or 1
            return -dy / length, dx / length
        n1, n2 = normal(a, b), normal(b, c)
        mx, my = n1[0] + n2[0], n1[1] + n2[1]
        size = mx * n1[0] + my * n1[1]
        if abs(size) < 1e-6:
            out.append((b[0] - n1[0] * by, b[1] - n1[1] * by))
        else:
            out.append((b[0] - mx / size * by, b[1] - my / size * by))
    return out


def filled_in(pixels, box, taken):
    """The pixels with those that `taken` accepts replaced by the nearest of the others, so the
    picture can be enlarged without them and they can be drawn back over it, traced."""
    x, y, w, h = box
    near = nearest_colours(pixels, box, lambda p: not taken(p))
    out = [list(row) for row in pixels]
    for j in range(h):
        for i in range(w):
            if taken(pixels[y + j][x + i]):
                out[y + j][x + i] = near.get((i, j), (0, 0, 0, 0))
    return out


def traced_piece(sheet, pixels, box, layers):
    """A piece redrawn from its own pixels: each layer is (inside, colour_from), painted in order."""
    x, y, w, h = box
    c = Canvas(w, h)
    for inside, colour_from in layers:
        traced_layer(c, pixels, box, inside, colour_from)
    wipe(sheet, box)
    paste(sheet, c, x, y)


# ---- the card before a song, and the results screen
def lum(p):
    return (p[0] * 3 + p[1] * 6 + p[2]) / 10


def card_line(p):
    """The card's white line art, not the teal glow it frames."""
    return p[3] > 150 and lum(p) > 170 and max(p[:3]) - min(p[:3]) < 40


def traced_over(sheet, pixels, box, inside, colour_from=None):
    """A layer of a box traced and laid over what the sheet has there."""
    c = Canvas(box[2], box[3])
    traced_layer(c, pixels, box, inside, colour_from)
    blend(sheet, c, box[0], box[1])


# The circle at a corner of the card's line art, by where its right side and bottom are, and its radius.
CARD_CIRCLES = (('info_free_ele01.png', 352, 104), ('info_free_ele02.png', 470, 104), ('info_free_ele03.png', 509, 223))
CARD_CIRCLE_R = 7.6
CARD_SHADOW_AT = (2.0, 3.0)    # where the lines' shadow falls, from the game's
CARD_SHADOW_SPREAD = 0.5       # how soft it is: as tight as the game's
CARD_SHADOW_ALPHA = 66         # how dark under a line one pixel wide
CARD_SHADOW_MOST_ALPHA = 140
CARD_SHADOW_REACH = 5          # how far from a line its old shadow is taken out
LINE_SHADOW_ALPHA = 61         # the titles' and the footer's lines: the same soft shadow
LINE_SHADOW_COLOUR = (30, 30, 30)
LINE_ART_W, LINE_ART_H = 481, 28   # the footer's line art, to the sheet's edge, with room for its shadow
CARD_LINE_OFF = 0.6      # the straight lines that run into it are this far past its middle


def card_circle_parts(right, bottom):
    """The circle's middle, and the triangle inside it, from its right side and bottom."""
    r = CARD_CIRCLE_R
    cx, cy = right + 0.5 - r, bottom + 0.5 - r
    triangle = [(cx + 2.1, cy - 3.9), (cx + 2.1, cy + 2.1), (cx - 3.9, cy + 2.1)]
    return cx, cy, triangle


def inside(point, poly):
    """Whether a point is inside a polygon, by crossings."""
    x, y = point
    hit = False
    for (ax, ay), (bx, by) in zip(poly, poly[1:] + poly[:1]):
        if (ay > y) != (by > y) and x < ax + (y - ay) * (bx - ax) / (by - ay):
            hit = not hit
    return hit


def without_card_circles(pixels):
    """The card's pixels with its corner circles and their triangles taken out of the white lines,
    so the tracer leaves them to be drawn round."""
    out = [list(row) for row in pixels]
    for _, right, bottom in CARD_CIRCLES:
        cx, cy, triangle = card_circle_parts(right, bottom)
        for j in range(int(cy - 11), int(cy + 11)):
            for i in range(int(cx - 11), int(cx + 11)):
                d = math.hypot(i + 0.5 - cx, j + 0.5 - cy)
                # The arc is three quarters round: the inner frame's corner passes by the missing one.
                on_arc = abs(d - CARD_CIRCLE_R) <= 1.2 and not (i + 0.5 < cx and j + 0.5 < cy)
                if card_line(pixels[j][i]) and (on_arc or inside((i + 0.5, j + 0.5), triangle)):
                    out[j][i] = (0, 0, 0, 0)
    return out


def card_circle(layer, pixels, box, right, bottom):
    """The corner circle: three quarters of a true circle from the line coming down to the line
    coming in from the left, both run on to meet it, and the triangle inside."""
    cx, cy, triangle = card_circle_parts(right, bottom)
    r, off = CARD_CIRCLE_R, CARD_LINE_OFF
    colour = next(tuple(pixels[j][i][:3]) for j in range(bottom - 2, bottom + 1) for i in range(right - 10, right) if card_line(pixels[j][i]))
    reach = math.sqrt(r * r - off * off)
    start, end = math.atan2(-reach, off), math.atan2(off, -reach) + 2 * math.pi * 0
    if end < start:
        end += 2 * math.pi
    arc = [(cx + r * math.cos(start + (end - start) * k / 48), cy + r * math.sin(start + (end - start) * k / 48)) for k in range(49)]
    path = [(cx + off, cy - r - 1.6)] + arc + [(cx - r - 1.6, cy + off)]
    x, y, w, h = box
    layer.paint([[(px - x, py - y) for px, py in thick(path, 1.0)], [(px - x, py - y) for px, py in triangle]], colour)


def card_shadow(p):
    """The soft black the game put under the card's lines, down and to the right of them."""
    return 0 < p[3] < CARD_SHADOW_MOST_ALPHA and max(p[:3]) < 10


def box_blur(alpha, w, h, r, passes=3):
    """A list of alphas, w by h, blurred by boxes 2r+1 wide, with running sums."""
    size = 2 * r + 1
    for _ in range(passes):
        rows = [0.0] * (w * h)
        for y in range(h):
            base = y * w
            run = sum(alpha[base + min(max(k, 0), w - 1)] for k in range(-r, r + 1))
            for x in range(w):
                rows[base + x] = run / size
                run += alpha[base + min(x + r + 1, w - 1)] - alpha[base + max(x - r, 0)]
        cols = [0.0] * (w * h)
        for x in range(w):
            run = sum(rows[min(max(k, 0), h - 1) * w + x] for k in range(-r, r + 1))
            for y in range(h):
                cols[y * w + x] = run / size
                run += rows[min(y + r + 1, h - 1) * w + x] - rows[max(y - r, 0) * w + x]
        alpha = cols
    return alpha


def drop_shadow(layer, darkest, colour=(0, 0, 0)):
    """The layer's shadow as the game drew it: its shape blurred, moved down and to the right, as
    dark as `darkest` under a line one pixel wide, darker under what is filled."""
    w, h = layer.w, layer.h
    r = max(1, int(CARD_SHADOW_SPREAD * SCALE))
    dx, dy = int(CARD_SHADOW_AT[0] * SCALE), int(CARD_SHADOW_AT[1] * SCALE)
    # A piece is laid beside others, its lines going on into theirs: so a line that reaches its
    # edge is taken to go on past it, as far as the shadow reaches, and the shadow is the same on
    # both sides of where two pieces meet instead of stopping at it.
    pad = r + max(dx, dy) + 1
    pw, ph = w + 2 * pad, h + 2 * pad
    alpha = [0] * (pw * ph)
    for y in range(ph):
        sy = min(h - 1, max(0, y - pad))
        for x in range(pw):
            sx = min(w - 1, max(0, x - pad))
            alpha[y * pw + x] = layer.data[(sy * w + sx) * 4 + 3]
    padded = box_blur(alpha, pw, ph, r)
    # What falls into the piece from just past its top and left edge.
    spread_from = lambda x, y: padded[(y + pad) * pw + x + pad]
    # How high a one pixel line comes out of the same blur.
    probe_w = 8 * r
    line = [255.0 if probe_w // 2 - SCALE // 2 <= i < probe_w // 2 + SCALE // 2 else 0.0 for i in range(probe_w)]
    peak = max(box_blur(line, probe_w, 1, r))
    shadow = Canvas(w // SCALE, h // SCALE)
    for ty in range(h):
        for tx in range(w):
            v = spread_from(tx - dx, ty - dy)
            if v <= 0.5:
                continue
            at = (ty * w + tx) * 4
            shadow.data[at:at + 3] = bytes(colour)
            shadow.data[at + 3] = min(255, int(v / peak * darkest))
    return shadow


def sheet_of_card(archive, out):
    """The card before a song: its corner lines, circle and ticks traced, with the white and its
    shadow taken out of the picture before it is enlarged so no step is left under them, then a new
    shadow from the new lines, and its three small icons."""
    picture = 'menu_common.png'
    w, h, pixels = decode_png(archive.read(archive.find(picture)))
    rects = layout_of(archive, picture)
    names = ('info_free_ele01.png', 'info_free_ele02.png', 'info_free_ele03.png')
    boxes = [rects[n] for n in names]
    # The old shadow falls within a few pixels down and right of a line; the dark panel behind the
    # words is shadow-coloured too, and stays.
    near_line = set()
    for bx, by, bw, bh in boxes:
        for j in range(by, by + bh):
            for i in range(bx, bx + bw):
                if card_line(pixels[j][i]):
                    near_line |= {(i + di, j + dj) for di in range(-1, CARD_SHADOW_REACH + 1) for dj in range(-1, CARD_SHADOW_REACH + 1)}
    # What is taken out is filled with what is around it, not left clear: the panel's own soft dark
    # is shadow-coloured too, and a clear hole in it showed lighter, as if the shadow were cut.
    taken_out = (1, 2, 3, 4)
    clean = [list(row) for row in pixels]
    for (i, j) in near_line:
        if 0 <= j < len(pixels) and 0 <= i < len(pixels[0]) and card_shadow(pixels[j][i]):
            clean[j][i] = taken_out
    for box in boxes:
        clean = filled_in(clean, box, lambda p: p == taken_out or card_line(p))
    c = enlarged(archive, picture, clean, w, h)
    lines = without_card_circles(pixels)
    for name, box in zip(names, boxes):
        layer = Canvas(box[2], box[3])
        traced_layer(layer, lines, box, card_line)
        for circle_name, right, bottom in CARD_CIRCLES:
            if circle_name == name:
                card_circle(layer, pixels, box, right, bottom)
        blend(c, drop_shadow(layer, CARD_SHADOW_ALPHA), box[0], box[1])
        blend(c, layer, box[0], box[1])
    white = lambda p: p[3] > 128 and lum(p) > 200
    card_star(c, pixels, rects['info_free_star.png'])
    card_icon(c, pixels, rects['info_free_kachi.png'], card_slate)
    card_icon(c, pixels, rects['info_free_oukan.png'], card_crown)
    write_sheet(c, pixels, out)


def card_star(sheet, pixels, box):
    """The song card's star: five straight points in the game's blue, outlined in white, as the
    picture has it but with no steps."""
    x, y, w, h = box
    inside = [pixels[y + j][x + i] for j in range(h) for i in range(w) if pixels[y + j][x + i][3] > 200 and lum(pixels[y + j][x + i]) <= 200]
    blue = max(set(inside), key=inside.count) if inside else (1, 120, 167, 255)
    def star(r_out, r_in):
        return [(w / 2 + (r_out if k % 2 == 0 else r_in) * math.cos(math.radians(-90 + k * 36)),
                 CARD_STAR_MIDDLE + (r_out if k % 2 == 0 else r_in) * math.sin(math.radians(-90 + k * 36))) for k in range(10)]
    c = Canvas(w, h)
    c.paint([star(CARD_STAR_R + CARD_STAR_EDGE * 1.7, (CARD_STAR_R + CARD_STAR_EDGE * 1.7) * CARD_STAR_WAIST)], (255, 255, 255))
    c.paint([star(CARD_STAR_R, CARD_STAR_R * CARD_STAR_WAIST)], blue[:3])
    wipe(sheet, box)
    paste(sheet, c, x, y)


def card_blue(pixels, box):
    x, y, w, h = box
    inside = [pixels[y + j][x + i] for j in range(h) for i in range(w) if pixels[y + j][x + i][3] > 200 and lum(pixels[y + j][x + i]) <= 200]
    return max(set(inside), key=inside.count)[:3] if inside else (1, 120, 167)


def card_icon(sheet, pixels, box, draw):
    """One of the song card's small icons, drawn by `draw` on a canvas of its size in its blue."""
    c = Canvas(box[2], box[3])
    draw(c, card_blue(pixels, box))
    wipe(sheet, box)
    paste(sheet, c, box[0], box[1])


def outlined_shapes(c, shapes, blue, edge=CARD_ICON_EDGE):
    """Shapes in blue with a white outline round all of them, its corners mitred."""
    c.paint(shapes + [thick(p, edge * 2, closed=True) for p in shapes], (255, 255, 255))
    c.paint(shapes, blue)


def card_slate(c, blue):
    """A clapperboard: its body with the striped band at its top, and the stick lifted from its left."""
    body = [(1, 4), (9, 4), (9, 9), (1, 9)]
    stick = [(2.4, 2.7), (8.0, 1.0), (8.3, 2.0), (2.7, 3.4)]
    outlined_shapes(c, [body, stick], blue)
    # The band's stripes, white slants across the blue, and the line under them.
    c.paint([[(1.6 + k * 2.3, 4), (2.7 + k * 2.3, 4), (2.0 + k * 2.3, 5.5), (0.9 + k * 2.3, 5.5)] for k in range(4)] +
            [[(1, 5.5), (9, 5.5), (9, 6.0), (1, 6.0)]], (255, 255, 255), clip=(1, 4, 9, 6))


def card_crown(c, blue):
    """A crown: a point in the middle and one each side, two stones on its band."""
    crown = [(5, 1.7), (6.0, 2.6), (6.4, 4.5), (8.7, 3.4), (8.6, 5.7), (7.4, 8.6), (2.6, 8.6), (1.4, 5.7), (1.3, 3.4), (3.6, 4.5),
             (4.0, 2.6)]
    outlined_shapes(c, [crown], blue)
    for cx in (3.6, 6.4):
        c.paint([[(cx + 0.65 * math.cos(math.radians(a)), 6.5 + 0.65 * math.sin(math.radians(a))) for a in range(0, 360, 30)]],
                (255, 255, 255))


def result_line(p):
    """The results screen's dark line art."""
    return p[3] > 200 and max(p[:3]) < 12


def result_icon(c, colour):
    """An octagon round a small square, in the results screen's title frame."""
    c.paint(pixel_strokes([[(5, 3), (10, 3), (12, 5), (12, 9), (10, 11), (5, 11), (3, 9), (3, 5), (5, 3)],
                           [(6, 5), (9, 5), (9, 9), (6, 9), (6, 5)]]), colour)


def result_header(archive, picture, pixels, c):
    """The top of the results screen: its lines traced over the picture enlarged without them, and
    its icon redrawn; the title's words, already in `c`, are kept."""
    w, h = len(pixels[0]), len(pixels)
    clean = filled_in(pixels, RESULT_HEADER, result_line)
    x, y, hw, hh = RESULT_ICON
    clean = filled_in(clean, (x, y, PLATE_ICON_W, PLATE_ICON_H), lambda p: p[3] > 200 and lum(p) > 200)
    fresh = enlarged(archive, picture, clean, w, h)
    centre_lines_over(fresh, pixels, RESULT_HEADER, result_line)
    plate_icon(fresh, pixels, x, y, result_icon)
    tx0, ty0, tx1, ty1 = (v * SCALE for v in RESULT_WORDS)
    for row in range(RESULT_HEADER[1] * SCALE, (RESULT_HEADER[1] + RESULT_HEADER[3]) * SCALE):
        for x0, x1 in (((RESULT_HEADER[0] * SCALE, tx0), (tx1, (RESULT_HEADER[0] + RESULT_HEADER[2]) * SCALE))
                       if ty0 <= row < ty1 else (((RESULT_HEADER[0] * SCALE, (RESULT_HEADER[0] + RESULT_HEADER[2]) * SCALE)),)):
            at = (row * c.w + x0) * 4
            c.data[at:at + (x1 - x0) * 4] = fresh.data[at:at + (x1 - x0) * 4]


def result_star(sheet, pixels, box):
    """A difficulty star: five straight points, a soft edge, the game's colour."""
    x, y, w, h = box
    middle = pixels[y + h // 2][x + w // 2]
    edge = max((pixels[y + j][x + i] for j in range(h) for i in range(w) if 0 < pixels[y + j][x + i][3] < 200),
               key=lambda p: p[3], default=middle)
    def star(r_out, r_in):
        return [(w / 2 + (r_out if k % 2 == 0 else r_in) * math.cos(math.radians(-90 + k * 36)),
                 RESULT_STAR_MIDDLE + (r_out if k % 2 == 0 else r_in) * math.sin(math.radians(-90 + k * 36))) for k in range(10)]
    c = Canvas(w, h)
    c.paint([star(RESULT_STAR_R + 0.6, RESULT_STAR_R * 0.42 + 0.3)], edge[:3], edge[3])
    c.paint([star(RESULT_STAR_R, RESULT_STAR_R * 0.42)], middle[:3], middle[3])
    wipe(sheet, box)
    paste(sheet, c, x, y)


# ---- the results window's words and the results digits
def thick(points, width, closed=False):
    """A stroke along the points with mitred corners and flat ends, as one polygon (a ring when closed)."""
    n = len(points)
    def normal(p, q):
        dx, dy = q[0] - p[0], q[1] - p[1]
        length = math.hypot(dx, dy) or 1
        return -dy / length, dx / length
    segs = [(points[k], points[(k + 1) % n]) for k in range(n if closed else n - 1)]
    normals = [normal(p, q) for p, q in segs]
    left, right = [], []
    for k in range(n):
        if closed:
            n1, n2 = normals[k - 1], normals[k % len(normals)]
        else:
            n1 = normals[max(0, k - 1)]
            n2 = normals[min(len(normals) - 1, k)]
        mx, my = n1[0] + n2[0], n1[1] + n2[1]
        size = 1 + n1[0] * n2[0] + n1[1] * n2[1]
        ox, oy = (mx / size, my / size) if size > 1e-6 else n1
        px, py = points[k]
        left.append((px + ox * width / 2, py + oy * width / 2))
        right.append((px - ox * width / 2, py - oy * width / 2))
    return with_hole(left, right[::-1]) if closed else left + right[::-1]


def glyph(ch, w, half=1.0):
    """A letter of the results window, as strokes on a box `w` wide and 8 high: (points, closed).
    `half` is half the stroke, so that the strokes' outsides meet the box."""
    l, r, t, m, b, c = half, w - half, half, 4, 8 - half, 1.5
    o = GLYPH_END_IN  # free ends at the box's side stop short of it, so neighbours never touch
    return {
        'A': [([(l, 8), (w / 2 - 0.9, t), (w / 2 + 0.9, t), (r, 8)], False), ([(l + 1.1, 5.4), (r - 1.1, 5.4)], False)],
        'B': [([(l, t), (r - c, t), (r, t + c), (r, m - 1), (r - 1, m), (l, m)], False),
              ([(l, m), (r - 1, m), (r, m + 1), (r, b - c), (r - c, b), (l, b)], False), ([(l, 0), (l, 8)], False)],
        'C': [([(w - o, t), (l + c, t), (l, t + c), (l, b - c), (l + c, b), (w - o, b)], False)],
        'D': [([(l, t), (r - c, t), (r, t + c), (r, b - c), (r - c, b), (l, b)], True)],
        'E': [([(w - o, t), (l, t), (l, b), (w - o, b)], False), ([(l, m), (w - 0.5 - o, m)], False)],
        'F': [([(w - o, t), (l, t), (l, 8)], False), ([(l, m), (w - 1, m)], False)],
        'H': [([(l, 0), (l, 8)], False), ([(r, 0), (r, 8)], False), ([(l, m), (r, m)], False)],
        'I': [([(l, 0), (l, 8)], False)],
        'L': [([(l, 0), (l, b - c), (l + c, b), (w - o, b)], False)],
        'M': [([(l, 8), (l, t + 1), (l + 1, t), (w / 2 - 1, t), (w / 2, t + 1), (w / 2, 8)], False),
              ([(w / 2, t + 1), (w / 2 + 1, t), (r - 1, t), (r, t + 1), (r, 8)], False)],
        'N': [([(l, 8), (l, 0)], False), ([(l, 0.6), (r, 7.4)], False), ([(r, 0), (r, 8)], False)],
        'O': [([(l + c, t), (r - c, t), (r, t + c), (r, b - c), (r - c, b), (l + c, b), (l, b - c), (l, t + c)], True)],
        'R': [([(l, 8), (l, t), (r - c, t), (r, t + c), (r, m - 1), (r - 1.2, m), (l + 1, m)], False),
              ([(l, m), (l + 1.5, m)], False), ([(r - 3, m), (r, 8)], False)],
        'S': [([(w - o, t), (l + c, t), (l, t + c), (l, m - 1), (l + 1, m), (r - 1, m), (r, m + 1), (r, b - c), (r - c, b), (o, b)], False)],
        'T': [([(o, t), (w - o, t)], False), ([(w / 2, t), (w / 2, 8)], False)],
        'W': [([(l, 0), (l, b - c), (l + c, b), (w / 2, b - 1.6), (r - c, b), (r, b - c), (r, 0)], False), ([(w / 2, 2.2), (w / 2, b - 1.6)], False)],
        ' ': [],
    }[ch]


GLYPH_W = {'A': 9, 'B': 8, 'C': 7, 'D': 8, 'E': 7, 'F': 7, 'H': 8, 'I': 2, 'L': 7, 'M': 10, 'N': 8, 'O': 8,
           'R': 8, 'S': 8, 'T': 7, 'W': 10, ' ': 3.5}
GLYPH_GAP = 1.0
GLYPH_END_IN = 0.5
# The words of the results window: where each stood, by its first and past its last column, its top and its height.
RESULT_WINDOW_WORDS = (
    ('COOL', 16, 51, 61, 8), ('FINE', 16, 43, 79, 8), ('SAFE', 16, 50, 97, 8), ('SAD', 16, 43, 115, 8),
    ('WORST', 16, 62, 133, 8), ('COMBO', 16, 62, 151, 8), ('CHANCE TIME', 16, 103, 169, 8), ('SCORE', 16, 75, 195, 11),
)


RESULT_COLON_COLUMNS = (118, 120)   # the columns of the colons' dots
RESULT_COLON_ROWS = (55, 185)      # the rows of the list they are in
RESULT_COLON_R = 1.2


def word_strokes(word, x0, x1, top, height):
    """The word's letters as polygons, spread to fill the columns it had."""
    total = sum(GLYPH_W[ch] for ch in word) + GLYPH_GAP * (len(word) - 1)
    sx, sy = (x1 - x0) / total, height / 8
    width = 2.0
    polys, at = [], 0.0
    for ch in word:
        for points, closed in glyph(ch, GLYPH_W[ch]):
            polys.append(thick([(x0 + (at + px) * sx, top + py * sy) for px, py in points], width, closed))
        at += GLYPH_W[ch] + GLYPH_GAP
    return polys, width


def letter_pixel(p):
    """A pixel of a results window word: bright or coloured, unlike the grey window."""
    return p[3] > 200 and (lum(p) > 95 or max(p[:3]) - min(p[:3]) > 60)


def result_window_words(archive, picture, pixels):
    """The results window with its words drawn again: the old letters and their dark edge taken out
    of the picture before it is enlarged, then each word in vector letters, coloured row by row as
    the game coloured it, inside a dark edge of its own colour."""
    w, h = len(pixels[0]), len(pixels)
    # The editor's window has no CHANCE TIME: a word is drawn only where the picture has it.
    words = [entry for entry in RESULT_WINDOW_WORDS
             if sum(1 for j in range(entry[3], entry[3] + entry[4]) for i in range(entry[1], entry[2]) if letter_pixel(pixels[j][i])) > RESULT_WORD_LEAST]
    taken = set()
    for word, x0, x1, top, height in words:
        for j in range(top - 1, top + height + 1):
            for i in range(x0 - 1, x1 + 1):
                if letter_pixel(pixels[j][i]):
                    taken |= {(i + di, j + dj) for di in (-1, 0, 1) for dj in (-1, 0, 1)}
    # The colons after the words: dots of two by two white pixels, drawn again as round dots.
    dots = []
    x0, x1 = RESULT_COLON_COLUMNS
    run = None
    for j in range(RESULT_COLON_ROWS[0], RESULT_COLON_ROWS[1] + 1):
        white = j < RESULT_COLON_ROWS[1] and any(pixels[j][i][3] > 200 and lum(pixels[j][i]) > 180 for i in range(x0, x1))
        if white and run is None:
            run = j
        elif not white and run is not None:
            dots.append((run, j))
            run = None
    for top, bottom in dots:
        taken |= {(i, j) for i in range(x0 - 1, x1 + 1) for j in range(top - 1, bottom + 1)}
    clean = texture_filled(pixels, (0, 0, w, h), taken)
    c = enlarged(archive, picture, clean, w, h)
    for word, x0, x1, top, height in words:
        rows = []
        for j in range(top, top + height):
            got = [pixels[j][i] for i in range(x0, x1) if letter_pixel(pixels[j][i])]
            rows.append(tuple(sum(p[k] for p in got) // len(got) for k in range(3)) if got else None)
        known = [k for k, v in enumerate(rows) if v]
        rows = [rows[k] or rows[min(known, key=lambda q: abs(q - k))] for k in range(len(rows))]
        def colour(px, py, rows=rows, top=top):
            f = min(len(rows) - 1.0, max(0.0, py - top - 0.5))
            a, b = rows[int(f)], rows[min(len(rows) - 1, int(f) + 1)]
            return tuple(int(a[k] + (b[k] - a[k]) * (f - int(f))) for k in range(3))
        width = RESULT_WORD_STROKE
        darkest = tuple(int(sum(v[k] for v in rows) / len(rows) * RESULT_WORD_EDGE) for k in range(3))
        bottom, e = top + height, RESULT_WORD_EDGE_W
        outer, inner = [], []
        paths = word_paths(word, x0, x1, top, height, width)
        for index, (points, closed) in enumerate(paths):
            others = [p for k, p in enumerate(paths) if k != index]
            if not closed:
                # Strokes that reach the word's top or bottom run on past it and are cut level there.
                points = list(points)
                for end, nextp in ((0, 1), (-1, -2)):
                    (px, py), (qx, qy) = points[end], points[nextp]
                    if py <= top + 0.01 or py >= bottom - 0.01:
                        length = math.hypot(px - qx, py - qy) or 1
                        points[end] = (px + (px - qx) / length * 2, py + (py - qy) / length * 2)
            outer.append(clipped_band(thick(points, width, closed), top, bottom))
            # A free end gets its edge too; an end that meets another stroke runs on into it, no edge between.
            inside = points if closed else ends_drawn(points, [joins(points[0], others, width), joins(points[-1], others, width)], e, width)
            inner.append(clipped_band(thick(inside, width - 2 * e, closed), top + e, bottom - e))
        # The dark edge is the letter's own outside: the colour is laid inside it, nothing reaches past.
        move = lambda polys: [[(px - x0 + 2, py - top + 2) for px, py in poly] for poly in polys if len(poly) >= 3]
        letters = Canvas(x1 - x0 + 4, height + 4)
        letters.paint(move(outer), darkest)
        letters.paint(move(inner), lambda px, py: colour(px + x0 - 2, py + top - 2))
        blend(c, letters, x0 - 2, top - 2)
    middle = sum(RESULT_COLON_COLUMNS) / 2
    c.paint([circle(middle, (top + bottom) / 2, RESULT_COLON_R, 24) for top, bottom in dots], WHITE)
    return c


def clipped_band(poly, y0, y1):
    """The polygon cut level at y0 and y1, keeping what lies between."""
    def cut(points, keep, y):
        out = []
        for k in range(len(points)):
            a, b = points[k - 1], points[k]
            if keep(b[1]):
                if not keep(a[1]):
                    out.append((a[0] + (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]), y))
                out.append(b)
            elif keep(a[1]):
                out.append((a[0] + (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]), y))
        return out
    return cut(cut(poly, lambda v: v >= y0, y0), lambda v: v <= y1, y1)


def joins(point, paths, width):
    """Whether a stroke's end lies on another of the strokes, `width` wide."""
    px, py = point
    for points, closed in paths:
        pairs = list(zip(points, points[1:] + (points[:1] if closed else [])))
        for (ax, ay), (bx, by) in pairs:
            dx, dy = bx - ax, by - ay
            t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / ((dx * dx + dy * dy) or 1)))
            if math.hypot(ax + dx * t - px, ay + dy * t - py) < width / 2 + 0.05:
                return True
    return False


def ends_drawn(points, joined, edge, width):
    """An open path with each free end drawn back by `edge` and each joined end left on the middle of the stroke it meets."""
    points = list(points)
    for (end, nextp), meets in zip(((0, 1), (-1, -2)), joined):
        (px, py), (qx, qy) = points[end], points[nextp]
        length = math.hypot(px - qx, py - qy) or 1
        by = 0.0 if meets else edge
        points[end] = (px + (qx - px) / length * by, py + (qy - py) / length * by)
    return points


def shortened(points, by):
    """An open path with each end drawn back by `by`."""
    points = list(points)
    for end, nextp in ((0, 1), (-1, -2)):
        (px, py), (qx, qy) = points[end], points[nextp]
        length = math.hypot(px - qx, py - qy) or 1
        points[end] = (px + (qx - px) / length * by, py + (qy - py) / length * by)
    return points


def word_paths(word, x0, x1, top, height, width=2.0):
    """The word's strokes as centre lines, laid so that strokes `width` wide fill its box."""
    total = sum(GLYPH_W[ch] for ch in word) + GLYPH_GAP * (len(word) - 1)
    sx, sy = (x1 - x0) / total, height / 8
    out, at = [], 0.0
    for ch in word:
        for points, closed in glyph(ch, GLYPH_W[ch], width / 2 / sy):
            out.append(([(x0 + (at + px) * sx, top + py * sy) for px, py in points], closed))
        at += GLYPH_W[ch] + GLYPH_GAP
    return out


def sheet_of_result_window(archive, picture, out):
    w, h, pixels = decode_png(archive.read(archive.find(picture)))
    write_sheet(result_window_words(archive, archive.find(picture), pixels), pixels, out)


# The results screen's digits, large and small: centre lines on the large ones' box, 11 by 12.
RESULT_DIGITS = {
    '1': [([(5.0, 1.0), (5.0, 11.6)], False)],
    '2': [([(1.0, 1.9), (8.3, 1.9), (9.0, 2.6), (9.0, 4.7), (8.1, 5.6), (2.7, 5.8), (1.9, 6.6), (1.9, 10.6), (10.0, 10.6)], False)],
    '3': [([(1.0, 1.9), (8.3, 1.9), (9.0, 2.6), (9.0, 9.9), (8.3, 10.6), (1.0, 10.6)], False), ([(1.5, 5.8), (9.0, 5.8)], False)],
    '4': [([(7.6, 1.9), (1.0, 8.2)], False), ([(0.0, 8.6), (11.0, 8.6)], False), ([(8.0, 1.0), (8.0, 11.6)], False)],
    '5': [([(9.0, 1.9), (1.0, 1.9), (1.0, 5.6), (8.2, 5.6), (9.0, 6.4), (9.0, 9.8), (8.2, 10.6), (0.0, 10.6)], False)],
    '6': [([(10.0, 1.9), (4.0, 1.9), (1.9, 4.0), (1.9, 9.2), (3.3, 10.6), (8.3, 10.6), (9.1, 9.8), (9.1, 6.4), (8.3, 5.6), (2.8, 5.6)], False), ([(1.0, 5.6), (3.5, 5.6)], False)],
    '7': [([(0.0, 1.9), (8.4, 1.9), (9.0, 2.6), (9.0, 3.6), (3.0, 11.6)], False)],
    '8': [([(2.7, 1.9), (8.3, 1.9), (9.0, 2.6), (9.0, 4.8), (8.3, 5.6), (2.7, 5.6), (2.0, 4.8), (2.0, 2.6)], True),
          ([(2.4, 5.6), (8.6, 5.6), (9.6, 6.6), (9.6, 9.7), (8.7, 10.6), (2.3, 10.6), (1.4, 9.7), (1.4, 6.6)], True)],
    '9': [([(7.5, 5.6), (9.9, 5.6)], False), ([(8.1, 5.6), (2.6, 5.6), (1.6, 4.6), (1.6, 2.8), (2.6, 1.9), (8.2, 1.9), (9.0, 2.7), (9.0, 8.4), (7.0, 10.6), (1.0, 10.6)], False)],
    '0': [([(2.8, 1.9), (8.2, 1.9), (9.4, 3.1), (9.4, 9.3), (8.2, 10.6), (2.8, 10.6), (1.4, 9.3), (1.4, 3.1)], True)],
}


def result_digit(sheet, box, strokes, small):
    """A digit of the results screen, white, its strokes straight and its corners cut."""
    x, y, w, h = box
    if small:
        place = lambda px, py: (px * 0.86 + 0.25, (py - 1.9) * 0.9 + 0.8)
        width = 1.65
    else:
        place = lambda px, py: (px, py)
        width = 1.8
    c = Canvas(w, h)
    c.paint([thick([place(px, py) for px, py in points], width, closed) for points, closed in strokes], WHITE)
    wipe(sheet, box)
    paste(sheet, c, x, y)


def line_chains(mask):
    """A set of one pixel wide lines as chains of pixels, end to end. Pixels touching at a corner
    are joined only where no pixel beside both already joins them."""
    def near(p):
        i, j = p
        out = [q for q in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1)) if q in mask]
        for di, dj in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
            q = (i + di, j + dj)
            if q in mask and (i + di, j) not in mask and (i, j + dj) not in mask:
                out.append(q)
        return out
    links = {p: near(p) for p in mask}
    seen_edges = set()
    chains = []
    starts = [p for p in mask if len(links[p]) != 2] + list(mask)
    for start in starts:
        for first in links[start]:
            if (start, first) in seen_edges:
                continue
            chain, here, last = [start, first], first, start
            seen_edges |= {(start, first), (first, start)}
            while len(links[here]) == 2:
                there = links[here][0] if links[here][0] != last else links[here][1]
                if (here, there) in seen_edges:
                    break
                seen_edges |= {(here, there), (there, here)}
                chain.append(there)
                last, here = here, there
            chains.append(chain)
    lone = [p for p in mask if not links[p]]
    return chains + [[p] for p in lone]


def chain_corners(chain):
    """The chain's pixels where it changes direction, with its two ends."""
    out = [chain[0]]
    for k in range(1, len(chain) - 1):
        a, b, c = chain[k - 1], chain[k], chain[k + 1]
        if (b[0] - a[0], b[1] - a[1]) != (c[0] - b[0], c[1] - b[1]):
            out.append(b)
    if len(chain) > 1:
        out.append(chain[-1])
    return out


def blob(component):
    """Whether a set of pixels has a two by two square in it: then it is a shape, not a line."""
    return any({(i + 1, j), (i, j + 1), (i + 1, j + 1)} <= component for i, j in component)


def components(mask):
    left, out = set(mask), []
    while left:
        stack = [left.pop()]
        part = set(stack)
        while stack:
            i, j = stack.pop()
            for q in ((i + a, j + b) for a in (-1, 0, 1) for b in (-1, 0, 1)):
                if q in left:
                    left.discard(q)
                    part.add(q)
                    stack.append(q)
        out.append(part)
    return out


def centre_lines_over(sheet, pixels, box, inside):
    """The box's one pixel lines that `inside` accepts, each drawn as a single stroke one pixel wide
    through its pixels' middles, so slants and straights are the same width and meet clean. Solid
    shapes among them, like filled arrow heads, are traced by their outline instead."""
    x, y, w, h = box
    mask = {(i, j) for j in range(h) for i in range(w) if inside(pixels[y + j][x + i])}
    if not mask:
        return
    near = nearest_colours(pixels, box, inside)
    colour = lambda px, py: near.get((min(w - 1, max(0, int(px))), min(h - 1, max(0, int(py)))), (0, 0, 0, 0))
    c = Canvas(w, h)
    solid = set()
    strokes = []
    for part in components(mask):
        if blob(part):
            solid |= part
            continue
        for chain in line_chains(part):
            corners = [(i + 0.5, j + 0.5) for i, j in chain_corners(chain)]
            if len(corners) == 1:
                cx, cy = corners[0]
                strokes.append(rect(cx - 0.5, cy - 0.5, 1, 1))
                continue
            # Each end reaches its pixel's edge, along the line.
            for end, nextp in ((0, 1), (-1, -2)):
                (px, py), (qx, qy) = corners[end], corners[nextp]
                length = math.hypot(px - qx, py - qy) or 1
                reach = 0.5 * max(abs(px - qx), abs(py - qy)) / length
                corners[end] = (px + (px - qx) / length * reach, py + (py - qy) / length * reach)
            strokes.append(thick(corners, LINE_WIDTH))
    if strokes:
        c.paint(strokes, colour)
    if solid:
        loops = [slanted(loop) for loop in pixel_loops(solid)]
        c.paint([as_one_polygon([loop for loop in loops if len(loop) >= 3])], colour)
    blend(sheet, c, x, y)


def texture_filled(pixels, box, taken):
    """The pixels at `taken` replaced from the same row further right, where the window is bare,
    so its patterned ground comes back as pattern, not smeared."""
    x, y, w, h = box
    out = [list(row) for row in pixels]
    width = len(pixels[0])
    for (i, j) in taken:
        for step in TEXTURE_FROM:
            a = i + step
            if 0 <= a < width and (a, j) not in taken and not letter_pixel(pixels[j][a]):
                out[j][i] = pixels[j][a]
                break
    return out


def write_sheet(c, pixels, folder):
    c.write(os.path.join(folder, '%08X.rgba' % colour_hash(pixels)))


def hint_width(name, start, text, drawer):
    """How wide a hint is with its words at their own widths, in the sheet's pixels."""
    signs, usual_start = signs_of(name, 'o')
    if not signs:
        return None
    start = start if start is not None else usual_start
    pen = sum(GGO_METRICS_ADVANCE(drawer, ch) for ch in text)
    return int(math.ceil(start + (pen + 4) / SCALE)) + 1


# The footer's hints are laid out again by the renderer, at their own widths: see pieces.txt in
# render_gl.c. A hint wider than its place on the sheet is drawn again whole in the rows the sheet
# leaves empty, under the bands; the narrowed one stays where it was, for screens that keep the
# game's layout.
SPARE_ROWS = (203, 216, 229, 242)   # below the footer line art and its shadow, which reach row 202
SPARE_GAP = 2
# Letters that hang below the line, which an 11 pixel hint cuts: such a hint goes to the spare rows
# too, with this much more room under it.
HANGING = set('gjpqy,;')
SPARE_BELOW = 2
HINT_ROWS = 11   # a hint's height on the sheet


# Hints the game never had, for what this edition adds, drawn in the spare rows too and listed in
# pieces.txt as "extra <number> <u> <v> <w> <h>": the renderer puts them in the footer when the game
# says so (psp_render_gl_footer_extras), or, with "extra_with <number> <u> <v>", whenever the game's
# hint at that place on the sheet is shown, from its first frame. In order: sorting with Select, the
# module with L and R; both come with the song list's PV hint, which no other screen has.
EXTRA_HINTS = ('basic_sele_sort', 'basic_lr_module', 'basic_start_practice')   # their words: pictures.hint.<last part>
EXTRA_WITH = {'basic_sele_sort': 'basic_tri_pvappr.png', 'basic_lr_module': 'basic_tri_pvappr.png',
              'basic_start_practice': 'basic_tri_pvappr.png'}


def sheet_of_hints(archive, labels, drawer, out, sign_set):
    picture = 'menu_basic02.png'
    w, h, pixels = decode_png(archive.read(archive.find(picture)))
    rects = layout_of(archive, picture)
    c = enlarged(archive, picture, pixels, w, h)
    tight = []
    pieces = ['sheet %08X' % colour_hash(pixels)]
    spare_row, spare_x = 0, 0
    for name, box in sorted(rects.items(), key=lambda kv: (kv[1][1], kv[1][0])):
        if name in labels:
            start, text = labels[name]
            wipe(c, box)
            squeeze = hint(c, name.split('.')[0], box, start, text, drawer, sign_set)
            if squeeze < 0.8:
                tight.append('%s %d%%' % (text, squeeze * 100))
            natural = hint_width(name.split('.')[0], start, text, drawer)
            if natural is None:
                continue
            x, y, bw, bh = box
            if natural <= bw and not HANGING & set(text):
                pieces.append('hint %d %d %d %d %d %d %d %d' % (x, y, bw, bh, x, y, natural, bh))
                continue
            if spare_x + natural > w:
                spare_row, spare_x = spare_row + 1, 0
            if spare_row >= len(SPARE_ROWS):
                print('no room in the spare rows for the hint %s' % text)
                continue
            whole = (spare_x, SPARE_ROWS[spare_row], natural, bh)
            hint(c, name.split('.')[0], whole, start, text, drawer, sign_set, SPARE_BELOW)
            pieces.append('hint %d %d %d %d %d %d %d %d' % (x, y, bw, bh, whole[0], whole[1], natural, bh + SPARE_BELOW))
            spare_x += natural + SPARE_GAP
    language = os.path.basename(os.path.normpath(out)).split('_')[0]
    words = ui_words(language)
    hints = [(name, words.get('pictures.hint.' + name.split('_', 2)[2])) for name in EXTRA_HINTS]
    for number, (name, text) in enumerate((name, text) for name, text in hints if text):
        start = signs_of(name, sign_set)[1]
        natural = hint_width(name, start, text, drawer)
        bh = HINT_ROWS
        if spare_x + natural > w:
            spare_row, spare_x = spare_row + 1, 0
        if spare_row >= len(SPARE_ROWS):
            print('no room in the spare rows for the hint %s' % text)
            continue
        whole = (spare_x, SPARE_ROWS[spare_row], natural, bh)
        hint(c, name, whole, start, text, drawer, sign_set, SPARE_BELOW)
        pieces.append('extra %d %d %d %d %d' % (number, whole[0], whole[1], natural, bh + SPARE_BELOW))
        if name in EXTRA_WITH:
            pieces.append('extra_with %d %d %d' % ((number,) + rects[EXTRA_WITH[name]][:2]))
        spare_x += natural + SPARE_GAP
    for name in ('basic_btm.png', 'basic_btm_ele01.png', 'basic_btm_ele02.png'):
        pieces.append('band %d %d %d %d' % rects[name])
    with open(os.path.join(out, 'pieces.txt'), 'w') as f:
        f.write('\n'.join(pieces) + '\n')
    btm, edit_btm = rects['basic_btm.png'], rects['edit_btm.png']
    wipe(c, btm)
    band(c, btm[1], 31, 46, btm[1] + 7, True)
    wipe(c, edit_btm)
    band(c, edit_btm[1], 45, 45, edit_btm[1] + 3, False)
    wipe(c, (32, 174, 480, 22))
    line_art(c, 32, 174)
    write_sheet(c, pixels, out)
    if tight and sign_set == 'o':
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
    # Its corners are round: outside a quarter circle at each, nothing.
    s, r = WINDOW_SIZE, WINDOW_ROUND
    corners = []
    for cx, cy, corner, start in ((r, r, (0, 0), 180), (s - r, r, (s, 0), 270), (s - r, s - r, (s, s), 0), (r, s - r, (0, s), 90)):
        corners.append([corner] + [(cx + r * math.cos(math.radians(start + 90 * k / 12)), cy + r * math.sin(math.radians(start + 90 * k / 12)))
                                   for k in range(13)])
    punch(c, corners)
    paste(sheet, c, WINDOW_X, WINDOW_Y)


def centred_on_button(drawer, text, button_width):
    """How far along a button its sign and word start, so that together they are in its middle."""
    word = sum(GGO_METRICS_ADVANCE(drawer, ch) for ch in text) / SCALE
    return max(0, int(button_width - BUTTON_WORD_AT - word) // 2)


def button_sign(c, x, y, kind):
    """A sign with a white edge all the way round, twelve pixels across, and the colon after it."""
    if kind in ('A', 'B'):
        c.paint([circle(x + 6, y + 6.5, 6)], WHITE)
        c.paint([circle(x + 6, y + 6.5, 5)], BUTTON_TEAL)
        c.paint(letter_strokes(kind, x + 6, y + 6.5, 6, 1.45), WHITE)
    elif kind == 'circle':
        c.paint(ring(x + 6, y + 6.5, 6, 2), WHITE)
        c.paint(ring(x + 6, y + 6.5, 5, 3), BUTTON_TEAL)
    else:
        arms = ((x + 1.6, y + 2.1, x + 10.4, y + 10.9), (x + 10.4, y + 2.1, x + 1.6, y + 10.9))
        # Longer than the teal, so that the white goes round the ends as well.
        white = ((x + 0.9, y + 1.4, x + 11.1, y + 11.6), (x + 11.1, y + 1.4, x + 0.9, y + 11.6))
        c.paint([segment(*arm, 4.4) for arm in white], WHITE)
        c.paint([segment(*arm, 2.4) for arm in arms], BUTTON_TEAL)
    c.paint([rect(x + 15, y + 4, 1.4, 2), rect(x + 15, y + 8, 1.4, 2)], WHITE)


def sheet_of_windows(archive, labels, drawer, out, sign_set):
    picture = 'menu_basic.png'
    w, h, pixels = decode_png(archive.read(archive.find(picture)))
    rects = layout_of(archive, picture)
    c = enlarged(archive, picture, pixels, w, h)
    wipe(c, (WINDOW_X, WINDOW_Y, WINDOW_SIZE, WINDOW_SIZE))
    window(c)
    wipe(c, (BLADE_X, BLADE_Y, BLADE_W, BLADE_H))
    blade(c, pixels)
    icon_ring(c, pixels, rects['basic_blade_ico.png'])
    slider_end(c, pixels, rects['basic_slider_up.png'], True)
    slider_end(c, pixels, rects['basic_slider_dn.png'], False)
    # The rows' bars, and the same arrows in the bar of a row with a value, when it is not the one chosen.
    bx, by, _, _ = rects['basic_planeon.png']
    streaked(c, pixels, rects['basic_planeoff.png'])
    streaked(c, pixels, rects['basic_planeon.png'], [(left, by + 4, right, by + 6 + BAR_ARROW_H) for left, right, _ in BAR_ARROWS])
    for left, right, direction in BAR_ARROWS:
        arrow_in_bar(c, pixels, (left, by + 5, right, by + 5 + BAR_ARROW_H), direction)
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
        button_sign(c, x + moved, y, SIGN_SETS[sign_set][0 if confirm else 1])
        write_text(c, drawer, labels[name][1], (x + moved + BUTTON_WORD_AT, y, bw - moved - BUTTON_WORD_AT, bh), WORD, False, WORD_EDGE)
    for name, word in (('basic_flwin_resume.png', 'RESUME'), ('basic_flwin_retry.png', 'RETRY'), ('basic_flwin_return.png', 'RETURN')):
        wipe(c, rects[name])
        write_text(c, drawer, word, rects[name], WHITE, True, WORD_EDGE)
    write_sheet(c, pixels, out)


# ---- the editor's small icons: the camera's ways of moving, its time and its preview
EDIT_ICON_EDGE = (11, 66, 166)
EDIT_ICON_FILL = (148, 254, 244)


def ellipse_points(cx, cy, rx, ry, from_deg, to_deg, steps=16):
    return [(cx + rx * math.cos(math.radians(from_deg + (to_deg - from_deg) * k / steps)),
             cy + ry * math.sin(math.radians(from_deg + (to_deg - from_deg) * k / steps))) for k in range(steps + 1)]


def edged_icon(sheet, box, fills=(), lines=(), dark=(), dark_lines=(), clear=True):
    """An icon in the editor's two colours: cyan shapes and strokes inside a dark blue edge one
    pixel wide all round, and marks of the dark blue drawn over them."""
    x, y, w, h = box
    c = Canvas(w, h)
    edge = [part for poly in fills for part in [poly] + polyline(poly + poly[:1], 2.0)]
    edge += [part for points, width in lines for part in polyline(points, width + 2.0)]
    c.paint(edge + list(dark), EDIT_ICON_EDGE)
    inside = list(fills) + [part for points, width in lines for part in polyline(points, width)]
    c.paint(inside, EDIT_ICON_FILL)
    marks = list(dark) + [part for points, width in dark_lines for part in polyline(points, width)]
    if marks:
        c.paint(marks, EDIT_ICON_EDGE)
    if clear:
        wipe(sheet, box)
    paste(sheet, c, x, y)


def edit_icons(sheet, rects):
    """Each traced from the game's twelve pixels as the shapes they stand for."""
    edged_icon(sheet, rects['cameramd_icon_parallel.png'],
               fills=[[(6, 1.0), (7.9, 3.1), (4.1, 3.1)], [(6, 11.0), (4.1, 8.9), (7.9, 8.9)],
                      [(1.0, 6), (3.1, 7.9), (3.1, 4.1)], [(11.0, 6), (8.9, 4.1), (8.9, 7.9)]],
               lines=[([(6, 2.8), (6, 9.2)], 1.8), ([(2.8, 6), (9.2, 6)], 1.8)])
    edged_icon(sheet, rects['cameramd_icon_rotate.png'],
               fills=[[(1.0, 2.6), (4.6, 2.6), (2.5, 5.2)], [(7.4, 2.6), (11.0, 2.6), (9.5, 5.2)]],
               lines=[(ellipse_points(6, 4.8, 4.0, 3.4, 0, 180), 1.3)],
               dark_lines=[([(6, 2.6), (6, 6.4)], 2.0)])
    edged_icon(sheet, rects['cameramd_icon_preview.png'], fills=[[(1.4, 1.2), (10.8, 6), (1.4, 10.8)]])
    edged_icon(sheet, rects['cameramd_icon_time.png'], fills=[circle(6, 6, 4.9, 32)],
               dark_lines=[([(6, 6.6), (6, 1.8)], 1.6), ([(6, 6.6), (9.0, 6.6)], 1.2)])
    camera = rects['cameramd_icon_camera.png']
    body = rounded(1, 3, 10, 8, 1.2)
    edged_icon(sheet, camera, fills=[body, rect(9, 1, 2, 1)],
               dark=[circle(6, 7, 2.1, 20)],
               dark_lines=[(ellipse_points(6, 7, 3.5, 3.1, 140, 220, 8), 1.3), (ellipse_points(6, 7, 3.5, 3.1, -40, 40, 8), 1.3)])


def edit_dark(p):
    """The dark blue of the editor's icons, which neither kind of button has elsewhere inside."""
    return p[2] > 130 and p[0] < 70 and p[1] < 130


def grid_icon_shapes(name):
    """The grid's icons, in the game's pixels from the corner of each one's dark edge: cyan lines
    one pixel wide (or shapes) that edged_icon puts inside an edge of the dark blue."""
    head = 1.3
    shapes = {
        '1': dict(lines=[([(0.8, 8.5), (27.5, 8.5)], 1.0), ([(8.8, 0.8), (0.8, 8.5), (8.8, 16.2)], head)]),
        '2': dict(lines=[([(5.5, 0.8), (5.5, 18.6)], 1.0), ([(1.2, 13.8), (5.5, 18.6), (9.8, 13.8)], head)]),
        '3': dict(lines=[([(0.5, 5.5), (22.2, 5.5)], 1.0), ([(17.6, 1.0), (22.2, 5.5), (17.6, 10.0)], head)]),
        '4': dict(lines=[([(7.5, 0.8), (7.5, 9.5), (1.6, 16.4)], 1.0), ([(1.6, 11.4), (1.6, 16.4), (6.6, 16.4)], head)]),
        '5': dict(lines=[([(1.0, 1.0), (18.4, 18.4)], 1.0), ([(18.4, 12.6), (18.4, 18.4), (12.6, 18.4)], head)]),
        '6': dict(lines=[([(1.0, 7.5), (13.4, 18.4)], 1.0), ([(13.4, 12.6), (13.4, 18.4), (8.6, 18.4)], head),
                         ([(20.6, 13.4), (9.6, 1.6)], 1.0), ([(9.6, 6.4), (9.6, 1.6), (14.6, 1.6)], head)]),
        '7': dict(lines=[([(0.6, 1.5), (8.5, 1.5), (21.4, 16.4)], 1.0), ([(21.4, 10.6), (21.4, 16.4), (16.6, 16.4)], head)]),
        '8': dict(lines=[([(0.8, 4.5), (16.4, 4.5)], 1.0), ([(4.6, 1.0), (0.8, 4.5), (4.6, 8.0)], head),
                         ([(6.6, 13.5), (22.2, 13.5)], 1.0), ([(18.4, 10.0), (22.2, 13.5), (18.4, 17.0)], head)]),
        'count': dict(lines=[(ellipse_points(8.6, 9.5, 7.5, 8.0, 0, 360, 40), 1.3), (ellipse_points(8.6, 9.5, 3.6, 3.8, 40, 320, 20), 1.3)]),
        'ie': dict(fills=[[(8.0, 0.8), (15.2, 9.2), (0.8, 9.2)], rect(3.2, 11.0, 9.6, 5.4)], dark=[rect(6.0, 11.0, 4.0, 3.6)]),
        'oto': dict(lines=[([(5.5, 0.8), (5.5, 12.6)], 1.0), ([(5.6, 1.2), (7.6, 3.2), (9.4, 5.6), (9.4, 7.8), (8.4, 9.6)], 1.0)],
                    fills=[ellipse(2.9, 14.0, 2.6, 1.9)]),
    }
    return shapes[name]


def icon_box_off(pixels, box):
    """Where an icon's dark edge is on a button that is not lit, and which of its pixels it covers."""
    x, y, w, h = box
    pts = [(i, j) for j in range(y + 2, y + h - 2) for i in range(x + 2, x + w - 2) if edit_dark(pixels[j][i])]
    x0, y0 = min(i for i, _ in pts), min(j for _, j in pts)
    x1, y1 = max(i for i, _ in pts) + 1, max(j for _, j in pts) + 1
    return (x0, y0, x1 - x0, y1 - y0), {(i - x0, j - y0) for i, j in pts}


def icon_box_lit(pixels, box, size, mask):
    """The same icon on the lit button, found where its dark pixels match best."""
    x, y, w, h = box
    iw, ih = size
    best = None
    for oy in range(y + 3, y + h - ih - 2):
        for ox in range(x + 3, x + w - iw - 2):
            score = sum(1 for i, j in mask if edit_dark(pixels[oy + j][ox + i]))
            if best is None or score > best[0]:
                best = (score, ox, oy)
    return best[1], best[2], iw, ih


def under_icon(sheet, box):
    """The button under an icon, run on from each side of it, row by row."""
    x, y, w, h = box
    m = 1
    x0, x1 = (x - m) * SCALE, (x + w + m) * SCALE
    for row in range((y - m) * SCALE, (y + h + m) * SCALE):
        a = sheet.data[(row * sheet.w + x0 - 1) * 4:(row * sheet.w + x0) * 4]
        b = sheet.data[(row * sheet.w + x1) * 4:(row * sheet.w + x1 + 1) * 4]
        for col in range(x0, x1):
            t = (col - x0 + 0.5) / (x1 - x0)
            sheet.data[(row * sheet.w + col) * 4:(row * sheet.w + col + 1) * 4] = bytes(int(a[k] + (b[k] - a[k]) * t) for k in range(4))


def grid_icons(sheet, pixels, rects):
    """The icons on the editor grid's buttons, unlit and lit, drawn over the button run on under them."""
    for name in ('1', '2', '3', '4', '5', '6', '7', '8', 'count', 'ie', 'oto'):
        off, mask = icon_box_off(pixels, rects['editmd_btn_%s_act.png' % name])
        lit = icon_box_lit(pixels, rects['editmd_btn_%s_a_act.png' % name], off[2:], mask)
        for box in (off, lit):
            under_icon(sheet, box)
            shapes = grid_icon_shapes(name)
            x, y = box[0], box[1]
            c = Canvas(box[2] + 2, box[3] + 2)
            moved = lambda pts: [(px + 1, py + 1) for px, py in pts]
            edged_icon(c, (0, 0, c.w // SCALE, c.h // SCALE),
                       fills=[moved(p) for p in shapes.get('fills', ())],
                       lines=[(moved(pts), width) for pts, width in shapes.get('lines', ())],
                       dark=[moved(p) for p in shapes.get('dark', ())], clear=False)
            blend(sheet, c, x - 1, y - 1)


def made_or_enlarged(archive, picture, pixels, out):
    """The picture as the build has made it so far in this language, or the game's own enlarged."""
    path = os.path.join(out, '%08X.rgba' % colour_hash(pixels))
    if not os.path.exists(path):
        w, h = len(pixels[0]), len(pixels)
        return enlarged(archive, picture, pixels, w, h)
    with open(path, 'rb') as f:
        made_w, made_h = struct.unpack('<II', f.read(8))
        data = f.read()
    c = Canvas(made_w // SCALE, made_h // SCALE)
    for row in range(made_h):
        c.data[(made_h - 1 - row) * made_w * 4:(made_h - row) * made_w * 4] = data[row * made_w * 4:(row + 1) * made_w * 4]
    return c


# ---- the shapes on the rhythm screen's beat lines: white, a coloured middle, and a soft glow
def glowing_symbol(sheet, box, white, middle_edge, middle, edge_colour, fill_colour, glow):
    """A beat line's shape: its glow in the colour of its middle, spread from the white shape,
    then the white, then its middle with a darker edge one pixel wide."""
    x, y, w, h = box
    c = Canvas(w, h)
    mask = Canvas(w, h)
    mask.paint(white, WHITE)
    for i, cover in enumerate(blurred(mask, SYMBOL_GLOW_REACH)):
        alpha = int(min(1.0, cover / 255 * 1.7) * SYMBOL_GLOW_ALPHA)
        if alpha:
            c.data[i * 4:i * 4 + 4] = bytes(glow[:3]) + bytes([alpha])
    c.paint(white, WHITE)
    c.paint(middle_edge, edge_colour)
    c.paint(middle, fill_colour)
    wipe(sheet, box)
    paste(sheet, c, x, y)


def rhythm_symbols(sheet, rects):
    """Measured from the game's: each centred at (22, 20) of its piece."""
    def bars(arm, width):
        return [segment(22 - arm, 20 - arm, 22 + arm, 20 + arm, width), segment(22 + arm, 20 - arm, 22 - arm, 20 + arm, width)]
    glowing_symbol(sheet, rects['edit_icon_sankaku.png'], [[(22, 4.2), (38.4, 34), (5.6, 34)]],
                   [[(22, 15.4), (29.4, 29), (14.6, 29)]], [[(22, 17.4), (27.6, 28), (16.4, 28)]], (4, 161, 129), (1, 193, 152), (2, 205, 167))
    glowing_symbol(sheet, rects['edit_icon_maru.png'], [circle(22, 20, 15, 72)],
                   [circle(22, 20, 10, 64)], [circle(22, 20, 9, 64)], (195, 39, 118), (252, 40, 146), (253, 43, 151))
    glowing_symbol(sheet, rects['deit_icon_batsu.png'], bars(13.5, 7.0), bars(7.6, 3.6), bars(7.0, 2.0), (55, 146, 204), (60, 162, 228),
                   (68, 180, 247))
    glowing_symbol(sheet, rects['edit_icon_shikaku.png'], [rect(8, 6, 28, 28)], [rect(14, 12, 16, 16)], [rect(15, 13, 14, 14)],
                   (189, 75, 156), (255, 101, 214), (255, 114, 219))


def sheet_of_rhythm(archive, out):
    picture = 'edit_rhythm.png'
    w, h, pixels = decode_png(archive.read(archive.find(picture)))
    c = made_or_enlarged(archive, picture, pixels, out)
    rhythm_symbols(c, layout_of(archive, picture))
    write_sheet(c, pixels, out)


def sheet_of_edit_buttons(archive, out):
    picture = 'edit_button.png'
    w, h, pixels = decode_png(archive.read(archive.find(picture)))
    c = made_or_enlarged(archive, picture, pixels, out)
    rects = layout_of(archive, picture)
    edit_icons(c, rects)
    grid_icons(c, pixels, rects)
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


# The words of the results: the rank, twice in other colours to flash between, and the difficulty.
# Each letter stands in its own columns, so it is drawn again in them, in the game's squared
# letters, and coloured as the game coloured it there. (picture's name, text, stroke)
RESULT_RANK_WORDS = (
    ('result_perfect01.png', 'PERFECT', 3.8), ('result_perfect02.png', 'PERFECT', 3.8),
    ('result_great01.png', 'GREAT', 3.8), ('result_great02.png', 'GREAT', 3.8),
    ('result_cheap01.png', 'CHEAP', 3.8), ('result_cheap02.png', 'CHEAP', 3.8),
    ('result_miss01.png', 'MISSxTAKE', 3.8), ('result_miss02.png', 'MISSxTAKE', 3.8),
    ('result_normal01.png', 'STANDARD', 3.8), ('result_normal02.png', 'STANDARD', 3.8),
    ('result_level_easy.png', 'EASY', 2.0), ('result_level_normal.png', 'NORMAL', 2.0), ('result_level_hard.png', 'HARD', 2.0),
)
INK = 0x60


def letter_boxes(pixels, box):
    """Each run of columns with something drawn in it, and the rows that run has drawn, in the picture."""
    x0, y0, w, h = box
    runs, start = [], None
    for i in range(w + 1):
        inked = i < w and any(pixels[y][x0 + i][3] > INK for y in range(y0, y0 + h))
        if inked and start is None:
            start = i
        elif not inked and start is not None:
            rows = [y for y in range(y0, y0 + h) if any(pixels[y][x][3] > INK for x in range(x0 + start, x0 + i))]
            runs.append((x0 + start, rows[0], x0 + i, rows[-1] + 1))
            start = None
    return runs


def colour_field(pixels, box):
    """The game's colours in a box, taken from inside its letters only (not their shaded edges),
    carried out to the empty pixels nearest them and softened, as fn(x, y)."""
    x0, y0, w, h = box
    def solid(x, y):
        return all(0 <= x + dx - x0 < w and 0 <= y + dy - y0 < h and pixels[y + dy][x + dx][3] > 200
                   for dx, dy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)))
    field = [[None] * w for _ in range(h)]
    front = []
    for j in range(h):
        for i in range(w):
            if solid(x0 + i, y0 + j):
                field[j][i] = tuple(pixels[y0 + j][x0 + i][:3])
                front.append((i, j))
    if not front:
        for j in range(h):
            for i in range(w):
                if pixels[y0 + j][x0 + i][3] > INK:
                    field[j][i] = tuple(pixels[y0 + j][x0 + i][:3])
                    front.append((i, j))
    while front:
        nxt = []
        for i, j in front:
            for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                a, b = i + di, j + dj
                if 0 <= a < w and 0 <= b < h and field[b][a] is None:
                    field[b][a] = field[j][i]
                    nxt.append((a, b))
        front = nxt
    soft = [[tuple(sum(field[b][a][k] for a in range(max(0, i - 1), min(w, i + 2)) for b in range(max(0, j - 1), min(h, j + 2))) //
                   (len(range(max(0, i - 1), min(w, i + 2))) * len(range(max(0, j - 1), min(h, j + 2)))) for k in range(3))
             for i in range(w)] for j in range(h)]

    def fn(x, y):
        fx = min(w - 1.0, max(0.0, x - x0 - 0.5))
        fy = min(h - 1.0, max(0.0, y - y0 - 0.5))
        i, j = int(fx), int(fy)
        i1, j1 = min(w - 1, i + 1), min(h - 1, j + 1)
        tx, ty = fx - i, fy - j
        return tuple(int((soft[j][i][k] * (1 - tx) + soft[j][i1][k] * tx) * (1 - ty) + (soft[j1][i][k] * (1 - tx) + soft[j1][i1][k] * tx) * ty)
                     for k in range(3))
    return fn


def squared_letters(text, boxes, stroke):
    """The squared letters, each fitted to its own columns, and to the rows most of the word's
    letters share when its own are within two of them: a round letter of the picture's reaches a
    row past the others. A letter small on purpose, as the x of MISSxTAKE, keeps its own."""
    tops, bottoms = [b[1] for b in boxes], [b[3] for b in boxes]
    top_row = max(set(tops), key=tops.count)
    bottom_row = max(set(bottoms), key=bottoms.count)
    boxes = [(x0, top_row, x1, bottom_row) if abs(y0 - top_row) <= 2 and abs(y1 - bottom_row) <= 2 else (x0, y0, x1, y1)
             for x0, y0, x1, y1 in boxes]
    polys = []
    for ch, (x0, y0, x1, y1) in zip(text, boxes):
        left, top = x0 + stroke / 2, y0 + stroke / 2
        ux, uy = (x1 - x0 - stroke) / 6, (y1 - y0 - stroke) / 8
        for line in GLYPHS[ch]:
            polys += polyline([(left + px * ux, top + py * uy) for px, py in line], stroke)
    return polys


def result_words(c, pixels, rects):
    for name, text, stroke in RESULT_RANK_WORDS:
        box = rects[name]
        boxes = letter_boxes(pixels, box)
        if len(boxes) != len(text):
            print('results: %s has %d letters, not %d; left as it is' % (name, len(boxes), len(text)))
            continue
        colour = colour_field(pixels, box)
        wipe(c, box)
        c.paint(squared_letters(text, boxes, stroke), colour)


def result_hints(archive, labels, drawer, out, made_in, sign_set):
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
    result_header(archive, picture, pixels, c)
    rects = layout_of(archive, picture)
    for star in ('result_star_act.png', 'result_star_white.png'):
        result_star(c, pixels, rects[star])
    for digit, strokes in RESULT_DIGITS.items():
        result_digit(c, rects['result_num_%s.png' % digit], strokes, False)
        result_digit(c, rects['result_numsm_%s.png' % digit], strokes, True)
    result_words(c, pixels, rects)
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
    confirm = SIGN_SETS[sign_set][0]
    if confirm in ('A', 'B'):
        piece.paint([circle(at + 5, 6.5, 5)], WHITE)
        punch(piece, letter_strokes(confirm, at + 5, 6.5, 6, 1.35))
    elif confirm == 'cross':
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


# ---- the card before a song: its words, numbers, stars and buttons, in the game's squared letters
CARD_SIGN = 24
CARD_TEAL = (51, 221, 187)
CARD_EDGE = (24, 26, 38)
CARD_DISC = (38, 41, 56)
CARD_RING = (200, 204, 216)
CARD_MARK = (240, 240, 244)


def card_button(c, cx, cy, kind):
    """A flat dark disc with a thin light edge, and its sign in light strokes."""
    r = 9.6
    c.paint([circle(cx, cy, r)], CARD_DISC)
    c.paint(ring(cx, cy, r, r - 1.1), CARD_RING)
    if kind == 'circle':
        c.paint(ring(cx, cy, 4.6, 2.9), CARD_MARK)
    elif kind == 'cross':
        c.paint(polyline([(cx - 3.6, cy - 3.6), (cx + 3.6, cy + 3.6)], 1.9) + polyline([(cx + 3.6, cy - 3.6), (cx - 3.6, cy + 3.6)], 1.9), CARD_MARK)
    else:
        c.paint(letter_strokes(kind, cx, cy, 8.4, 1.9), CARD_MARK)


def card_signs(sign_set, out):
    """The card's two buttons with letters, the one by Start above the one by Back. The renderer
    draws them in place of its Circle and Cross."""
    c = Canvas(CARD_SIGN, CARD_SIGN * 2)
    for slot, letter in enumerate(SIGN_SETS[sign_set][:2]):
        card_button(c, 11.5, CARD_SIGN * slot + 12, letter)
    c.write(os.path.join(out, 'card_signs.rgba'))


def square_word(text, x, y, ux, h, stroke):
    """The game's squared letters, `ux` wide for each unit of their grid, `h` high."""
    units = len(text) * 6 + (len(text) - 1) * 1.6
    return text_strokes(text, x, y, units * ux + stroke, h, stroke)


def square_width(text, ux, stroke):
    return (len(text) * 6 + (len(text) - 1) * 1.6) * ux + stroke


def star(cx, cy, r, inner=0.48):
    return [(cx + (r if i % 2 == 0 else r * inner) * math.sin(i * math.pi / 5),
             cy - (r if i % 2 == 0 else r * inner) * math.cos(i * math.pi / 5)) for i in range(10)]


def card_piece(name, w, h):
    """One of the card's pieces drawn anew, or None if it is not one of those."""
    c = Canvas(w, h)
    if name.startswith('difficulty_'):
        c.paint(square_word(name[11:-4].upper(), 7, 6, 1.745, 11, 2.2), CARD_TEAL)
    elif name.startswith('level_number'):
        c.paint(text_strokes(name[12:14][-1], 2, 3, 12, 11, 2.2), CARD_TEAL)
    elif name in ('start.png', 'return.png'):
        word = name[:-4].upper()
        c.paint(square_word(word, 3.0, 2.8, 1.45, 10, 1.9 + 1.6), CARD_EDGE)
        c.paint(square_word(word, 3.8, 3.6, 1.45, 8.4, 1.9), WHITE)
    elif name.startswith('star_'):
        colour = WHITE if name == 'star_empty.png' else CARD_TEAL
        if name == 'star_flash01.png':
            for grow, alpha in ((1.55, 40), (1.3, 70), (1.12, 110)):
                c.paint([star(12.5, 12.4, 7.3 * grow, 0.52)], colour, alpha)
        c.paint([star(12.5, 12.4, 7.3)], colour)
    elif name == 'button_o.png':
        card_button(c, 11.5, 12, 'circle')
    elif name == 'button_x.png':
        card_button(c, 11.5, 12, 'cross')
    else:
        return None
    return c


def card_toggle(word, w, h, left):
    """The part of the button sound's row after its name: arrows either side of ON or OFF."""
    c = Canvas(w, h)
    c.paint([[(58 - left, 8.5), (65 - left, 4.5), (65 - left, 12.5)], [(113 - left, 8.5), (106 - left, 4.5), (106 - left, 12.5)]], CARD_TEAL)
    c.paint(square_word(word, 85.5 - left - square_width(word, 1.3, 2.2) / 2, 3, 1.3, 11, 2.2), CARD_TEAL)
    return c


def paste_halved(data, fw, fh, piece, x, y):
    """A piece drawn at four times put in a sheet kept at twice the game's size, in place of what
    was there. Rows of the sheet are bottom first."""
    k = SCALE // 2
    for row in range(piece.h // k):
        at = ((fh - 1 - (y * 2 + row)) * fw + x * 2) * 4
        for col in range(piece.w // k):
            r = g = b = a = 0
            for dy in range(k):
                for dx in range(k):
                    i = ((row * k + dy) * piece.w + col * k + dx) * 4
                    pa = piece.data[i + 3]
                    r += piece.data[i] * pa
                    g += piece.data[i + 1] * pa
                    b += piece.data[i + 2] * pa
                    a += pa
            o = at + col * 4
            if a:
                data[o:o + 4] = bytes((r // a, g // a, b // a, a // (k * k)))
            else:
                data[o:o + 4] = b'\0\0\0\0'


def sheets_of_cards(archive, out):
    """Each card's sheet, as lang_images.py made it, with its pieces drawn anew."""
    toggles = {'se_on.png': 'ON', 'se_off.png': 'OFF'}
    name_ends = 50   # where the button sound's name ends, as the words for it are placed
    for entry in sorted({e[0] for e in archive.entries if e[0].startswith('pv_start_') and '.p' in e[0]}):
        w, h, pixels = decode_png(archive.read(entry))
        path = os.path.join(out, '%08X.rgba' % colour_hash(pixels))
        if not os.path.exists(path):
            continue
        with open(path, 'rb') as f:
            fw, fh = struct.unpack('<II', f.read(8))
            data = bytearray(f.read())
        if fw != w * 2 or fh != h * 2:
            continue
        for name, (x, y, pw, ph) in layout_of(archive, entry).items():
            if name in toggles:
                paste_halved(data, fw, fh, card_toggle(toggles[name], pw - name_ends, ph, name_ends), x + name_ends, y)
                continue
            piece = card_piece(name, pw, ph)
            if piece:
                paste_halved(data, fw, fh, piece, x, y)
        with open(path, 'wb') as f:
            f.write(struct.pack('<II', fw, fh))
            f.write(data)


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
    for ending, _ in SET_FOLDERS:
        os.makedirs(out + ending, exist_ok=True)
    drawer = Drawer(family, HINT_H * SCALE)
    for picture in CLEAN_ONLY:
        sheet_clean(archive, picture, out)
    result_labels = labels.get('menu_result.png', {})
    for ending, sign_set in SET_FOLDERS:
        folder = out + ending
        sheet_of_hints(archive, labels.get('menu_basic02.png', {}), drawer, folder, sign_set)
        sheet_of_windows(archive, labels.get('menu_basic.png', {}), drawer, folder, sign_set)
        result_hints(archive, result_labels, drawer, folder, out, sign_set)
        if SIGN_SETS[sign_set][0] in ('A', 'B'):
            card_signs(sign_set, folder)
    titles(archive, out)
    language = os.path.basename(os.path.normpath(out))
    words = ui_words(language)
    if all(k in words for k in TITLE_WORDS):
        title_sheet(archive, drawer, out, tuple(words[k] for k in TITLE_WORDS))
        # Japanese has no pictures of its own here, and its title has these words in English.
        demo_start_sheet(archive, family, out, words[DEMO_WORD])
        if language == 'en':
            everyone = os.path.dirname(os.path.dirname(os.path.normpath(out)))
            title_sheet(archive, drawer, everyone, tuple(words[k] for k in TITLE_WORDS))
            demo_start_sheet(archive, family, everyone, words[DEMO_WORD])
    sheet_of_arrows(archive, out)
    sheet_of_icons(archive, family, out)
    sheet_of_card(archive, out)
    sheet_of_edit_buttons(archive, out)
    sheet_of_rhythm(archive, out)
    for picture in ('result_win_nm.png', 'result_win_edit.pn'):
        sheet_of_result_window(archive, picture, out)
    write_card_swaps(archive, out)
    sheets_of_cards(archive, out)
    print('%s: done' % out)


if __name__ == '__main__':
    main()
