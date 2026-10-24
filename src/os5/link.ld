/* MnHangul5 ARM 코드: 위치 독립 한 덩어리(.text만). ctx.S가 맨 앞 */
SECTIONS {
  . = 0;
  .text : { *(.text.ctx) *(.text*) *(.rodata*) }
  /DISCARD/ : { *(.data*) *(.bss*) *(.ARM.exidx*) *(.comment) }
}
