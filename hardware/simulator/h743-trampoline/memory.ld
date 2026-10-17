/* STM32H743 emulator map with Trampoline's generated vector table. */
ENTRY(tpl_reset_handler)
MEMORY {
 FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1M
 RAM (rwx) : ORIGIN = 0x24001000, LENGTH = 0x1F000
}
_estack = 0x24020000;
SECTIONS {
 .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
 .text : {
   *(.text*) *(.SC_handler) *(.EI_handler)
   *(.osCode*) *(.osApiCode*) *(.appCommonCode*) *(.appCode*)
   *(.rodata*) *(.osConst*) *(.appConst*)
 } > FLASH
 . = ALIGN(4);
 _sidata = LOADADDR(.data);
 .data : {
   _sdata = .;
   *(.data*) *(.osVar*) *(.osApiVar*) *(.appVar*)
   . = ALIGN(4);
   _edata = .;
 } > RAM AT > FLASH
 .bss (NOLOAD) : {
   . = ALIGN(4);
   _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .;
 } > RAM
}
ASSERT(_ebss < _estack - 0x1000, "Cortex-M7 task stack headroom")
