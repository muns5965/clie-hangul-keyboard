#!/usr/bin/env python3
"""README 소개 그림 생성 → docs/images/*.png (스토어 소개 화면 형식)

실제 캡처가 아니라 같은 데이터로 다시 그린 그림이다(100% 같지는 않음).
- 키보드 배치·키 크기: src/skwr/skwr.c와 같은 규칙(고해상도 320×130, 4줄, 아랫줄 칸 경계 26·48·64·112·128)
- 자모 키 이름: 키보드 본체와 같은 선 그림 데이터(src/skwr/jamo_glyphs.h, 12×12 격자, 2픽셀 굵기)
- 본문 한글: MnHangul 조합형 묶음(src/hangul/data/HgJ12·16·24.bin)
- 영문·숫자: Neo둥근모 TTF(픽셀 글꼴, 16px) — 실제 Sony 글꼴과 모양이 다름
- 제목 글씨: Noto Sans CJK

사용: python3 tools/gen-readme-images.py <neodgm.ttf>
필요: Pillow, Noto Sans CJK(/usr/share/fonts/opentype/noto)
"""
import os
import re
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out_dir = os.path.join(root, 'docs', 'images')
os.makedirs(out_dir, exist_ok=True)

PIX = ImageFont.truetype(sys.argv[1], 16)
NOTO = '/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc'
NOTO_R = '/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'
H1 = ImageFont.truetype(NOTO, 44, index=1)     # 1 = KR
H2 = ImageFont.truetype(NOTO_R, 26, index=1)
CAP = ImageFont.truetype(NOTO_R, 22, index=1)

INK = (24, 28, 26)
PAPER = (255, 255, 255)
BG = (232, 238, 233)
ACCENT = (31, 95, 191)

# ---------- 조합형 묶음 ----------
def load_pack(name):
    return open(os.path.join(root, 'src/hangul/data', name), 'rb').read()


PK = {12: load_pack('HgJ12.bin'), 16: load_pack('HgJ16.bin'), 24: load_pack('HgJ24.bin')}


def rows_at(pk, at, H):
    if pk[2] >= 2:
        return list(struct.unpack_from('>%dI' % H, pk, at))
    return [r << 16 for r in struct.unpack_from('>%dH' % H, pk, at)]


def glyph(pk, ch):
    """한글 음절·낱자모 → 32비트 줄 목록(왼쪽 = 비트 31)"""
    H, Nc, Nj = pk[3], pk[6], pk[7]
    w = 4 if pk[2] >= 2 else 2
    o_p, n_p, o_e, n_e, o_s, n_s, o_j, _ = struct.unpack_from('>8H', pk, 112)
    c = ord(ch)
    if not 0xAC00 <= c <= 0xD7A3:
        k = ch.encode('euc_kr')[1] - 0xA1
        return rows_at(pk, o_j + k * H * w, H)
    s = c - 0xAC00
    rb = H * w
    for i in range(n_s):
        at = o_s + i * (2 + rb)
        if struct.unpack_from('>H', pk, at)[0] == s:
            return rows_at(pk, at + 2, H)
    L, V, T = s // 588, (s % 588) // 28, s % 28
    ids = None
    for i in range(n_e):
        e = struct.unpack_from('>4H', pk, o_e + i * 8)
        if e[0] == s:
            ids = e[1:]
    if ids is None:
        ids = (pk[10 + V * 2 + (T > 0)] * 19 + L, Nc * 19 + pk[52 + L * 2 + (T > 0)] * 21 + V,
               (Nc * 19 + Nj * 21 + pk[90 + V] * 27 + T - 1) if T else 0xFFFF)
    a = [0] * H
    for pid in ids:
        if pid != 0xFFFF:
            a = [x | y for x, y in zip(a, rows_at(pk, o_p + pid * rb, H))]
    return a


def is_ko(ch):
    return 0xAC00 <= ord(ch) <= 0xD7A3 or 0x3131 <= ord(ch) <= 0x3163


