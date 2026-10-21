#!/usr/bin/env python3
"""Sony 가상 키보드 sym 탭 오른쪽 한/영 표시 → src/sony/data/SyIc0002.bin('한', 한글 모드)·SyIc0003.bin('A', 영문)

MnSonyKbd v22(진단). 8비트 그림, 글자 색 0(흰색), 바탕은 투명(flags hasTransparency, 투명 색 0xD8) — 탭 줄 바탕색을
모르므로 글자만 그린다. 글자: Galmuri11 12px(OFL 1.1, gen-caps-icon.py와 같음). 크기 14×18(고해상도).

사용: python3 tools/gen-tab-mark.py <Galmuri11.ttf>
"""
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W, H, BG = 14, 18, 0xD8
font = ImageFont.truetype(sys.argv[1], 12)
for n, text in ((2, '한'), (3, 'A')):
    mask = Image.new('1', (W, H), 0)
    d = ImageDraw.Draw(mask)
    bb = d.textbbox((0, 0), text, font=font)
    d.text(((W - (bb[2] - bb[0])) // 2 - bb[0], (H - (bb[3] - bb[1])) // 2 - bb[1] + 1), text, font=font, fill=1)
    hdr = struct.pack('>hhHHBBHBBH', W, H, W, 0x2000, 8, 2, 0, BG, 0, 0)
    px = bytes(0x00 if mask.getpixel((x, y)) else BG for y in range(H) for x in range(W))
    out = os.path.join(root, 'src/sony/data', 'SyIc%04d.bin' % n)
    open(out, 'wb').write(hdr + px)
    print(out, len(hdr + px))
    for y in range(H):
        print(''.join('#' if mask.getpixel((x, y)) else '.' for x in range(W)))
