#!/usr/bin/env python3
"""Sony 가상 키보드 ⇪ 키 대신 그릴 '한A' 아이콘 → src/hangul/data/HgIc0000.bin(보통)·HgIc0001.bin(켜짐)

MnHangul v43. ROM SilkExtn의 ⇪ 그림(Tbmp 2400 보통, 2500 반전)은 18×18 1비트 + 8비트 묶음이고
8비트판은 글자 색 0(흰색), 바탕 0xD8(보통)·0x39(켜짐, 파랑)이다(ROM 실측). 같은 색으로 22×18 8비트 그림을 만든다.
글자: Galmuri11 12px(OFL 1.1) '한A'. 키 칸(고해상도 약 25×20)에 맞춰 가운데 정렬.

사용: python3 tools/gen-caps-icon.py <Galmuri11.ttf>
"""
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W, H = 22, 18
font = ImageFont.truetype(sys.argv[1], 12)
mask = Image.new('1', (W, H), 0)
d = ImageDraw.Draw(mask)
bb = d.textbbox((0, 0), '한A', font=font)
d.text(((W - (bb[2] - bb[0])) // 2 - bb[0], (H - (bb[3] - bb[1])) // 2 - bb[1] + 1), '한A', font=font, fill=1)

for n, bg in ((0, 0xD8), (1, 0x39)):
    hdr = struct.pack('>hhHHBBHBBH', W, H, W, 0, 8, 2, 0, 0, 0, 0)
    px = bytes(0x00 if mask.getpixel((x, y)) else bg for y in range(H) for x in range(W))
    out = os.path.join(root, 'src/hangul/data', 'HgIc%04d.bin' % n)
    open(out, 'wb').write(hdr + px)
    print(out, len(hdr + px))
for y in range(H):
    print(''.join('#' if mask.getpixel((x, y)) else '.' for x in range(W)))