def text(img, x, y, s, size=16, color=INK, line_h=22):
    """한글은 묶음(size), 그 밖은 픽셀 글꼴. y = 줄 위쪽. 다음 x를 돌려줌"""
    px = img.load()
    pk = PK[size]
    cell = {12: 13, 16: 18, 24: 24}[size]
    top = y + (line_h - size) // 2
    d = ImageDraw.Draw(img)
    for ch in s:
        if is_ko(ch):
            g = glyph(pk, ch)
            off = 1 if cell > size else 0
            for r, v in enumerate(g):
                for cx in range(32):
                    if (v >> (31 - cx)) & 1:
                        px[x + off + cx, top + r] = color
            x += cell
        else:
            d.text((x, y + (line_h - 16) // 2 - 1), ch, font=PIX, fill=color)
            x += int(round(d.textlength(ch, font=PIX)))
    return x


# ---------- 키보드 (src/skwr/skwr.c와 같은 규칙) ----------
src = open(os.path.join(root, 'src/skwr/jamo_glyphs.h'), encoding='utf-8').read()
words = [int(v, 16) for v in re.findall(r'0x([0-9a-f]{4})', src.split('JamoSeg:')[0])]
seg = [int(v, 16) for v in re.findall(r'0x([0-9a-f]{2})\b', src.split('JamoSeg:')[1])]
OFF = words          # 인덱스 o(1~51) → JamoSeg 위치, 0xFFFF = 없음 (o = KS 0xA4A1 + o − 1)

HAN = ["ㅂㅈㄷㄱㅅㅗㅐㅔ", "ㅁㄴㅇㄹㅎㅓㅏㅣ", "^ㅋㅌㅊㅍㅜㅡ\b"]
HAN_SH = ["ㅃㅉㄸㄲㅆㅗㅐㅔ", "ㅁㄴㅇㄹㅎㅓㅏㅣ", "^ㅋㅌㅊㅍㅜㅡ\b"]
ENG = ["qwertyuiop", "asdfghjkl", "^zxcvbnm\b"]
NUM = ["1234567890", "-/:;()$&@\"", "#%*+='?!\b"]
BOTTOM = [0, 26, 48, 64, 112, 128, 160]


def draw_jamo(d, ch, ox, oy, color):
    o = ch.encode('euc_kr')[1] - 0xA1 + 1
    at = OFF[o] if o < len(OFF) else 0xFFFF
    if at == 0xFFFF:
        return False
    n = seg[at]
    for i in range(n):
        a, b = seg[at + 1 + i * 2], seg[at + 2 + i * 2]
        x1, y1, x2, y2 = ox + (a >> 4), oy + (a & 15), ox + (b >> 4), oy + (b & 15)
        d.line((x1, y1, x2, y2), fill=color)
        if abs(x2 - x1) >= abs(y2 - y1):
            d.line((x1, y1 + 1, x2, y2 + 1), fill=color)
        else:
            d.line((x1 + 1, y1, x2 + 1, y2), fill=color)
    return True


def round_rect(d, r, fill=None, outline=INK):
    x0, y0, x1, y1 = r
    d.rounded_rectangle((x0, y0, x1 - 1, y1 - 1), radius=4, fill=fill, outline=outline, width=1)


def keyboard(state='han', shift=False, pressed=None):
    """320×130 고해상도 키보드 그림. pressed = (줄, 칸) 반전"""
    W, H = 320, 130
    img = Image.new('RGB', (W, H), PAPER)
    d = ImageDraw.Draw(img)
    rows = {'han': HAN_SH if shift else HAN, 'eng': ENG, 'num': NUM}[state]
    rh = H // 4
    for row in range(4):
        top, bot = row * rh, (H if row == 3 else row * rh + rh)
        keys = rows[row] if row < 3 else ['한/A', '123', ',', ' ', '.', 'Ent']
        n = len(keys)
        for col, k in enumerate(keys):
            if row == 3:
                l, r = BOTTOM[col] * 2, BOTTOM[col + 1] * 2
            else:
                l, r = col * W // n, (col + 1) * W // n
            box = (l + 3, top + 3, r - 3, bot - 3)
            inv = (pressed == (row, col)) or (k == '^' and shift) or (k == '123' and state == 'num')
            round_rect(d, box, fill=INK if inv else PAPER)
            fg = PAPER if inv else INK
            cx, cy = (box[0] + box[2]) // 2, (box[1] + box[3]) // 2
            if k == '\b':
                k = 'BS'
            if len(k) == 1 and is_ko(k) and draw_jamo(d, k, cx - 6, cy - 6, fg):
                continue
            if k == ' ':
                continue
            if state == 'eng' and shift and k.isalpha() and len(k) == 1:
                k = k.upper()
            tw = 0
            for ch in k:
                tw += 18 if is_ko(ch) else int(round(d.textlength(ch, font=PIX)))
            text(img, cx - tw // 2, cy - 11, k, color=fg)
    return img


# ---------- 화면 ----------
def statusbar(img, y):
    d = ImageDraw.Draw(img)
    d.rectangle((0, y, 319, y + 23), fill=(70, 74, 72))
    for i in range(5):
        d.rounded_rectangle((4 + i * 26, y + 3, 24 + i * 26, y + 20), radius=3, fill=(205, 208, 206))
    d.text((262, y + 3), "6:15 pm", font=PIX, fill=(240, 240, 240))


def memo_screen(lines, kb, title="Memo 1 of 3", size=16, line_h=22, cursor=None):
    """320×480: 제목 줄 + 본문(밑줄) + 키보드(아래 130) + 상태바(24)"""
    img = Image.new('RGB', (320, 480), PAPER)
    d = ImageDraw.Draw(img)
    d.rectangle((0, 0, 319, 23), fill=PAPER)
    d.rounded_rectangle((0, 0, 140, 26), radius=4, fill=(32, 24, 140))
    d.text((8, 4), title, font=PIX, fill=(255, 255, 255))
    d.line((0, 26, 319, 26), fill=(32, 24, 140), width=3)
    y = 34
    for i in range(12):
        ly = y + i * line_h
        if ly + line_h > 480 - 130 - 24 - 34:
            break
        for x in range(4, 316, 3):
            img.putpixel((x, ly + line_h - 1), (150, 150, 150))
        if i < len(lines):
            end = text(img, 6, ly, lines[i], size=size, line_h=line_h)
            if cursor == i:
                d.line((end + 1, ly + 2, end + 1, ly + line_h - 3), fill=INK, width=2)
    by = 480 - 130 - 24 - 30
    round_rect(d, (6, by, 70, by + 26))
    text(img, 18, by + 2, "Done")
    round_rect(d, (80, by, 160, by + 26))
    text(img, 90, by + 2, "Details")
    img.paste(kb, (0, 480 - 130 - 24))
    statusbar(img, 480 - 24)
    return img


def frame(screen, scale=2):
    """기기 화면 테두리"""
    s = screen.resize((screen.width * scale, screen.height * scale), Image.NEAREST)
    pad = 14
    f = Image.new('RGB', (s.width + pad * 2, s.height + pad * 2), (54, 58, 60))
    f.paste(s, (pad, pad))
    return f


def card(title, sub, body, name, notes=()):
    W = 760
    top = 200
    hh = top + body.height + 40 + len(notes) * 34
    img = Image.new('RGB', (W, hh), BG)
    d = ImageDraw.Draw(img)
    d.text((40, 42), title, font=H1, fill=INK)
    d.text((40, 112), sub, font=H2, fill=(80, 92, 86))
    d.rectangle((40, 160, 110, 165), fill=ACCENT)
    img.paste(body, ((W - body.width) // 2, top))
    for i, n in enumerate(notes):
        d.text((40, top + body.height + 16 + i * 34), n, font=CAP, fill=(80, 92, 86))
    img.save(os.path.join(out_dir, name), optimize=True)
    print(name, img.size)


def stack(*imgs, gap=24, label=None):
    w = max(i.width for i in imgs)
    lab_h = 40 if label else 0
    out = Image.new('RGB', (w, sum(i.height for i in imgs) + gap * (len(imgs) - 1) + lab_h * len(imgs)), BG)
    y = 0
    d = ImageDraw.Draw(out)
    for k, i in enumerate(imgs):
        if label:
            d.text((0, y + 4), label[k], font=CAP, fill=INK)
            y += lab_h
        out.paste(i, ((w - i.width) // 2, y))
        y += i.height + gap
    return out


# 1. 전체 화면
card("가상 실크스크린 자리에 한글 키보드",
     "Sony CLIÉ PEG-NR70V · Palm OS 4.1",
     frame(memo_screen(["한글 키보드 시험", "안녕하세요, 클리에!", "Palm OS 4.1 메모장", "다람쥐 헌 쳇바퀴에 타고파"],
                       keyboard('han'), cursor=3)),
     "01-overview.png",
     ["Graffiti 영역 대신 320×130 고해상도 키보드를 그립니다.",
      "상태바 □→ 아이콘으로 기본 실크와 한글 키보드를 바꿉니다."])

# 2. 단모음 자판
card("단모음 한글 자판",
     "자음 14키 + 모음 8키 · Shift로 쌍자음",
     stack(frame(keyboard('han')), frame(keyboard('han', shift=True)), label=["기본", "Shift (^) — ㅃ ㅉ ㄸ ㄲ ㅆ"]),
     "02-hangul-layout.png",
     ["ㅏ·ㅓ·ㅗ·ㅜ를 한 번 더 누르면 ㅑ·ㅕ·ㅛ·ㅠ, ㅐ·ㅔ는 ㅒ·ㅖ가 됩니다."])

# 3. 영문·숫자
card("영문 QWERTY · 숫자/기호",
     "한/A로 한·영 전환, 123으로 숫자·기호",
     stack(frame(keyboard('eng')), frame(keyboard('num')), label=["영문 (한/A)", "숫자·기호 (123)"]),
     "03-english-number.png")

# 4. 조합 (test/compose_test.py에서 통과하는 입력만 사용)
rows4 = [("ㄷ", "ㄷ"), ("ㄷㅏ", "다"), ("ㄷㅏㄹ", "달"), ("ㄷㅏㄹㄱ", "닭"), ("ㄷㅏㄹㄱㅣ", "달기"),
         ("ㄷㅏㄹㄱ BS", "달"), ("ㄱㅗㅏ", "과"), ("ㅇㅏㅏ", "야"), ("ㄱㅏㅂㅅ", "값")]
cell = Image.new('RGB', (320, 16 + 30 * len(rows4)), PAPER)
dd = ImageDraw.Draw(cell)
for i, (keys, res) in enumerate(rows4):
    y = 8 + i * 30
    x = 8
    for k in keys.split(' ')[0]:
        round_rect(dd, (x, y, x + 22, y + 24))
        draw_jamo(dd, k, x + 5, y + 6, INK)
        x += 25
    if keys.endswith('BS'):
        round_rect(dd, (x, y, x + 30, y + 24))
        text(cell, x + 6, y + 1, "BS")
        x += 33
    ax = max(x + 6, 150)                      # 화살표(픽셀 글꼴에 → 가 없음)
    dd.line((ax, y + 12, ax + 16, y + 12), fill=INK, width=2)
    dd.polygon([(ax + 18, y + 12), (ax + 12, y + 7), (ax + 12, y + 17)], fill=INK)
    text(cell, max(x + 6, 150) + 26, y + 1, res)
card("받침 넘김 · 자모 단위 지우기",
     "초성·중성·종성, 복합모음, 겹받침, 모음 연타",
     frame(cell),
     "04-compose.png",
     ["받침 뒤에 모음이 오면 마지막 자음이 다음 글자로 넘어갑니다(닭 + ㅣ → 달기).",
      "조합 중 BS는 자모 하나씩 지웁니다(닭 → 달)."])

# 5. CJKOS 없이 (시험 중)
lines12 = Image.new('RGB', (320, 140), PAPER)
text(lines12, 6, 6, "12×12 키보드 저해상도", size=12, line_h=16)
text(lines12, 6, 34, "16×16 메모 일반·볼드", size=16, line_h=22)
text(lines12, 6, 70, "24×24 큰 글자", size=24, line_h=30)
text(lines12, 6, 104, "닭 값 읽 넓 괜찮아요", size=16, line_h=22)
card("CJKOS 없이 한글 표시 (시험 중)",
     "MnHangul: 조합형 글꼴 12·16·24 + 글자 경계·폭 처리",
     frame(lines12),
     "05-mnhangul.png",
     ["Neo둥근모·물마루 기반 조합형 글꼴(OFL). 24×24는 추가 설치 글꼴입니다.",
      "기기 입력은 KS X 1001 완성형 2,350자 범위입니다."])
