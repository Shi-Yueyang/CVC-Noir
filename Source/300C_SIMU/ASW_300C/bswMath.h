#ifndef _GENERIC_FUNC_
#define _GENERIC_FUNC_

#include "cvc_datatypes.h"

void STD_F_MemcpyEx(char* fileName, INT16U lineNo, void *pDest, INT32U len, const void *pSrc, INT32U n);
void STD_F_MemsetEx(char* fileName, INT16U lineNo, void *pDest, INT8U c, INT32U n);
INT32U CRC_F_InlineCalculateCRC32_0x04C11DB7(INT8U* pData, INT32U dataLen);

#endif /*_GENERIC_FUNC_*/