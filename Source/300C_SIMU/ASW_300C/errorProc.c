#include "errorProc.h"
#include <stdio.h>

static FatalErrorRecord_t cycleRecord = { 0u };

void ERROR_PROCESS_F_ErrMsg_Add(INT32U ERRLevel, INT32U ERRNo, INT32S arg1, INT32S arg2, INT32S arg3, INT32S arg4, INT32S arg5, INT32S arg6)
{
	switch (ERRLevel)
	{
	case ERROR_PROCESS_C_ALARM_FATAL:
		/*ERROR_F_Print();*/
		printf("fatal error:0X%04X %d %d %d %d %d %d\n", ERRNo, arg1, arg2, arg3, arg4, arg5, arg6);
		storeFatalError(ERRNo, 0u);
		// GDF_M_FAULT_EXT(90, ERRNo, "");
		break;

	case ERROR_PROCESS_C_ALARM_NO_FATAL:
		// printf("Good, no fatal error. Error No [0X%04X] %d %d %d %d %d %d\n", ERRNo, arg1, arg2, arg3, arg4, arg5, arg6);
		// ERROR_PROCESS_F_TaskQuePush(ERRNo,arg1, arg2, arg3, arg4, arg5, arg6);
		break;

	default:
		/*do nothing*/
		break;
	}
	return;
}

BOOLEAN storeFatalError(INT16U code,INT32U vsn)
{
	BOOLEAN res = CVC_FALSE;

	if (256U > cycleRecord.errorNum)
	{
		cycleRecord.VsnRecord = vsn;
		cycleRecord.errorCodes[cycleRecord.errorNum] = code;
		cycleRecord.errorNum++;
		res = CVC_TRUE;
	}

	return res;
}

INT16U getFatalErrorNum(FatalErrorRecord_t** pData)
{
	if (NULL != pData)
	{
		*pData = &cycleRecord;
	}
	return cycleRecord.errorNum;
}

