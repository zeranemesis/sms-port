"""Parse marioEU.MAP into {symbol: info}, tracking the section each row lives in.

The map's symbol tables are laid out as:

    .text section layout
      Starting        Virtual
      address  Size   address
      -----------------------
      000000e0 000374 800056e0  4 calcForces__11TBoidLeaderCFPC5TBoid  Animal.a boid.cpp
      UNUSED   000050 ........ TRK_memmove                              TRK...a mem_TRK.c

* the `<name> section layout` header names the section the following rows belong to
* ordinal 1 marks a section row (its name starts with `.`), ordinal 4 a code symbol
* UNUSED rows keep their size but show `........` where the address goes, and
  have no ordinal column at all

Only `.text` rows are functions; everything else is data of some kind, and
counting them as missing functions inflates the backlog badly.
"""
import re

MAPF = 'orig/GMSP01/files/marioEU.MAP'

PAT_SECTION = re.compile(r'^\s*(\S+)\s+section layout\s*$')
PAT_UNUSED = re.compile(
    r'^\s*UNUSED\s+([0-9A-Fa-f]{4,8})\s+\S+\s+(\S+)\s+(\S+\.a)\s+'
    r'(\S+\.(?:cpp|c|c\+\+|cc|s))\s*$')
PAT_ROW = re.compile(
    r'^\s*([0-9A-Fa-f]{4,8})\s+([0-9A-Fa-f]{4,8})\s+[0-9A-Fa-f]{4,8}\s+'
    r'(\d+)\s+(\S+)\s+\t?(\S+\.a)\s+(\S+\.(?:cpp|c|c\+\+|cc|s))\s*$')


def load_map(path=MAPF):
    entries = {}
    stats = {'unused': 0, 'live': 0, 'section_rows': 0, 'unparsed': 0}
    section = None
    for line in open(path, 'r', encoding='latin-1', errors='replace'):
        s = line.rstrip('\r\n')
        if '.a' not in s:
            m = PAT_SECTION.match(s)
            if m:
                section = m.group(1)
            continue
        m = PAT_UNUSED.match(s)
        if m:
            sym = m.group(2)
            if sym.startswith('.'):
                continue
            prev = entries.get(sym)
            if prev is None:
                entries[sym] = {'size': int(m.group(1), 16), 'unused': True,
                                'lib': m.group(3), 'file': m.group(4),
                                'section': section}
            stats['unused'] += 1
            continue
        m = PAT_ROW.match(s)
        if m:
            ordinal, sym = m.group(3), m.group(4)
            if ordinal == '1' or sym.startswith('.'):
                stats['section_rows'] += 1
                continue
            prev = entries.get(sym)
            if prev is None or prev.get('unused'):
                entries[sym] = {'size': int(m.group(2), 16), 'unused': False,
                                'lib': m.group(5), 'file': m.group(6),
                                'section': section}
            stats['live'] += 1
            continue
        stats['unparsed'] += 1
    return entries, stats


def text_symbols(path=MAPF):
    """Only the `.text` rows -- i.e. actual functions, live and UNUSED alike."""
    e, _ = load_map(path)
    return {k: v for k, v in e.items() if v.get('section') == '.text'}


if __name__ == '__main__':
    e, st = load_map()
    print('symbols %d   %s' % (len(e), st))
    from collections import Counter
    print('by section:', Counter(v.get('section') for v in e.values()))
    t = {k: v for k, v in e.items() if v.get('section') == '.text'}
    print('.text symbols: %d   live %d   unused %d   bytes %d'
          % (len(t), sum(1 for v in t.values() if not v['unused']),
             sum(1 for v in t.values() if v['unused']),
             sum(v['size'] for v in t.values())))
    for probe in ('checkDropInWater__6TGessoFv',
                  'perform__13TBossHanachanFUlPQ26JDrama9TGraphics',
                  'instance$2000', '@3673'):
        print('%-56s %s' % (probe, e.get(probe)))
