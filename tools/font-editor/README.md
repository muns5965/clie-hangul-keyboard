# Neo둥근모 24 편집기

Neo둥근모(OFL 1.1) 16×16 한글 조각(8×4×4 벌식: 초성 19×8벌, 중성 21×4벌, 종성 27×4벌 = 344개)을
24×24로 자동 확대하고, 조각 단위로 고치면서 조합된 글자를 바로 확인하는 웹 페이지.

| 파일 | 내용 |
| --- | --- |
| `editor.html` | 페이지 틀(데이터 자리 `/*DATA*/null`) |
| `build.py` | 글꼴에서 조각 344개(16×16)와 음절 11,172자의 조각 번호를 뽑아 `index.html` 생성 |
| `index.html` | 생성된 페이지(Neo둥근모 파생 데이터 포함, OFL 1.1 — `src/hangul/data/NEODGM-OFL.md`) |

```bash
python3 tools/font-editor/build.py <neodgm.ttf>   # npm @kfonts/neodgm 0.5.0 의 src/neodgm.ttf
```

- 자동 확대: 가로·세로를 따로 늘리며, 짧은 구간(획 굵기)은 정해진 굵기로, 긴 구간(획 길이)은 1.5배로 늘린다.
  방식 4가지(획 비례 / 가는 획 / 중간 / 단순 1.5배) 중 고른다.
- 저장: claude.ai 아티팩트의 db(`pieces/<조각 이름>` = `{bits: 24줄 × 6자리 16진수}`, 고친 조각만)와 브라우저.
- 내보내기 JSON(`format: mnhangul-hangul-pieces`): 조각 344개 전체 24×24(16진수, 줄마다 6자리, 왼쪽이 MSB),
  `edited`(손으로 고친 조각 이름), `method`(자동 확대 방식). 기기용 데이터는 이 JSON에서 만든다.
- 결과물은 Neo둥근모의 파생 글꼴이므로 OFL 1.1을 따르고, 이름은 Neo둥근모를 쓰지 않는다.
