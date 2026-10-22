#!/usr/bin/env python3
"""README 그림 — CloudpilotEmu(NR70V) 실제 화면 캡처로 만든다 → docs/images/*.png

캡처(docs/images/capture/*.png)는 Palm 화면 부분(고해상도 320×480을 2배로 그린 640×960)만 잘라 둔 것이다.
에뮬레이터를 브라우저 없이 돌려 캡처했다(cloudpilot-emu-embedded + 헤드리스 Chromium, 펜 누름을 스크립트로 재생).

사용: python3 tools/gen-readme-captures.py
필요: Pillow, Noto Sans CJK(/usr/share/fonts/opentype/noto)
"""
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CAP = os.path.join(ROOT, 'docs', 'images', 'capture')
OUT = os.path.join(ROOT, 'docs', 'images')
NOTO = '/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc'
NOTO_R = '/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'
H1 = ImageFont.truetype(NOTO, 40, index=1)
H2 = ImageFont.truetype(NOTO_R, 24, index=1)
CF = ImageFont.truetype(NOTO_R, 21, index=1)
INK = (24, 28, 26)
BG = (232, 238, 233)
ACCENT = (31, 95, 191)
SUB = (90, 96, 92)
SW = 300   # 캡처 한 장 폭(원본 640의 약 절반 — 고해상도 점이 거의 그대로)
GAP = 20

FIGS = {   # 세 키보드 같은 말: 부제 = "모듈 · 한/A 단추 자리 = 한/영", 칸 = "한: …", "Shift: 쌍자음", "A: 영문"
    'system-keyboard.png': ('1. 시스템 키보드 화면으로 한글', 'MnKbdHan · Int\'l 탭 오른쪽 한/A = 한/영 · 메모 Edit > Keyboard',
                            [('kh_han', '한: 두벌식 자모'), ('kh_shift', 'Shift: 쌍자음'), ('kh_eng', 'A: 영문')]),
    'mn-keyboard.png': ('2. Mn 가상 키보드', 'MnSkPlugin + MnSkCore · 왼쪽 아래 한/A 키 = 한/영',
                        [('mn_han', '한: 단모음 자모'), ('mn_shift', 'Shift: 쌍자음'),
                         ('mn_eng', 'A: 영문'), ('mn_num', '123: 숫자·기호')]),
    'sony-keyboard.png': ('3. Sony 가상 키보드로 한글', 'MnSonyKbd · sym 탭 오른쪽 한/A = 한/영',
                          [('sony_han', '한: 두벌식 자모'), ('sony_shift', 'Shift: 쌍자음'),
                           ('sony_eng', 'A: 영문'), ('sony_sym', 'sym: 기호(그대로)')]),
    'apps.png': ('설정 앱 네 개 — 같은 화면', 'MnHangul · MnSonyKbd · MnKbdHan · MnSkKbd · 모듈 상태 4줄은 어느 앱에서나 같음',
                 [('app_hg', 'MnHangul (한글 표시)'), ('app_sy', 'MnSonyKbd'), ('app_kh', 'MnKbdHan'), ('app_sk', 'MnSkKbd (Mn 키보드)')]),
    'mnhangul.png': ('CJKOS 없이 한글 표시', 'MnHangul · 조합형 글꼴(24×24) · 2바이트 글자 경계·줄바꿈',
                     [('hg_memo', '메모장(24×24 글꼴)')]),
}


def figure(name, title, sub, shots):
    ims = []
    for f, _ in shots:
        s = Image.open(os.path.join(CAP, f + '.png')).convert('RGB')
        ims.append(s.resize((SW, round(s.height * SW / s.width)), Image.LANCZOS))
    sh = ims[0].height
    n = len(ims)
    W = max(40 * 2 + n * SW + (n - 1) * GAP, 720)
    H = 150 + sh + 56 + 50
    img = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(img)
    d.text((40, 28), title, font=H1, fill=INK)
    d.text((40, 90), sub, font=H2, fill=SUB)
    d.rectangle((40, 128, 110, 132), fill=ACCENT)
    y = 150
    x0 = (W - (n * SW + (n - 1) * GAP)) // 2
    for i, (im, (_, cap)) in enumerate(zip(ims, shots)):
        x = x0 + i * (SW + GAP)
        d.rectangle((x - 1, y - 1, x + SW, y + sh), outline=(160, 168, 162))
        img.paste(im, (x, y))
        tw = d.textlength(cap, font=CF)
        d.text((x + (SW - tw) / 2, y + sh + 12), cap, font=CF, fill=INK)
    d.text((40, y + sh + 56), 'CloudpilotEmu(PEG-NR70V, Palm OS 4.1S) 실제 화면 캡처', font=CF, fill=SUB)
    img.save(os.path.join(OUT, name), optimize=True)
    print(name, img.size)


for k, v in FIGS.items():
    figure(k, *v)
