#!/usr/bin/env python3
"""skwr.c 조합 로직 시험: 키 순서 → 메모 본문(문자열) 비교. tools/test-compose.sh 가 실행."""
import ctypes, sys, os
lib = ctypes.CDLL(sys.argv[1])
lib.T_Reset.restype = None
lib.T_Text.restype = ctypes.c_char_p

# gen-ksc-table.py 와 같은 표를 만들어 넣는다
bits = bytearray(1397); cum = bytearray(88); n = 0
for i in range(11172):
    if i % 256 == 0:
        cum[(i >> 8) * 2] = n >> 8; cum[(i >> 8) * 2 + 1] = n & 0xFF
    if len(chr(0xAC00 + i).encode('euc_kr', 'replace')) == 2:
        bits[i >> 3] |= 0x80 >> (i & 7); n += 1
lib.T_Load(bytes(bits), bytes(cum))

# 한글 자판 키 번호(줄*16+칸+1)
HAN = {}
for row, keys in enumerate(["ㅂㅈㄷㄱㅅㅗㅐㅔ", "ㅁㄴㅇㄹㅎㅓㅏㅣ", "^ㅋㅌㅊㅍㅜㅡ<"]):
    for col, ch in enumerate(keys):
        HAN[ch] = row * 16 + col + 1
HAN[' '] = 3 * 16 + 3 + 1
HAN['.'] = 3 * 16 + 4 + 1
SHIFT_OF = {'ㅃ': 'ㅂ', 'ㅉ': 'ㅈ', 'ㄸ': 'ㄷ', 'ㄲ': 'ㄱ', 'ㅆ': 'ㅅ'}

def run(keys):
    lib.T_Reset()
    for ch in keys:
        if ch in SHIFT_OF:
            lib.T_Key(HAN['^']); lib.T_Key(HAN[SHIFT_OF[ch]])
        else:
            lib.T_Key(HAN[ch])
    return lib.T_Text().decode('euc_kr')

cases = [
    ("ㄱㅏ", "가"), ("ㄱㅏㄱ", "각"), ("ㄱㅏㄱㅏ", "가가"),
    ("ㅎㅏㄴㄱㅡㄹ", "한글"), ("ㄷㅏㄹㄱㅏ", "달가"), ("ㄷㅏㄹㄱ", "닭"),
    ("ㄷㅏㄹㄱㅇㅣ", "닭이"), ("ㄷㅏㄹㄱㅣ", "달기"),
    ("ㄱㅗㅏ", "과"), ("ㅇㅜㅣ", "위"), ("ㅇㅡㅣ", "의"), ("ㄱㅗㅐ", "괘"), ("ㅇㅜㅔ", "웨"),
    ("ㅇㅏㅏ", "야"), ("ㅇㅓㅓ", "여"), ("ㅇㅗㅗ", "요"), ("ㅇㅜㅜ", "유"), ("ㅇㅐㅐ", "얘"), ("ㅇㅔㅔ", "예"),
    ("ㄲㅏ", "까"), ("ㅆㅏㄹ", "쌀"), ("ㅃㅏㅇ", "빵"), ("ㄸㅏㄸ", "따ㄸ"),
    ("ㄱㅏ<", "ㄱ"), ("ㄱㅏㄱ<", "가"), ("ㄱㅏㄱ<<<", ""), ("ㄱㅏ ㄴ<", "가 "),
    ("ㄱㅗㅏ<", "고"), ("ㅇㅏㅏ<", "아"), ("ㄷㅏㄹㄱ<", "달"),
    ("ㅏ", "ㅏ"), ("ㅏㄱ", "ㅏㄱ"), ("ㄱㄴ", "ㄱㄴ"), ("ㄱㅏ.ㅏ", "가.ㅏ"),
    ("ㄱㅏㅂㅅ", "값"), ("ㄱㅏㅂㅅㅇㅣ", "값이"), ("ㄱㅏㅂㅅㅣ", "갑시"),
    ("ㄷㅗㅁ", "돔"),
    ("ㄸㅗㅁ", "또ㅁ"),          # 똠은 KSC5601에 없음 → 또 확정 + ㅁ
    ("ㅁㅜㅓㄹ", "뭘"), ("ㅅㅏ ㄹㅏㅇ", "사 랑"),
]
fix = {}
bad = 0
for keys, want in cases:
    want = fix.get(keys, want)
    got = run(keys)
    ok = got == want
    bad += not ok
    print(("ok  " if ok else "FAIL"), keys, "->", repr(got), "" if ok else "want %r" % want)
print("fail:", bad)
sys.exit(1 if bad else 0)
