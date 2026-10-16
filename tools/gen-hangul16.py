#!/usr/bin/env python3
"""시험용 16×16 완성형 한글 글자 데이터 생성 → src/hangul/data/HgFn0000.bin, HgFn0001.bin

Neo둥근모(OFL 1.1, src/hangul/data/NEODGM-OFL.md)의 TTF를 16픽셀 격자로 그려 KS X 1001 완성형 한글 2,350자를
글자마다 32바이트(16줄 × 16비트, 빅엔디언, 왼쪽이 MSB)로 저장한다. 순서는 KS X 1001 순서
(순위 r = (앞바이트-0xB0)*94 + (뒷바이트-0xA1)). Palm 리소스 한계(64KB) 때문에 둘로 나눈다:
HgFn 0 = 순위 0~1174, HgFn 1 = 1175~2349.
HgFn 2 = KS X 1001 한글 자모 행 0xA4A1~0xA4FE(94자, 조합 중 단독 자모용). 글꼴에 없는 글자는 빈칸.
이 데이터는 2c 그리기 시험용이며, 최종 모듈은 벌식 조합형 글꼴을 쓴다(docs/KOREAN_SUPPORT.md).

그리는 방법: 이 글꼴은 1픽셀 = 64단위(unitsPerEm 1024 = 16픽셀, 기준선 위 12픽셀) 격자 위의 사각형 윤곽이고,
한글 음절은 초·중·종성 조각(component)을 겹쳐 만든다. PIL(FreeType) 렌더링은 조각이 겹치는 곳에 한 줄씩
더 칠해 일부 획이 두꺼워졌다(v12 실측). 그래서 윤곽을 직접 풀어 픽셀 중심이 안에 있는지(nonzero winding)로 칠한다.

사용: python3 tools/gen-hangul16.py <neodgm.ttf>   (npm 패키지 @kfonts/neodgm 0.5.0의 src/neodgm.ttf)
"""
import os
import sys

from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.ttLib import TTFont

SPLIT = 1175

ttf = sys.argv[1]
tt = TTFont(ttf)
cmap = tt.getBestCmap()
gset = tt.getGlyphSet()
assert tt['head'].unitsPerEm == 1024 and tt['hhea'].ascent == 768


def render(ch):
    """16×16 비트맵 32바이트. 기준선 = 위에서 12줄, 1픽셀 = 64단위, nonzero winding"""
    pen = DecomposingRecordingPen(gset)
    gset[cmap[ord(ch)]].draw(pen)
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
            raise ValueError('curve in pixel font: ' + op)
    out = bytearray()
    for y in range(16):
        fy = (12 - y - 0.5) * 64
        v = 0
        for x in range(16):
            fx = (x + 0.5) * 64
            w = 0
            for poly in polys:
                n = len(poly)
                for i in range(n):
                    (x1, y1), (x2, y2) = poly[i], poly[(i + 1) % n]
                    if (y1 > fy) != (y2 > fy) and fx < x1 + (fy - y1) * (x2 - x1) / (y2 - y1):
                        w += 1 if y2 > y1 else -1
            if w != 0:
                v |= 0x8000 >> x
        out += bytes([v >> 8, v & 0xFF])
    return out


glyphs = bytearray()
for r in range(2350):
    code = bytes([0xB0 + r // 94, 0xA1 + r % 94])
    ch = code.decode('euc_kr')
    assert ord(ch) in cmap, ch
    glyphs += render(ch)
assert len(glyphs) == 2350 * 32



jamo = bytearray()
missing = 0
for t in range(0xA1, 0xFF):
    try:
        ch = bytes([0xA4, t]).decode('euc_kr')
    except UnicodeDecodeError:
        ch = None
    if ch is None or ord(ch) not in cmap:
        jamo += bytes(32)
        missing += 1
    else:
        jamo += render(ch)
assert len(jamo) == 94 * 32
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
d = os.path.join(root, 'src/hangul/data')
open(os.path.join(d, 'HgFn0000.bin'), 'wb').write(glyphs[:SPLIT * 32])
open(os.path.join(d, 'HgFn0001.bin'), 'wb').write(glyphs[SPLIT * 32:])
open(os.path.join(d, 'HgFn0002.bin'), 'wb').write(jamo)
print('jamo missing', missing)
if '--preview' in sys.argv:
    for s in ('한글', '가힝'):
        for y in range(16):
            line = ''
            for ch in s:
                c = ch.encode('euc_kr')
                r = (c[0] - 0xB0) * 94 + c[1] - 0xA1
                v = (glyphs[r * 32 + y * 2] << 8) | glyphs[r * 32 + y * 2 + 1]
                line += ''.join('#' if v & (0x8000 >> x) else '.' for x in range(16)) + ' '
            print(line)
print('ok', len(glyphs), 'bytes')
