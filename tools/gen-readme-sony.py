#!/usr/bin/env python3
"""README 소개 그림 06 — Sony 가상 키보드 한글(MnHangul) → docs/images/06-sony-keyboard.png

다른 소개 그림(gen-readme-images.py)과 달리 실제 CloudpilotEmu(NR70V) 화면 캡처 두 장을 쓴다.
캡처는 Palm 화면 부분(320×480 고해상도 화면을 확대한 것)만 잘라 docs/images/capture-*.png 로 둔다.

사용: python3 tools/gen-readme-sony.py docs/images/capture-sony-base.png docs/images/capture-sony-shift.png
필요: Pillow, Noto Sans CJK(/usr/share/fonts/opentype/noto)
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NOTO = '/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc'
NOTO_R = '/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'
H1 = ImageFont.truetype(NOTO, 44, index=1)
H2 = ImageFont.truetype(NOTO_R, 26, index=1)
CAP = ImageFont.truetype(NOTO_R, 22, index=1)
INK = (24, 28, 26)
BG = (232, 238, 233)
ACCENT = (31, 95, 191)
SUB = (90, 96, 92)

W = 760
shots = [Image.open(p).convert('RGB') for p in sys.argv[1:3]]
sw = 330
shots = [s.resize((sw, round(s.height * sw / s.width)), Image.LANCZOS) for s in shots]
sh = shots[0].height

H = 168 + sh + 50 + 90
img = Image.new('RGB', (W, H), BG)
d = ImageDraw.Draw(img)
d.text((40, 36), 'Sony 가상 키보드로 한글', font=H1, fill=INK)
d.text((40, 104), 'MnHangul 하나, 리셋 없음 · 쉬프트 고정 = 한/영', font=H2, fill=SUB)
d.rectangle((40, 146, 110, 150), fill=ACCENT)

y = 168
caps = ['쉬프트 고정: 두벌식 자모', '+ Shift: 쌍자음']
for i, s in enumerate(shots):
    x = 40 + i * (sw + 20)
    d.rectangle((x - 6, y - 6, x + sw + 5, y + sh + 5), fill=(48, 52, 54))
    img.paste(s, (x, y))
    tw = d.textlength(caps[i], font=CAP)
    d.text((x + (sw - tw) / 2, y + sh + 12), caps[i], font=CAP, fill=INK)

d.text((40, y + sh + 56), 'CloudpilotEmu(NR70V) 실제 캡처. Sony 내장 키보드의 영문 키 글자를', font=CAP, fill=SUB)
d.text((40, y + sh + 86), '자모로 바꿔 그리고, 누른 글자를 두벌식으로 조합해 넣습니다.', font=CAP, fill=SUB)
out = os.path.join(root, 'docs', 'images', '06-sony-keyboard.png')
img.save(out, optimize=True)
print(out, img.size)
