| MnHangul 호출 세기 코드 (리소스 HgCt 0) — 3단계 조사: 폭·글자 경계 함수가 실제로 불리는지
|
| 함수마다 18바이트 짜리 통과 코드(StubN)를 둔다. 인자·스택을 건드리지 않으므로 함수 모양을 몰라도 된다:
|   원래 주소를 스택에 넣고 → a0 보관 → 횟수 칸 +1 → a0 복구 → rts (= 원래 함수로 점프, 스택은 호출 때 그대로)
| 배치: 오프셋 0 = 횟수 블록 주소(앱이 DmWrite), 4 + 4n = 원래 주소 n(앱이 DmWrite), 100 + 18n = StubN.
| 횟수 블록은 앱이 동적 힙에 만들고 소유자를 시스템(0)으로 바꿔 앱이 끝나도 남긴다.
| 대상 24개(앱 app.c kCountId와 같은 순서): 트랩 8개(0~7) + Intl 루틴 16개(8~23).
	.text
	.even
Counters:
	.long 0
Orig:
	.rept 24
	.long 0
	.endr

	.macro STUB n
Stub\n:
	move.l Orig+(\n*4)(%pc),-(%sp)
	move.l %a0,-(%sp)
	movea.l Counters(%pc),%a0
	addq.l #1,(\n*4)(%a0)
	movea.l (%sp)+,%a0
	rts
	.endm

	STUB 0
	STUB 1
	STUB 2
	STUB 3
	STUB 4
	STUB 5
	STUB 6
	STUB 7
	STUB 8
	STUB 9
	STUB 10
	STUB 11
	STUB 12
	STUB 13
	STUB 14
	STUB 15
	STUB 16
	STUB 17
	STUB 18
	STUB 19
	STUB 20
	STUB 21
	STUB 22
	STUB 23
