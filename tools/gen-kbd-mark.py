#!/usr/bin/env python3
"""시스템 키보드 화면(MnKbdHan) 한/영 표시 → src/kbdhan/data/KhIc0000.bin('한')·KhIc0001.bin('A')

MnSonyKbd 표시 그림(src/sony/data/SyIc0002·0003.bin, tools/gen-tab-mark.py, Galmuri11)과 같은 글자 모양을
흰 바탕 키보드 화면에 맞게 글자 색만 검정(색 번호 0xFF)으로 바꾼다. 14×18 8비트, 투명 색 0xD8(머리 그대로).

사용: python3 tools/gen-kbd-mark.py
"""
import os

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.makedirs(os.path.join(root, 'src/kbdhan/data'), exist_ok=True)
for src, dst in ((2, 0), (3, 1)):
    d = open(os.path.join(root, 'src/sony/data', 'SyIc%04d.bin' % src), 'rb').read()
    hdr, px = d[:16], d[16:]
    out = os.path.join(root, 'src/kbdhan/data', 'KhIc%04d.bin' % dst)
    open(out, 'wb').write(hdr + bytes(0xFF if p == 0x00 else p for p in px))
    print(out, len(d))
