#ifndef ASW_RUNTIME_API_INCLUDE
#define ASW_RUNTIME_API_INCLUDE

#include "cvc_datatypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void API_ReadCurrentRunTime(INT32U* const opValue);
extern INT8U SRV_Initialize(INT8U* const ipCfgBytes, const INT32U icfgBytes, INT32U* opAppVer, INT64U SFRawAddr);
extern void SRV_ActiveCycle(void);
extern INT8U SRV_Teach(void);
extern INT8U SRV_Learn(void);
extern void SRV_ShutdownActiveCycle(void);
extern INT8U SRV_ReadMaintMsg(INT8U* ipMantMsg, INT16U iMantMsgLen);


#ifdef __cplusplus
}
#endif

#endif
