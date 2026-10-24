@ MnHangul5: 표 8 주소를 돌려줌(앱이 PceNativeCall로 부름). *userData = [r9 - 8] (리틀엔디언)
        .text
        .arm
        .global Table8
Table8: ldr     r0, [r9, #-8]
        str     r0, [r1]
        mov     r0, #0
        bx      lr
