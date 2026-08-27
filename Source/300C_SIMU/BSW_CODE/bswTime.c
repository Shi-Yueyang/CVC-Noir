#ifdef _WIN32
#include <windows.h>
#else
#define _POSIX_C_SOURCE 199309L
#include <time.h>
#endif
#include<stdint.h>
#include "bswTime.h"

static unsigned char ini_sys_time_flag = 0;

#ifdef _WIN32
static double g_win_freq = 0.0;
static double ini_large_int_time = 0;
LARGE_INTEGER large_int_freq = { 0 };
LARGE_INTEGER cur_large_int_time = { 0 };
#else
static struct timespec g_start_time = { 0, 0 };
#endif

static double tick = 0.0;
static uint32_t currRunTime = 0u;
static uint32_t lastRunTime = 0u;

/*10ms*/
void setcurrRunTime(uint32_t val)
{
	currRunTime = val;
	return;
}

/*10ms*/
void setlastRunTime(uint32_t val)
{
	lastRunTime = val;
	return;
}

/*10ms*/
uint32_t getcurrRunTime(void)
{
	return currRunTime;
}

/*10ms*/
uint64_t getcurrRunTimeFromAdapter(const uint8_t * const temp_array)
{

	uint32_t times_temp = 0U;
	uint64_t times	   = 0U;
	uint32_t first_byte = 0U;
	uint32_t sec_byte   = 0U;
	uint32_t third_byte = 0U;
	uint32_t forth_byte = 0U;

	first_byte = temp_array[2];
	sec_byte   = (temp_array[3] << 8)  & 0xffffffff;
	third_byte = (temp_array[4] << 16) & 0xffffffff;
	forth_byte = (temp_array[5] << 24) & 0xffffffff;

	times_temp = (first_byte + sec_byte + third_byte + forth_byte);
	times = times_temp / 10;

	return times;
}


/*10ms*/
uint32_t getlastRunTime(void)
{
	return lastRunTime;
}

void timer_Init(void)
{
	//clock
	if (0 == ini_sys_time_flag)
	{
#ifdef _WIN32
		QueryPerformanceFrequency(&large_int_freq);
		g_win_freq = (double)large_int_freq.QuadPart;
		QueryPerformanceCounter(&cur_large_int_time);
		ini_large_int_time = (double)cur_large_int_time.QuadPart;/* ms */;
#else
		clock_gettime(CLOCK_MONOTONIC, &g_start_time);
#endif
		ini_sys_time_flag = 1;
	}
}

/// <summary>
/// update curr clock
/// </summary>
static void timer_Update(void)
{
#ifdef _WIN32
	QueryPerformanceCounter(&cur_large_int_time);
	tick = (double)(((double)cur_large_int_time.QuadPart - ini_large_int_time) / g_win_freq * 10000.0);/* 0.1ms */
#else
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	double elapsed_sec = (double)(now.tv_sec - g_start_time.tv_sec) +
	                     (double)(now.tv_nsec - g_start_time.tv_nsec) / 1e9;
	tick = elapsed_sec * 10000.0; /* 0.1ms units, matches Windows behaviour */
#endif
	return;
}

/// <summary>
/// get curr clock(return daouble)
/// </summary>
/// <returns> 0.1ms per </returns>
double timer_TickGet(void)
{
	timer_Update();
	return tick;
}

uint32_t timer_GetCostTime(uint32_t start, uint32_t end)
{
	uint64_t t;
	uint64_t endValue = (uint64_t)end;
	uint64_t startValue = (uint64_t)start;
	uint32_t cost;
	t = (endValue - startValue + (uint64_t)0xFFFFFFFFu + (uint64_t)1u) % ((uint64_t)0xFFFFFFFFu + (uint64_t)1u);
	cost = (uint32_t)(t / 100u)/* t * 1000000 / sys_timestamp_freq()*/;
	/*    debug_printf("cost:%d us\n", cost);*/
	return cost;
}

// 1 ms
uint32_t timer_bswCurrTickGet(void)
{
	uint32_t curr = 0U;
	timer_Update();
	curr = (uint32_t)(tick / 10);
	return curr;
}