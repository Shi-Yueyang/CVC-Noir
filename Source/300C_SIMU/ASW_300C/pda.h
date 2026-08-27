#ifndef _PDA_H_
#define _PDA_H_

#include "cvc_datatypes.h"
#include "Interface_Data.h"

#define VITAL_DATA_OFFSET      0x00000
#define NVITAL_DATA_OFFSET     0x00000

#define LONG_DATA_MAX_SIZE      0xF00004u 
#define PDA_MSG_MAX_SIZE           0x200004u 
#define VITAL_DATA_MAX_SIZE     1024u
#pragma pack(1)
typedef struct
{
	INT8U AreaIdex; /*mass data:[0,6]; vital_data:7,9;nvital_data:8,10*/
	BOOLEAN operationResult;/*0:failed;1:successful*/
}PDA_T_PdaAck;

typedef struct
{
	INT8U AreaIdex;/*mass data:[0,6]; vital_data:7,9;nvital_data:8,10*/
	INT32U datasize;
	INT32U crc;
}PDA_T_PdaMsgHeader;

typedef struct
{
	PDA_T_PdaMsgHeader PdaMsgHeader;
	INT8U  data[PDA_MSG_MAX_SIZE];
}PDA_T_PdaMsg;

typedef struct
{
	PDA_T_PdaMsgHeader PdaMsgHeader;
	INT8U  data[LONG_DATA_MAX_SIZE];
}PDA_T_PdaLongMsg;

typedef struct
{
	PDA_T_PdaMsgHeader PdaMsgHeader;
	INT8U  data[VITAL_DATA_MAX_SIZE];
}PDA_T_PdaVitalMsg;
#pragma pack()

void CVC_BSW_ITF_PdaDataInitRead(INT8U areaIdx, INT8U* pData, INT32U iLength);
void CVC_BSW_ITF_MassDataInit(INT8U IArea,INT8U* pData, INT32U iLength);
void CVC_BSW_ITF_FileIDInitRead(INT8U* pData, INT32U size);

CVC_APP_PDA_STATE_t CVC_ReadPDAVital(CVC_APP_Store_Data_t* pData);
CVC_APP_PDA_STATE_t CVC_ReadPDAVitalStatus(void);
CVC_APP_PDA_STATE_t CVC_ReadPDANVital(CVC_APP_Store_Data_t* pData);
CVC_APP_PDA_STATE_t CVC_WriteNVitalData(CVC_APP_Store_Data_t* pStoreData);
CVC_APP_PDA_STATE_t CVC_ReadPDANVitalStatus(void);
CVC_APP_PDA_STATE_t CVC_ReadPDAMassStatus(const INT8U iArea);
void CVC_ReadPDAMassLineID(INT32U* opLineID);

#endif /*_PDA_H_*/
