#!/bin/bash
# PRC(리소스 DB) 헤더와 리소스 목록 출력
# Palm OS PDB/PRC 헤더 고정 오프셋(빅엔디언) 기준으로 파싱한다.
#   0  name[32]      32 attributes(2)  34 version(2)
#   60 type[4]       64 creator[4]     76 numRecords(2)
#   78~ 리소스 항목 {type[4], id(2), offset(4)} x numRecords
# 사용법: tools/prc-info.sh file.prc [...]
set -euo pipefail

u16() { od -An -t u2 --endian=big -j "$2" -N 2 "$1" | tr -d ' '; }
u32() { od -An -t u4 --endian=big -j "$2" -N 4 "$1" | tr -d ' '; }
str() { dd if="$1" bs=1 skip="$2" count="$3" 2>/dev/null | tr -d '\0'; }

for f in "$@"; do
    size=$(stat -c %s "$f")
    name=$(str "$f" 0 32)
    attr=$(u16 "$f" 32)
    ver=$(u16 "$f" 34)
    type=$(str "$f" 60 4)
    creator=$(str "$f" 64 4)
    n=$(u16 "$f" 76)

    if (( (attr & 1) == 0 )); then
        echo "$f: not a resource database (attr=$attr)" >&2
        exit 1
    fi

    res=""
    for ((i = 0; i < n; i++)); do
        o=$((78 + i * 10))
        rt=$(str "$f" "$o" 4)
        rid=$(u16 "$f" $((o + 4)))
        off=$(u32 "$f" $((o + 6)))
        if (( i + 1 < n )); then
            next=$(u32 "$f" $((o + 16)))
        else
            next=$size
        fi
        res+=" ${rt}${rid}(${off}+$((next - off)))"
    done

    echo "$(basename "$f"): name='$name' type=$type creator=$creator ver=$ver size=$size resources=$n:$res"
done
