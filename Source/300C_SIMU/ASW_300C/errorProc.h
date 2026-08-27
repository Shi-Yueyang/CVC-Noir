#ifndef _ERROR_300CPROCESS_
#define _ERROR_300CPROCESS_

#include <stdio.h>
#include "cvc_datatypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(1)
typedef struct
{
    INT8U errorNum;
    INT32U VsnRecord;
    INT16U errorCodes[256u];
}FatalErrorRecord_t;

typedef enum
{
    ERROR_PROCESS_C_ALARM_PRINT = 4U,	/*** ����ά����Ĵ��󣬲����ô�����������ӡ������*/
    ERROR_PROCESS_C_ALARM_FATAL_INTR = 3U,	/*** �ж���Ĺ��ϣ�����Ӱ��ϵͳ���У�ֱ��崻� */
    ERROR_PROCESS_C_ALARM_FATAL_FRONT = 2U,	/*** ����ά�������ʼ���ɹ�ǰ������Ӱ��ϵͳ���У�ֱ��崻� */
    ERROR_PROCESS_C_ALARM_FATAL = 1U,	/*** ����Σ�չ��Ͼ��棬����Ӱ��ϵͳ���У�ֱ��崻� */
    ERROR_PROCESS_C_ALARM_NO_FATAL = 0U,	/*** ������Σ�չ��Ͼ��棬ϵͳ�ɼ������� */
} ERROR_PROCESS_E_ERR_LEVEL_t;
#pragma pack()

#define GDF_M_RANGE_ASSERT(_STATEMENT,_file,_line) do{                                     \
        if(_STATEMENT)                                                                  \
        {                                                                         \
            printf(_file);                                                         \
            printf(" line %d:\nout range fatal error!\n", _line);                         \
            return;                                                          \
        }                                                               \
        /*else{}        */                                                  \
    }                                                                   \
    while(0)


#define GDF_M_NULL_ASSERT(_POINTER,_file,_line) do{               \
        if(CVC_NULL == (_POINTER) )                                         \
        {                                                           \
            printf(_file);                                            \
            printf(" line %d:\nNULL pointer fatal error!\n", _line);                         \
            return;                                                   \
        }                                                               \
        /*else{}        */                                                  \
    }                                                                   \
    while(0)


void ERROR_PROCESS_F_ErrMsg_Add(INT32U ERRLevel, INT32U ERRNo, INT32S arg1, INT32S arg2, INT32S arg3, INT32S arg4, INT32S arg5, INT32S arg6);
BOOLEAN storeFatalError(INT16U code, INT32U vsn);
INT16U getFatalErrorNum(FatalErrorRecord_t** pData);

#ifdef __cplusplus
}
#endif

#endif