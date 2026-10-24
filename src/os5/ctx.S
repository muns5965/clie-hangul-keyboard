@ MnHangul5: 상태 블록(struct Ctx, os5.h)과 PC 상대 접근 함수. 코드와 함께 복사되므로 위치와 상관없이 쓴다.
@ link.ld가 이 구역을 맨 앞(오프셋 0)에 둔다.
        .section .text.ctx, "ax"
        .arm
        .global CtxPtr
CtxPtr:
        adr     r0, ctx
        bx      lr
        .align  2
        .global ctx
ctx:    .space  384                 @ struct Ctx (os5.h) 376바이트
