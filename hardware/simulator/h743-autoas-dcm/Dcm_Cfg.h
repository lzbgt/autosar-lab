/* Minimal genuine AutoAS Dcm service configuration for the MCU Renode lab.
 * Only DiagnosticSessionControl (0x10) and ReadDataByIdentifier (0x22) are
 * enabled. No security access, memory read/write, download, NvM, or Dem.
 */
#ifndef DCM_CFG_H
#define DCM_CFG_H
#ifndef DCM_CONST
#define DCM_CONST
#endif
#define DCM_MAIN_FUNCTION_PERIOD 10u
#define DCM_CONVERT_MS_TO_MAIN_CYCLES(ms) \
    (((ms) + DCM_MAIN_FUNCTION_PERIOD - 1u) / DCM_MAIN_FUNCTION_PERIOD)
#define DCM_USE_SERVICE_DIAGNOSTIC_SESSION_CONTROL
#define DCM_USE_SERVICE_READ_DATA_BY_IDENTIFIER
#endif
