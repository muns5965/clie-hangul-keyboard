#!/bin/bash
# Palm OS 68K 개발 도구 체인 설치 스크립트 (Ubuntu 24.04 x86_64에서 검증)
#
# 설치 항목:
#   - prc-tools-remix (m68k-palmos-gcc 2.95.3, build-prc 등)  https://github.com/jichu4n/prc-tools-remix
#   - Palm OS SDK 4.0                                          https://github.com/jichu4n/palm-os-sdk
#   - PilRC (리소스 컴파일러)                                   https://github.com/jichu4n/pilrc
#
# 사용법: sudo ./tools/setup-toolchain.sh [작업디렉터리]
# 소요 시간: 컴파일러 빌드에 수십 분

set -euo pipefail

work="${1:-$HOME/palm-toolchain-src}"
mkdir -p "$work"
cd "$work"

# 1. 빌드 의존성
apt-get install -y build-essential autoconf automake libncurses-dev flex bison texinfo gperf

# 2. 소스 받기
[ -d prc-tools-remix ] || git clone --depth 1 https://github.com/jichu4n/prc-tools-remix
[ -d palm-os-sdk ]     || git clone --depth 1 https://github.com/jichu4n/palm-os-sdk
[ -d pilrc ]           || git clone --depth 1 https://github.com/jichu4n/pilrc

# 3. 컴파일러 빌드 (m68k 대상만)
#    주의: make 병렬(-j2 이상)로 돌리면 cc1plus 생성 전에 libgcc2 빌드가 시작돼 실패함. 반드시 -j1.
cd "$work/prc-tools-remix"
rm -rf build && mkdir build && cd build
CC="gcc -std=gnu89" CFLAGS="-w -O1 -fcommon -fno-strict-aliasing" \
  ../prc-tools-2.3/configure \
    --enable-targets=m68k-palmos \
    --enable-languages=c,c++ \
    --disable-nls \
    --build=i686-linux-gnu --host=i686-linux-gnu \
    --prefix=/usr \
    --with-palmdev-prefix=/opt/palmdev
CC="gcc -std=gnu89" CFLAGS="-w -O1 -fcommon -fno-strict-aliasing" make -j1
make MAKEINFO=true install

# 4. SDK 연결
mkdir -p /opt/palmdev
ln -sfn "$work/palm-os-sdk/sdk-4" /opt/palmdev/sdk-4
palmdev-prep

# 5. PilRC 빌드
cd "$work/pilrc/unix"
export CFLAGS="-std=gnu89 -w"
aclocal && automake --add-missing && autoconf
mkdir -p "$work/pilrc/b" && cd "$work/pilrc/b"
../unix/configure --prefix=/usr/local
make
install -m 0755 pilrc /usr/local/bin/pilrc

echo "완료: $(m68k-palmos-gcc --version | head -1)"
