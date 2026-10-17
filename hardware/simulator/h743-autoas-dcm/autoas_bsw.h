#ifndef AUTOAS_STM32H743_BSW_RUNTIME_H
#define AUTOAS_STM32H743_BSW_RUNTIME_H

/* Same real AutoAS CanIf/CanTp/PduR/Dcm modules in both firmware profiles.
 * Standalone profile: SysTick-driven main loop.
 * Trampoline profile: generated OSEK task executes these services every 1 ms.
 * The 10 ms MainFunction cadence is performed inside AutoAS_BswTick1ms.
 */
void AutoAS_BswInit(void);
void AutoAS_BswPoll(void);
void AutoAS_BswTick1ms(void);
void AutoAS_BswUpdate(void);

#endif
