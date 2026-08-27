#ifndef _MAIN_SAFETY_H_
#define _MAIN_SAFETY_H_

#ifdef __cplusplus
struct AppContext;
#endif

void MAIN_SAFETY_F_ProcessInData(void);

#ifdef __cplusplus
void SyncInput(AppContext* ctx);
void SyncOutput(AppContext* ctx);
#endif

#endif