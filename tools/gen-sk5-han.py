#!/usr/bin/env python3
"""MnSk5 한/A 키의 '한' 그림 → src/skwr5/data/Sk5Han.bin (16×16, 1비트, 줄마다 2바이트, 왼쪽이 MSB, 32바이트)

MnHangul 16×16 조합형 묶음(src/hangul/data/HgJ16.bin, tools/gen-johab.py, Neo둥근모 기반 OFL)의 조각을
묶음 규칙대로 겹쳐 음절 '한'을 만든다(예외표·특수 글자표도 확인). 조합형 묶음 형식은 tools/gen-johab.py 머리말.

사용: python3 tools/gen-sk5-han.py
"""
import os

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
pk = open(os.path.join(root, 'src/hangul/data/HgJ16.bin'), 'rb').read()
be = lambda o, n=2: int.from_bytes(pk[o:o + n], 'big')
ver, H, W = pk[2], pk[3], pk[4]
w = 4 if ver >= 2 else 2
Nc, Nj, Nt = pk[6], pk[7], pk[8]
assert H == 16 and w == 2, (ver, H, w)
poff, pcnt = be(112), be(114)
eoff, ecnt = be(116), be(118)
soff, scnt = be(120), be(122)


def piece(i):
    o = poff + i * H * w
    return [be(o + r * w, w) for r in range(H)]


def glyph(L, V, T):
    s = (L * 21 + V) * 28 + T
    for k in range(scnt):   # 특수 글자: 음절 번호 + 그림
        o = soff + k * (2 + H * w)
        if be(o) == s:
            return [be(o + 2 + r * w, w) for r in range(H)]
    has = 1 if T else 0
    c = Nc and pk[10 + (V * 2 + has)]
    j = pk[52 + (L * 2 + has)]
    t = pk[90 + V]
    ids = [c * 19 + L, Nc * 19 + j * 21 + V]
    if T:
        ids.append(Nc * 19 + Nj * 21 + t * 27 + (T - 1))
    for k in range(ecnt):   # 예외표: 음절 번호, 초·중·종 조각 번호
        o = eoff + k * 8
        if be(o) == s:
            ids = [x for x in (be(o + 2), be(o + 4), be(o + 6)) if x != 0xFFFF]
    rows = [0] * H
    for i in ids:
        rows = [a | b for a, b in zip(rows, piece(i))]
    return rows


CHO = "ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ"
JUNG = "ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ"
JONG = "ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ"
rows = glyph(CHO.index('ㅎ'), JUNG.index('ㅏ'), JONG.index('ㄴ') + 1)
assert any(rows)
for r in rows:
    print(''.join('#' if r & (0x8000 >> c) else '.' for c in range(16)))
out = os.path.join(root, 'src/skwr5/data/Sk5Han.bin')
open(out, 'wb').write(b''.join(r.to_bytes(2, 'big') for r in rows))
print(out, 32)
