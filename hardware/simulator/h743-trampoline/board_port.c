/* Board-specific (not OS) adaptation: emulated H743 Cortex-M7 reset 64MHz.
 * The real Trampoline kernel/context-switch/startup assembly is upstream.
 */
#include <stdint.h>
void tpl_set_systick_timer(void) {
  volatile uint32_t *const sys = (volatile uint32_t *)0xE000E010u;
  sys[0]=0u;
  sys[1]=63999u;
  sys[2]=0u;
  sys[0]=7u;
}
void SysTick_ClearFlag(void) {}
/* F407 GOIL template enables no external interrupt sources; on H743 it
 * must not write an STM32F4-only RCC APB2 register. */
void tpl_init_external_interrupts(void) {}
void NMI_Handler(void) { for (;;) {} }
void HardFault_Handler(void) { for (;;) {} }
void MemManage_Handler(void) { for (;;) {} }
void BusFault_Handler(void) { for (;;) {} }
void UsageFault_Handler(void) { for (;;) {} }
void DebugMonitor_Handler(void) { for (;;) {} }
void PendSV_Handler(void) { for (;;) {} }
extern unsigned _sidata, _sdata, _edata, _sbss, _ebss;
extern int main(void);
void tpl_continue_reset_handler(void) {
  unsigned *src=&_sidata;
  for (unsigned *dst=&_sdata; dst<&_edata;) *dst++=*src++;
  for (unsigned *p=&_sbss; p<&_ebss; ++p) *p=0u;
  main();
  for (;;) {}
}
