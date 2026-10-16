#!/usr/bin/env python3
"""24×24 조합형 한글 묶음 생성 → src/hangul/data/HgJ24.bin

입력: 편집기(tools/font-editor) 내보내기 JSON(format mnhangul-hangul-pieces, size 24, 조각 344개)과
      16×16 묶음 HgJ16.bin(같은 Neo둥근모 8×4×4 벌 규칙·자모표를 쓴다).
결과는 Neo둥근모(OFL 1.1) 파생 데이터다. 이름에 "Neo둥근모"를 쓰지 않는다.

형식은 gen-johab.py의 묶음과 같고, 다른 점만:
  2 판 = 2 (그림 한 줄 = 4바이트, 왼쪽이 비트 31. 24칸이면 비트 31~8)
  3 칸 높이 24, 4 글자 폭 24, 5 기준선 = JSON baseline(18)
  조각 순서·규칙·벌 수는 HgJ16.bin과 같다(조각 번호 = 초 벌×19+L, 중 Nc×19+벌×21+V, 종 Nc×19+Nj×21+벌×27+T−1).
  자모표(KS X 1001 0xA4A1~0xA4FE 94칸, 그림은 현대 자모 0xA4A1~0xA4D3 = U+3131~3163 51자)는 글자마다
    ① 조각 JSON의 'jamo'(글자 → 24줄 16진수, 편집기가 넣음) → ② src/hangul/data/hangul24-jamo.json(손그림 글자만)
    → ③ 지금 조각으로 만들기(자음 = cho_1_xx, 겹받침 = jong_1_xx, 모음 = jung_0_xx — 편집기 화면 표시로는
       초성 2벌·종성 2벌·중성 1벌 — 을 24칸 가운데로:
       가로 시작 = int((24 − 폭) / 2 + 0.5), 세로 시작 = (24 − 높이) // 2) 순서로 정한다.
    조각을 고치면 ③ 글자는 따라 바뀐다. 나머지 43칸(옛한글·채움)은 빈칸.
검사: 11,172자 전부, 조각 이름으로 직접 겹친 그림과 기기 방식(규칙→조각 번호) 조합 결과가 같아야 한다.
      낱자모 51자 모두 그림이 있고 서로 달라야 한다(③ 글자는 매번 조각에서 만든다).

사용: python3 tools/gen-johab24.py <neodgm24-pieces.json> [미리보기.png]
"""
import json
import os
import struct
import sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S = 24

def main():
    js = json.load(open(sys.argv[1], encoding='utf-8'))
    assert js['format'] == 'mnhangul-hangul-pieces' and js['size'] == S, 'not a 24×24 piece export'
    p16 = open(os.path.join(root, 'src/hangul/data/HgJ16.bin'), 'rb').read()
    assert p16[:2] == b'HJ' and p16[3] == 16
    Nc, Nj, Nt = p16[6], p16[7], p16[8]
    o_p, n_p, o_e, n_e, o_s, n_s, o_j, _ = struct.unpack_from('>8H', p16, 112)
    assert n_e == 0 and n_s == 0 and n_p == Nc * 19 + Nj * 21 + Nt * 27

    def pid(name):
        kind, st, jm = name.split('_')
        st, jm = int(st), int(jm)
        if kind == 'cho':
            return st * 19 + jm
        if kind == 'jung':
            return Nc * 19 + st * 21 + jm
        return Nc * 19 + Nj * 21 + st * 27 + (jm - 1)

    pieces = [None] * n_p
    by_name = {}
    for name, hx in zip(js['names'], js['pieces']):
        assert len(hx) == S * 6
        rows = [int(hx[i * 6:i * 6 + 6], 16) << 8 for i in range(S)]
        by_name[name] = rows
        pieces[pid(name)] = rows
    assert all(p is not None for p in pieces), 'missing pieces'

    jamo, src = jamo_table(js, by_name)

    out = bytearray(b'HJ' + bytes([2, S, S, js['baseline'], Nc, Nj, Nt, 0]))
    out += p16[10:112]
    hdr = len(out)
    out += bytes(16)
    rb = lambda rows: b''.join(struct.pack('>I', r) for r in rows)
    o_p2 = len(out)
    for r in pieces:
        out += rb(r)
    o_x = len(out)
    o_j2 = len(out)
    for r in jamo:
        out += rb(r)
    struct.pack_into('>8H', out, hdr, o_p2, n_p, o_x, 0, o_x, 0, o_j2, 0)
    assert len(out) < 65000

    # 검사: 기기 방식(규칙) 조합 == 조각 이름으로 직접 겹친 그림
    bad = 0
    names = {pid(n): n for n in by_name}
    for s in range(11172):
        L, V, T = s // 588, (s % 588) // 28, s % 28
        ids = [out[10 + V * 2 + (T > 0)] * 19 + L, Nc * 19 + out[52 + L * 2 + (T > 0)] * 21 + V]
        if T:
            ids.append(Nc * 19 + Nj * 21 + out[90 + V] * 27 + T - 1)
        a = [0] * S
        for i in ids:
            pr = struct.unpack_from('>%dI' % S, out, o_p2 + i * S * 4)
            a = [x | y for x, y in zip(a, pr)]
        b = [0] * S
        for i in ids:
            b = [x | y for x, y in zip(b, by_name[names[i]])]
        bad += a != b
    print('HgJ24.bin: %d bytes, pieces %d, edited %d, method %s, jamo %d = json %d + hand %d + pieces %d, mismatches %d'
          % (len(out), n_p, len(js.get('edited', [])), js.get('method'), sum(any(j) for j in jamo),
             src.count('json'), src.count('hand'), src.count('pieces'), bad))
    if bad:
        sys.exit(1)
    open(os.path.join(root, 'src/hangul/data/HgJ24.bin'), 'wb').write(out)
    if len(sys.argv) > 2:
        preview(bytes(out), sys.argv[2])


