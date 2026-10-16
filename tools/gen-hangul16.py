#!/usr/bin/env python3
"""시험용 16×16 완성형 한글 글자 데이터 생성 → src/hangul/data/HgFn0000.bin, HgFn0001.bin

Neo둥근모(OFL 1.1, src/hangul/data/NEODGM-OFL.md)의 TTF를 16픽셀로 그려 KS X 1001 완성형 한글 2,350자를
글자마다 32바이트(16줄 × 16비트, 빅엔디언, 왼쪽이 MSB)로 저장한다. 순서는 KS X 1001 순서
(순위 r = (앞바이트-0xB0)*94 + (뒷바이트-0xA1)). Palm 리소스 한계(64KB) 때문에 둘로 나눈다:
HgFn 0 = 순위 0~1174, HgFn 1 = 1175~2349.
이 데이터는 2c 그리기 시험용이며, 최종 모듈은 벌식 조합형 글꼴을 쓴다(docs/KOREAN_SUPPORT.md).

사용: python3 tools/gen-hangul16.py <neodgm.ttf>   (npm 패키지 @kfonts/neodgm 0.5.0의 src/neodgm.ttf)
"""
import os
import sys

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

SPLIT = 1175

ttf = sys.argv[1]
cmap = TTFont(ttf).getBestCmap()
font = ImageFont.truetype(ttf, 16)
glyphs = bytearray()
for r in range(2350):
    code = bytes([0xB0 + r // 94, 0xA1 + r % 94])
    ch = code.decode('euc_kr')
    assert ord(ch) in cmap, ch
    im = Image.new('1', (16, 16), 0)
    ImageDraw.Draw(im).text((0, 0), ch, font=font, fill=1)
    for y in range(16):
        v = 0
        for x in range(16):
            if im.getpixel((x, y)):
                v |= 0x8000 >> x
        glyphs += bytes([v >> 8, v & 0xFF])
assert len(glyphs) == 2350 * 32
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
d = os.path.join(root, 'src/hangul/data')
open(os.path.join(d, 'HgFn0000.bin'), 'wb').write(glyphs[:SPLIT * 32])
open(os.path.join(d, 'HgFn0001.bin'), 'wb').write(glyphs[SPLIT * 32:])
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
