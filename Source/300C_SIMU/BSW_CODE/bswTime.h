#ifndef _BSW_TIME_H_
#define _BSW_TIME_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


void setcurrRunTime(uint32_t val);
void setlastRunTime(uint32_t val);
uint32_t getcurrRunTime(void);
extern uint64_t getcurrRunTimeFromAdapter(const uint8_t* const temp_array);

uint32_t getlastRunTime(void);
void timer_Init(void);
double timer_TickGet(void);
uint32_t timer_GetCostTime(uint32_t start, uint32_t end);
uint32_t timer_bswCurrTickGet(void);

#ifdef __cplusplus
}
#endif

#endif