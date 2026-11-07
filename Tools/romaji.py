"""Make the romaji the game can show, on this computer, from this computer's copy of the game.

    python Tools/romaji.py <executable> lang/romaji_titles.txt <output folder>

Writes songs.txt, from the list in this repository, and lyrics.txt, made from the lyrics in the
executable. Lyrics belong to their authors, so they are never kept in this repository, and what
this makes from them stays out of it too.

Reading kanji needs a dictionary (fugashi with unidic-lite). Without one there are no lyrics, only
song names. A dictionary can guess wrong, songs most of all, so a line can come out misread.
"""
import os
import sys
import unicodedata

LOAD_ADDRESS = 0x08804000
FILE_OFFSET = 0x54
# Where the lyrics are in ULJM05472 v1.01, the only version the build accepts.
LYRICS = (0x3238CC, 0x32BDD8)

ROWS = {
    'ア': 'a', 'イ': 'i', 'ウ': 'u', 'エ': 'e', 'オ': 'o',
    'カ': 'ka', 'キ': 'ki', 'ク': 'ku', 'ケ': 'ke', 'コ': 'ko',
    'サ': 'sa', 'シ': 'shi', 'ス': 'su', 'セ': 'se', 'ソ': 'so',
    'タ': 'ta', 'チ': 'chi', 'ツ': 'tsu', 'テ': 'te', 'ト': 'to',
    'ナ': 'na', 'ニ': 'ni', 'ヌ': 'nu', 'ネ': 'ne', 'ノ': 'no',
    'ハ': 'ha', 'ヒ': 'hi', 'フ': 'fu', 'ヘ': 'he', 'ホ': 'ho',
    'マ': 'ma', 'ミ': 'mi', 'ム': 'mu', 'メ': 'me', 'モ': 'mo',
    'ヤ': 'ya', 'ユ': 'yu', 'ヨ': 'yo',
    'ラ': 'ra', 'リ': 'ri', 'ル': 'ru', 'レ': 're', 'ロ': 'ro',
    'ワ': 'wa', 'ヰ': 'i', 'ヱ': 'e', 'ヲ': 'wo',
    'ガ': 'ga', 'ギ': 'gi', 'グ': 'gu', 'ゲ': 'ge', 'ゴ': 'go',
    'ザ': 'za', 'ジ': 'ji', 'ズ': 'zu', 'ゼ': 'ze', 'ゾ': 'zo',
    'ダ': 'da', 'ヂ': 'ji', 'ヅ': 'zu', 'デ': 'de', 'ド': 'do',
    'バ': 'ba', 'ビ': 'bi', 'ブ': 'bu', 'ベ': 'be', 'ボ': 'bo',
    'パ': 'pa', 'ピ': 'pi', 'プ': 'pu', 'ペ': 'pe', 'ポ': 'po',
    'ヴ': 'vu',
    'ァ': 'a', 'ィ': 'i', 'ゥ': 'u', 'ェ': 'e', 'ォ': 'o',
    'ャ': 'ya', 'ュ': 'yu', 'ョ': 'yo', 'ヮ': 'wa',
}
# A consonant sound joined with the small kana after it.
JOINED = {
    'キ': 'ky', 'シ': 'sh', 'チ': 'ch', 'ニ': 'ny', 'ヒ': 'hy', 'ミ': 'my', 'リ': 'ry',
    'ギ': 'gy', 'ジ': 'j', 'ヂ': 'j', 'ビ': 'by', 'ピ': 'py',
    'フ': 'f', 'ヴ': 'v', 'テ': 't', 'デ': 'd', 'ウ': 'w', 'ツ': 'ts',
}
SMALL_Y = {'ャ': 'a', 'ュ': 'u', 'ョ': 'o'}
SMALL_VOWEL = {'ァ': 'a', 'ィ': 'i', 'ゥ': 'u', 'ェ': 'e', 'ォ': 'o'}
PUNCTUATION = {'、': ',', '。': '.', '・': ' ', '「': '"', '」': '"', '『': '"', '』': '"', '　': ' ',
               '〜': '~', '～': '~', '…': '...', '【': '[', '】': ']'}
# Particles, which are written with one kana and said as another.
PARTICLES = {'ハ': 'wa', 'ヘ': 'e', 'ヲ': 'wo'}


def to_katakana(text):
    return ''.join(chr(ord(c) + 0x60) if 'ぁ' <= c <= 'ゖ' else c for c in text)


def has_kana(text):
    return any('ァ' <= c <= 'ヺ' or c == 'ー' for c in to_katakana(text))


