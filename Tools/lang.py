"""Work on a translation with the game's own text beside it.

    python Tools/lang.py review <executable> lang/en.txt <output.txt>
        Writes every line of the translation next to the Japanese it replaces.

    python Tools/lang.py missing <executable> <lang_missing_en.txt> lang/en.txt
        Lists what the game showed with no translation, and where the text is in the
        executable when it is there. The game adds to that file when DIVA_LANG_LOG is
        set, so what has been translated since is left out.

The output holds the game's text, so it stays on your computer: do not add it to this
repository. lang/en.txt holds addresses and English only, for the same reason.
"""
import sys

LOAD_ADDRESS = 0x08804000
FILE_OFFSET = 0x54


def text_at(game, address):
    start = address - LOAD_ADDRESS + FILE_OFFSET
    return game[start:game.index(b'\0', start)].decode('cp932', 'replace')


def review(game, table, out):
    with open(table, encoding='utf-8') as f, open(out, 'w', encoding='utf-8') as o:
        for line in f:
            line = line.rstrip('\n')
            if not line or line.startswith('#'):
                continue
            address, _, english = line.partition('=')
            address = int(address.split('+')[0], 16)
            o.write('%08X  %s\n          %s\n\n' % (address, text_at(game, address), english.strip()))


def missing(game, path, table):
    with open(path, encoding='ascii') as f:
        seen = {line.strip() for line in f if line.strip()}
    done = set()
    with open(table, encoding='utf-8') as f:
        for line in f:
            if line.strip() and not line.startswith('#'):
                start = int(line.split('=')[0].split('+')[0], 16) - LOAD_ADDRESS + FILE_OFFSET
                done.add(game[start:game.index(b'\0', start)])
    for item in sorted(seen):
        raw = bytes.fromhex(item)
        if raw in done:
            continue
        at = game.find(b'\0' + raw + b'\0')
        where = '%08X' % (at + 1 - FILE_OFFSET + LOAD_ADDRESS) if at >= 0 else '(not in the executable)'
        sys.stdout.buffer.write(('%s  %s\n' % (where, raw.decode('cp932', 'replace'))).encode('utf-8'))


def main():
    args = sys.argv[1:]
    if len(args) == 4 and args[0] == 'review':
        with open(args[1], 'rb') as f:
            review(f.read(), args[2], args[3])
    elif len(args) == 4 and args[0] == 'missing':
        with open(args[1], 'rb') as f:
            missing(f.read(), args[2], args[3])
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
