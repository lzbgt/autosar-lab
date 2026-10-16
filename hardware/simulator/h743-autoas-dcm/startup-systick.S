.syntax unified
.cpu cortex-m7
.thumb

/* Vector indices 0=SP, 1=Reset, 2..14=system exceptions, 15=SysTick.
 * Only the Dcm integration server uses this exception-enabled startup.
 * Other MCU examples retain their previously verified startup file. */
.section .isr_vector, "a", %progbits
.global __vectors
__vectors:
    .word _estack
    .word Reset_Handler
    .rept 13
    .word Default_Handler
    .endr
    .word SysTick_Handler

.section .text.Reset_Handler, "ax", %progbits
.thumb_func
.global Reset_Handler
Reset_Handler:
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:
    cmp r1, r2
    bhs 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b
2:
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
3:
    cmp r1, r2
    bhs 4f
    str r3, [r1], #4
    b 3b
4:
    bl main
5:
    b 5b

.section .text.Default_Handler, "ax", %progbits
.thumb_func
Default_Handler:
    b Default_Handler
