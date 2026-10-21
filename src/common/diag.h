/*
 * 진단 빌드 / 배포 빌드 구분 (MnHangul·MnSonyKbd·MnSkCore 공용).
 *   MN_DIAG 1(기본) = 진단 빌드: 그리기·폭·입력 경로마다 진단 기록(Feature)을 남긴다. 앱 진단 화면이 이 값을 보여 준다.
 *   MN_DIAG 0 = 배포 빌드(DIAG=0 tools/build-*.sh): 동작에 쓰이지 않는 기록만 뺀다. 동작용 상태 기록은 그대로.
 *   어떤 기록이 동작용인지는 docs/ARCHITECTURE.md "기록 감사" 표.
 */
#ifndef MN_DIAG_H
#define MN_DIAG_H

#ifndef MN_DIAG
#define MN_DIAG 1
#endif

#if MN_DIAG
#define MN_D(x) x
#else
#define MN_D(x) ((void)0)
#endif

#endif
