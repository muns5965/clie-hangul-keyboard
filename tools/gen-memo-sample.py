#!/usr/bin/env python3
"""한글 견본 메모 DB(MemoDB.pdb, EUC-KR) 만들기 — 입력기가 없는 기종(T600C 등)에서 MnHangul 표시만 시험할 때.

설치하면 그 세션의 메모장 DB(MemoDB)를 통째로 바꾸므로 백업 세션에서만 쓴다.
사용: python3 tools/gen-memo-sample.py [출력 경로]   (기본 build/MemoDB.pdb)
"""
import struct
import sys
import time

MEMOS = [
    "대한민국",
    "가나다 한글",
    "긴밤 달가 값읽 와의왜",
    "ABC 한글 mixed 123",
    "뽐뿌질 꽃밭 읽기 닭",
    "한글 줄바꿈 시험: 동해물과 백두산이 마르고 닳도록 하느님이 보우하사 우리나라 만세",
    "1줄\n2줄 한글\n3줄 끝",
]

PALM_EPOCH = 2082844800  # 1904-01-01 → 1970-01-01 초


def appinfo():
    labels = ["Unfiled", "Business", "Personal"] + [""] * 13
    b = struct.pack(">H", 0)
    for s in labels:
        b += s.encode("ascii").ljust(16, b"\0")
    b += bytes(range(16))          # 분류 고유 번호
    b += bytes([15, 0])            # lastUniqID, reserved
    b += bytes([0, 0])             # 메모 앱 정보: reserved, sortOrder
    return b


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "build/MemoDB.pdb"
    recs = [m.encode("cp949") + b"\0" for m in MEMOS]
    for m in MEMOS:   # KS X 1001 완성형 2,350자만(cp949 확장·EUC-KR 8바이트 조합 표현 금지 — 똠·쀍 등)
        for ch in m:
            if ord(ch) >= 0xAC00:
                b = ch.encode("euc-kr")
                assert len(b) == 2 and 0xB0 <= b[0] <= 0xC8, (ch, b.hex())
    now = int(time.time()) + PALM_EPOCH
    n = len(recs)
    hdr_len = 78 + 8 * n + 2
    ai = appinfo()
    off = hdr_len + len(ai)
    head = b"MemoDB".ljust(32, b"\0")
    head += struct.pack(">HHIIIIIIII", 0x0008, 0, now, now, 0, 0, hdr_len, 0, 0, 0)
    head = head[:60] + b"DATA" + b"memo" + struct.pack(">IIH", 0, 0, n)
    # 위 pack에서 type/creator 자리(8바이트)는 0으로 채웠다가 덮어씀
    lst = b""
    for i, r in enumerate(recs):
        lst += struct.pack(">IB", off, 0x40) + (0x500001 + i).to_bytes(3, "big")
        off += len(r)
    data = head + lst + b"\0\0" + ai + b"".join(recs)
    assert len(head) == 78
    with open(out, "wb") as f:
        f.write(data)
    print(out, len(data), "bytes,", n, "memos")


if __name__ == "__main__":
    main()