def kana_to_romaji(kana):
    """Hepburn, with long vowels as two letters so that the game's font can show them."""
    kana = to_katakana(kana)
    out = ''
    double = False
    i = 0
    while i < len(kana):
        c = kana[i]
        following = kana[i + 1] if i + 1 < len(kana) else ''
        if c == 'ッ':
            double = True
            i += 1
            continue
        if c == 'ン':
            out += 'n'
            if following and following in 'アイウエオヤユヨ':
                out += "'"
            i += 1
            continue
        if c == 'ー':
            if out and out[-1] in 'aiueo':
                out += out[-1]
            i += 1
            continue
        if c in JOINED and (following in SMALL_Y or following in SMALL_VOWEL):
            # シャ is sha and キャ kya, but ファ is fa and ティ ti.
            piece = None
            if following in SMALL_Y and c in 'キシチニヒミリギジヂビピ':
                piece = JOINED[c] + SMALL_Y[following]
            elif following in SMALL_VOWEL:
                piece = JOINED[c] + SMALL_VOWEL[following]
            if piece:
                if double:
                    out += 't' if piece.startswith('ch') else piece[0]
                    double = False
                out += piece
                i += 2
                continue
        piece = ROWS.get(c)
        if piece is None:
            out += c
            double = False
            i += 1
            continue
        if double:
            if piece[0] not in 'aiueo':
                out += 't' if piece.startswith('ch') else piece[0]
            double = False
        out += piece
        i += 1
    return out


def join_tokens(tokens):
    """Tokens of (text, joins the word before it)."""
    out = ''
    for text, joined in tokens:
        if not text:
            continue
        if not out or joined or out.endswith((' ', '"', '[')) or text[0] in ',.!?~)]" ':
            out += text
        else:
            out += ' ' + text
    return ' '.join(out.split())


def line_to_romaji(tagger, line):
    tokens = []
    for word in tagger(line):
        surface = word.surface
        feature = word.feature
        kana = getattr(feature, 'kana', None)
        pos1 = getattr(feature, 'pos1', '') or ''
        pos2 = getattr(feature, 'pos2', '') or ''
        if surface in PUNCTUATION:
            tokens.append((PUNCTUATION[surface], False))
        elif pos1 == '助詞' and kana in PARTICLES and surface in ('は', 'へ', 'を', 'ハ', 'ヘ', 'ヲ'):
            tokens.append((PARTICLES[kana], False))
        elif kana and kana != '*' and has_kana(kana):
            joined = pos1 in ('助動詞', '接尾辞') or (pos1 == '助詞' and pos2 == '接続助詞') or pos2 == '非自立可能'
            tokens.append((kana_to_romaji(kana), joined and pos1 != '名詞'))
        elif has_kana(surface):
            tokens.append((kana_to_romaji(surface), False))
        else:
            tokens.append((unicodedata.normalize('NFKC', surface), False))
    return join_tokens(tokens)


def strings_in(game, start, end):
    at = start
    while at < end:
        stop = game.index(b'\0', at)
        if stop > at:
            yield at, game[at:stop]
        at = stop + 1


def write_songs(titles, out):
    written = 0
    with open(titles, encoding='utf-8') as source, open(os.path.join(out, 'songs.txt'), 'wb') as target:
        for line in source:
            line = line.strip()
            if line and not line.startswith('#'):
                target.write(line.encode('cp932') + b'\n')
                written += 1
    print('song names: %d' % written)


def write_lyrics(game, out):
    try:
        import fugashi
        tagger = fugashi.Tagger()
    except Exception as problem:  # a dictionary that will not load only costs the lyrics
        print('no dictionary, so no romaji for lyrics: %s' % problem)
        return False
    lines = misread = 0
    path = os.path.join(out, 'lyrics.txt')
    with open(path + '.new', 'wb') as target:
        for at, raw in strings_in(game, *LYRICS):
            if all(b < 0x80 for b in raw):
                continue
            try:
                text = raw.decode('cp932')
            except UnicodeDecodeError:
                continue
            romaji = line_to_romaji(tagger, text)
            if not romaji:
                continue
            # Whatever the dictionary could not read stays as it is on the disc.
            misread += any(ord(c) >= 0x80 for c in romaji)
            address = at - FILE_OFFSET + LOAD_ADDRESS
            target.write(('%08X = ' % address).encode('ascii') + romaji.encode('cp932', 'replace') + b'\n')
            lines += 1
    os.replace(path + '.new', path)
    print('lyric lines: %d, with something left unread: %d' % (lines, misread))
    return True


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    with open(sys.argv[1], 'rb') as f:
        game = f.read()
    os.makedirs(sys.argv[3], exist_ok=True)
    write_songs(sys.argv[2], sys.argv[3])
    # Exit 2: the song names are done, and lyrics have to wait for the dictionary.
    sys.exit(0 if write_lyrics(game, sys.argv[3]) else 2)


if __name__ == '__main__':
    main()