CHO = "ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ"
JUNG = "ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ"
JONG = "ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ"   # T-1


def hex_rows(hx, what):
    assert isinstance(hx, str) and len(hx) == S * 6, 'bad jamo rows: ' + what
    return [int(hx[i * 6:i * 6 + 6], 16) << 8 for i in range(S)]


def jamo_from_pieces(c, by_name):
    """낱자모 c를 지금 조각으로: 자음 = cho_1_xx(초성에 없는 겹받침은 jong_1_xx), 모음 = jung_0_xx, 24칸 가운데로"""
    if c in JUNG:
        n = 'jung_0_%02d' % JUNG.index(c)
    elif c in CHO:
        n = 'cho_1_%02d' % CHO.index(c)
    else:
        n = 'jong_1_%02d' % (JONG.index(c) + 1)
    rows = by_name[n]
    ys = [y for y in range(S) if rows[y]]
    assert ys, 'empty piece ' + n
    m = 0
    for r in rows:
        m |= r
    xs = [x for x in range(S) if (m >> (31 - x)) & 1]
    dx = int((S - (xs[-1] - xs[0] + 1)) / 2 + 0.5) - xs[0]       # JS Math.round와 같게(0.5는 올림)
    dy = (S - (ys[-1] - ys[0] + 1)) // 2 - ys[0]
    out = [0] * S
    for y in ys:
        r = rows[y] >> dx if dx >= 0 else rows[y] << -dx
        out[y + dy] = r & 0xFFFFFF00
    return out


def jamo_table(js, by_name):
    """자모표 94칸과 51자의 출처 목록. ① 조각 JSON 'jamo' → ② 손그림 파일 → ③ 조각에서 만들기"""
    hand = {}
    hp = os.path.join(root, 'src/hangul/data/hangul24-jamo.json')
    if os.path.exists(hp):
        hj = json.load(open(hp, encoding='utf-8'))
        assert hj.get('format') == 'mnhangul-jamo' and hj.get('size') == S, 'not a 24×24 jamo file: ' + hp
        hand = hj['glyphs']
    over = js.get('jamo') or {}
    table, src, used = [[0] * S for _ in range(94)], [], []
    for k in range(51):
        c = chr(0x3131 + k)
        kc = c.encode('euc_kr')[1] - 0xA1
        if c in over:
            table[kc], how = hex_rows(over[c], c), 'json'
        elif c in hand:
            table[kc], how = hex_rows(hand[c], c), 'hand'
        else:
            table[kc], how = jamo_from_pieces(c, by_name), 'pieces'
        assert any(table[kc]), 'empty jamo ' + c
        src.append(how)
        used.append(kc)
    for c in list(over) + list(hand):
        assert c in [chr(0x3131 + k) for k in range(51)], 'unknown jamo key ' + c
    assert len(set(used)) == 51 and len({tuple(table[k]) for k in used}) == 51, 'duplicate jamo shapes'
    return table, src


def compose(pk, s):
    S_ = pk[3]
    Nc, Nj = pk[6], pk[7]
    o_p = struct.unpack_from('>H', pk, 112)[0]
    L, V, T = s // 588, (s % 588) // 28, s % 28
    ids = [pk[10 + V * 2 + (T > 0)] * 19 + L, Nc * 19 + pk[52 + L * 2 + (T > 0)] * 21 + V]
    if T:
        ids.append(Nc * 19 + Nj * 21 + pk[90 + V] * 27 + T - 1)
    a = [0] * S_
    for i in ids:
        a = [x | y for x, y in zip(a, struct.unpack_from('>%dI' % S_, pk, o_p + i * S_ * 4))]
    return a


def preview(pk, path):
    from PIL import Image
    lines = ['다람쥐 헌 쳇바퀴에 타고파', '키스의 고유조건은 입술끼리', '똠방각하 펲시 뷁 쓁 읢 갉',
             '가나다라마바사아자차카타파하']
    o_j = struct.unpack_from('>H', pk, 124)[0]
    jl = 'ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ ㅏㅐㅓㅔㅗㅘㅜㅝㅡㅢㅣ'
    rows_txt = lines + [jl]
    cw, sc = 26, 3
    W = max(len(t) for t in rows_txt) * cw * sc + 20
    img = Image.new('L', (W, len(rows_txt) * 28 * sc + 20), 255)
    px = img.load()
    for li, t in enumerate(rows_txt):
        for ci, ch in enumerate(t):
            c = ord(ch)
            if 0xAC00 <= c <= 0xD7A3:
                g = compose(pk, c - 0xAC00)
            elif ch != ' ':
                k = ch.encode('euc_kr')[1] - 0xA1
                g = struct.unpack_from('>24I', pk, o_j + k * 96)
            else:
                continue
            for y in range(24):
                for x in range(24):
                    if (g[y] >> (31 - x)) & 1:
                        for dy in range(sc):
                            for dx in range(sc):
                                px[10 + (ci * cw + 1 + x) * sc + dx, 10 + (li * 28 + 2 + y) * sc + dy] = 0
    img.save(path)


main()
