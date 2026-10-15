#!/bin/bash
# 복구용 ClieDiag → bin/DiagRescue.prc
#
# DisS 스위치를 켜면 실크 영역(홈 아이콘 포함)이 사라져 런처를 열 수 없다.
# 이때 하드웨어 To Do 버튼으로 진단 앱을 열 수 있도록, ClieDiag와 같은 코드를
# 크리에이터 'todo', 버전 2로 빌드한다. ROM의 To Do List는 버전 1이므로
# 버튼을 누르면 더 높은 버전인 이 앱이 실행된다. To Do 데이터(ToDoDB)는 건드리지 않는다.
# 시험이 끝나면 이 앱을 지우면 원래 To Do가 돌아온다.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$root/build/rescue" "$root/bin"
cd "$root/build/rescue"

m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar \
  "$root/src/diag/diag.c" -o diag 2>&1 | tee build.log
m68k-palmos-obj-res diag
build-prc -v 2 "$root/bin/DiagRescue.prc" "DiagRescue" todo *.diag.grc
ls -l "$root/bin/DiagRescue.prc"
