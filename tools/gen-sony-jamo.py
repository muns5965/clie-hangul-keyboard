#!/usr/bin/env python3
"""MnSonyKbd 키 글자 자모 그림 → src/sony/data/SyJm0000.bin

MnHangul 16×16 조합형 묶음(src/hangul/data/HgJ16.bin, tools/gen-johab.py, OFL)에서 자모표만 떼어 낸다.
형식: 묶음 머리 126바이트를 그대로 두고 124~125(자모표 위치)만 126으로 바꾼 뒤, 자모표(KS X 1001 0xA4A1~0xA4FE
94자 × 16줄 × 2바이트)를 붙인다. 오버레이(src/sony/spatch.c)는 머리의 판(2)·높이(3)·자모표 위치(124)만 읽는다.
(나머지 머리 값은 원래 묶음 기준이라 이 파일에서는 쓰지 않는다.)

사용: python3 tools/gen-sony-jamo.py
"""
import os

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
pk = open(os.path.join(root, 'src/hangul/data/HgJ16.bin'), 'rb').read()
ver, H = pk[2], pk[3]
w = 4 if ver >= 2 else 2
off = int.from_bytes(pk[124:126], 'big')
size = 94 * H * w
table = pk[off:off + size]
assert len(table) == size, (off, size, len(pk))
hdr = bytearray(pk[:126])
hdr[124:126] = (126).to_bytes(2, 'big')
out = os.path.join(root, 'src/sony/data/SyJm0000.bin')
open(out, 'wb').write(bytes(hdr) + table)
print(out, 126 + size)
