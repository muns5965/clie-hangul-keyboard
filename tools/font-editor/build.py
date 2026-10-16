#!/usr/bin/env python3
"""Neo둥근모 24 편집기(웹 페이지) 생성 → tools/font-editor/index.html

Neo둥근모(OFL 1.1)의 한글 조각(초성 cho_<벌>_<자모>, 중성 jung_.., 종성 jong_..; 8×4×4 벌식, 344개)을
16×16 비트맵으로 풀고(1픽셀 = 64단위 격자, nonzero), 음절 11,172자마다 쓰는 조각 번호를 뽑아
editor.html 틀의 /*DATA*/ 자리에 넣는다. 낱자모 손그림(src/hangul/data/hangul24-jamo.json)도 함께 넣는다. 조각은 이동·변형 없이 겹치기만 한다(검사함).

사용: python3 tools/font-editor/build.py <neodgm.ttf>   (npm @kfonts/neodgm 0.5.0의 src/neodgm.ttf)
"""
import base64
import json
import os
import sys

from fontTools.pens.recordingPen import DecomposingRecordingPen, RecordingPen
from fontTools.ttLib import TTFont

here = os.path.dirname(os.path.abspath(__file__))
tt = TTFont(sys.argv[1])
cmap = tt.getBestCmap()
gs = tt.getGlyphSet()
assert tt['head'].unitsPerEm == 1024 and tt['hhea'].ascent == 768


def raster(name):
    pen = DecomposingRecordingPen(gs)
    gs[name].draw(pen)
    polys, cur = [], []
    for op, args in pen.value:
        if op == 'moveTo':
            cur = [args[0]]
        elif op == 'lineTo':
            cur.append(args[0])
        elif op in ('closePath', 'endPath'):
            polys.append(cur)
            cur = []
        else:
            raise ValueError(op)
    rows = []
    for y in range(16):
        fy = (12 - y - 0.5) * 64
        v = 0
        for x in range(16):
            fx = (x + 0.5) * 64
            w = 0
            for p in polys:
                n = len(p)
                for i in range(n):
                    (x1, y1), (x2, y2) = p[i], p[(i + 1) % n]
                    if (y1 > fy) != (y2 > fy) and fx < x1 + (fy - y1) * (x2 - x1) / (y2 - y1):
                        w += 1 if y2 > y1 else -1
            if w:
                v |= 0x8000 >> x
        rows.append(v)
    return rows


ORDER = {'cho': 0, 'jung': 1, 'jong': 2}
used = {}
syl = []
for c in range(0xAC00, 0xD7A4):
    pen = RecordingPen()
    gs[cmap[c]].draw(pen)
    comps = []
    for op, args in pen.value:
        assert op == 'addComponent' and tuple(args[1]) == (1, 0, 0, 1, 0, 0), (chr(c), op, args)
        comps.append(args[0])
        used[args[0]] = True
    syl.append(comps)


def key(n):
    k, a, b = n.split('_')
    return (ORDER[k], int(a), int(b))


names = sorted(used, key=key)
index = {n: i for i, n in enumerate(names)}
pieces = [''.join('%04x' % r for r in raster(n)) for n in names]
mp = bytearray()
for comps in syl:
    ids = [index[n] for n in comps] + [0xFFFF] * (3 - len(comps))
    for v in ids:
        mp += bytes([v >> 8, v & 0xFF])
# 낱자모 손그림(조각에서 만들지 않는 글자, 24×24). 생성 도구 gen-johab24.py의 ②와 같은 파일
hand = json.load(open(os.path.join(here, '..', '..', 'src', 'hangul', 'data', 'hangul24-jamo.json'), encoding='utf-8'))
assert hand['format'] == 'mnhangul-jamo' and hand['size'] == 24
data = {
    'base': 'Neo둥근모 1.600 (OFL 1.1)',
    'names': names,
    'pieces': pieces,
    'map': base64.b64encode(bytes(mp)).decode(),
    'hand': hand['glyphs'],
}
tpl = open(os.path.join(here, 'editor.html'), encoding='utf-8').read()
out = tpl.replace('/*DATA*/null', json.dumps(data, ensure_ascii=False, separators=(',', ':')))
open(os.path.join(here, 'index.html'), 'w', encoding='utf-8').write(out)
print('pieces', len(names), 'syllables', len(syl), 'html', len(out))
