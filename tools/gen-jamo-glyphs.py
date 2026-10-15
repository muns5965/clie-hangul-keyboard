#!/usr/bin/env python3
"""자판 키 이름용 자모 선(벡터) 글꼴 생성 → src/skwr/jamo_glyphs.h

각 자모는 12x12 격자 위 선분 목록. 본체가 고해상도(HR) 자판을 그릴 때 HRWinDrawLine으로 그리고,
선 굵기 2픽셀(가로에 가까운 선은 y+1, 세로에 가까운 선은 x+1로 한 번 더).
형식(.text, PC 상대로 읽음):
  JamoOff: .word × 52  호환 자모 번호 o(유니코드-0x3130, 0~51)별 JamoSeg 안 오프셋, 없으면 0xFFFF
  JamoSeg: 자모마다 [선분 수 1바이트] + 선분당 2바이트 (x1<<4|y1, x2<<4|y2)
미리보기: python3 tools/gen-jamo-glyphs.py --preview  → build/jamo_preview.png (Pillow 필요)
"""
import os, sys
def poly(*pts, close=False):
    segs=[(pts[i][0],pts[i][1],pts[i+1][0],pts[i+1][1]) for i in range(len(pts)-1)]
    if close: segs.append((pts[-1][0],pts[-1][1],pts[0][0],pts[0][1]))
    return segs
def sh(segs,dx): return [(a+dx,b,c+dx,d) for a,b,c,d in segs]
OCT=lambda x0,y0,x1,y1: poly((x0+2,y0),(x1-2,y0),(x1,y0+2),(x1,y1-2),(x1-2,y1),(x0+2,y1),(x0,y1-2),(x0,y0+2),close=True)
G={}
G['ㄱ']=poly((1,1),(10,1),(10,11))
G['ㄲ']=poly((0,1),(5,1),(5,11))+poly((6,1),(11,1),(11,11))
G['ㄴ']=poly((2,0),(2,10),(11,10))
G['ㄷ']=poly((10,1),(1,1),(1,10),(10,10))
G['ㄸ']=poly((5,1),(0,1),(0,10),(5,10))+poly((11,1),(6,1),(6,10),(11,10))
G['ㄹ']=poly((1,1),(10,1),(10,5),(1,5),(1,10),(10,10))
G['ㅁ']=poly((1,1),(10,1),(10,10),(1,10),close=True)
G['ㅂ']=poly((1,0),(1,10),(10,10),(10,0))+[(1,5,10,5)]
G['ㅃ']=poly((0,0),(0,10),(5,10),(5,0))+[(0,5,5,5)]+sh(poly((0,0),(0,10),(5,10),(5,0))+[(0,5,5,5)],6)
G['ㅅ']=[(5,1,1,10),(5,4,10,10)]
G['ㅆ']=[(3,1,0,10),(3,4,5,10),(8,1,6,10),(8,4,11,10)]
G['ㅇ']=OCT(1,1,10,10)
G['ㅈ']=[(1,1,10,1),(6,1,1,10),(6,5,10,10)]
G['ㅉ']=[(0,1,5,1),(3,1,0,10),(3,5,5,10),(6,1,11,1),(9,1,6,10),(9,5,11,10)]
G['ㅊ']=[(5,0,5,1),(1,3,10,3),(6,3,1,11),(6,6,10,11)]
G['ㅋ']=poly((1,1),(10,1),(10,11))+[(1,6,10,6)]
G['ㅌ']=poly((10,1),(1,1),(1,10),(10,10))+[(1,5,10,5)]
G['ㅍ']=[(0,1,11,1),(0,10,11,10),(3,1,3,10),(8,1,8,10)]
G['ㅎ']=[(4,0,7,0),(0,2,11,2)]+OCT(2,4,9,11)
G['ㅏ']=[(4,0,4,11),(4,5,8,5)]
G['ㅐ']=[(3,0,3,11),(3,5,6,5),(8,0,8,11)]
G['ㅓ']=[(7,0,7,11),(3,5,7,5)]
G['ㅔ']=[(5,0,5,11),(2,5,5,5),(9,0,9,11)]
G['ㅗ']=[(5,4,5,9),(0,9,11,9)]
G['ㅜ']=[(0,3,11,3),(5,3,5,9)]
G['ㅡ']=[(0,6,11,6)]
G['ㅣ']=[(5,0,5,11)]


root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def build():
    off = [0xFFFF] * 52
    seg = bytearray()
    for ch, segs in G.items():
        o = ord(ch) - 0x3130
        assert 1 <= o <= 51, ch
        off[o] = len(seg)
        seg.append(len(segs))
        for a, b, c, d in segs:
            assert all(0 <= v <= 15 for v in (a, b, c, d)), (ch, a, b, c, d)
            seg += bytes(((a << 4) | b, (c << 4) | d))
    if len(seg) % 2:
        seg.append(0)
    lines = ['/* tools/gen-jamo-glyphs.py 가 생성. 직접 고치지 말 것 */',
             'asm(".text\\n.even\\nJamoOff:\\n"']
    for k in range(0, 52, 13):
        lines.append('    " .word ' + ','.join('0x%04x' % v for v in off[k:k + 13]) + '\\n"')
    lines.append('    "JamoSeg:\\n"')
    for k in range(0, len(seg), 16):
        lines.append('    " .byte ' + ','.join('0x%02x' % v for v in seg[k:k + 16]) + '\\n"')
    lines.append('    ".even\\n");')
    with open(os.path.join(root, 'src/skwr/jamo_glyphs.h'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print('ok', len(G), 'glyphs,', len(seg), 'bytes')

def preview():
    from PIL import Image, ImageDraw
    im = Image.new('1', (14 * 9 + 2, 14 * 3 + 2), 1)
    d = ImageDraw.Draw(im)
    for i, (ch, segs) in enumerate(G.items()):
        ox, oy = 2 + (i % 9) * 14, 2 + (i // 9) * 14
        for a, b, c, e in segs:
            d.line((ox + a, oy + b, ox + c, oy + e), fill=0)
            if abs(c - a) >= abs(e - b):
                d.line((ox + a, oy + b + 1, ox + c, oy + e + 1), fill=0)
            else:
                d.line((ox + a + 1, oy + b, ox + c + 1, oy + e), fill=0)
    os.makedirs(os.path.join(root, 'build'), exist_ok=True)
    p = os.path.join(root, 'build/jamo_preview.png')
    im.resize((im.width * 4, im.height * 4), Image.NEAREST).save(p)
    print('preview', p)

build()
if '--preview' in sys.argv:
    preview()
