#include <stdio.h>
#include <memory.h>
#include "bswMath.h"

void STD_F_MemsetEx(char* fileName, INT16U lineNo, void *pDest, INT8U c, INT32U n)
{
   /* INT32U i = 0u;
    INT8U* pAddr = CVC_NULL;*/

	if(CVC_NULL == pDest)
	{
		printf("[memsetEx] Dest is NULL! file:%s,line:%d\n", fileName, lineNo);
		// GDF_M_FAULT_EXT(EVT_C_NULL_PTR_ERROR, GDF_M_CONTEXT(0), "NULL pointer detected");
		return;
	}    
	
	memset(pDest,c,n);
	
	return;
}

void STD_F_MemcpyEx(char* fileName, INT16U lineNo, void *pDest, INT32U len, const void *pSrc, INT32U n)
{
    INT32U count = 0u;
    INT32U i = 0u;
    INT32U j = 0u;
    INT8U* pDestAddr = CVC_NULL;
    const INT8U* pSourceAddr = CVC_NULL;
    
    //GDF_M_NULL_ASSERT(pDest);
    //GDF_M_NULL_ASSERT(pSrc);
    if(len < n)
    {
    	printf("STD_F_Memcpy len error:%s,line %d destLen %d srcLen %d\n", fileName, lineNo, len, n);
    	// GDF_M_FAULT_EXT(EVT_C_OUT_OF_RANGE_ERROR, GDF_M_CONTEXT(len), "Out of range error"); 
    }
    

    pDestAddr = (INT8U*)pDest;
    pSourceAddr = (const INT8U*)pSrc;

	if (((INT64U)pDestAddr) <= ((const INT64U)pSourceAddr))
    {
        if ((pSourceAddr - pDestAddr) < n)
        {
        	//printf("STD_F_Memcpy error1:%s, %d %d %x %x\n", fileName, lineNo, n, pSourceAddr, pDestAddr);
            // GDF_M_FAULT_EXT(EVT_C_OVERLAPPING_ERROR, n, "");
			return;
        }
    }
    else
    {
        if ((pDestAddr - pSourceAddr) < n)
        {
			//printf("STD_F_Memcpy error2:%s, %d %d %x %x\n", fileName, lineNo, n, pSourceAddr, pDestAddr);
            // GDF_M_FAULT_EXT(EVT_C_OVERLAPPING_ERROR, n, "");
			return;
        }
    }
	
	memcpy(pDestAddr, pSourceAddr, n);

	return;
}

INT32U CRC_F_InlineCalculateCRC32_0x04C11DB7(INT8U* pData, INT32U dataLen)
{
	INT32U ret = 0xffu;


	return ret;
}