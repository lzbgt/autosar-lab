/* Real Trampoline task, activated by generated 1-ms OIL alarm.
 * This proof body is replaced by AutoAS service execution after kernel
 * scheduler and H743 timer are independently verified.
 */
#include "tpl_os.h"
#include "autoas_bsw.h"
#include <stdint.h>
#define DIAG ((volatile uint32_t *)0x24000000u)
FUNC(int, OS_APPL_CODE) main(void) {
  AutoAS_BswInit();
  DIAG[25] = 0u;
  DIAG[26] = 0u;
  StartOS(OSDEFAULTAPPMODE);
  DIAG[0] = 0xBAD70001u;
  for (;;) { __asm volatile("wfi"); }
}
TASK(bsw_tick) {
  /* Real Trampoline OIL alarm owns SysTick; one task activation per 1 ms. */
  AutoAS_BswPoll();
  AutoAS_BswTick1ms();
  AutoAS_BswUpdate();
  DIAG[25]++;
  if ((DIAG[25] % 10u) == 0u) DIAG[26]++;
  TerminateTask();
}
__attribute__((noreturn)) void _exit(int status) {
  (void)status;
  for (;;) { __asm volatile("wfi"); }
}
