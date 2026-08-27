#include "pda.h"
#include "bswMath.h"
#include "errorProc.h"

INT8U PDA_V_AccessPermission[9u] = { 0u }; /*val 1:not argee 3:argee idx:0-6mass 7vital 8nvital*/
static PDA_T_PdaMsg PDA_V_MassData[2u] = { 0u };
static PDA_T_PdaLongMsg PDA_V_LongMassData[5u] = { 0u };
static PDA_T_PdaVitalMsg PDA_V_VitalData_R500 = { 0u };
static PDA_T_PdaVitalMsg PDA_V_NonVitalData_R500 = { 0u };
static INT8U PDA_V_OperationStatus[9u] = { 0u }; /*idx 0 - 6:mass 7vital 8nvital; val 0:read success, 1: write success, 2:ing, 3:fail*/
static INT32U lineID[7U] = { 0u };
static INT8U PDA_V_OperationfailedNum[9u] = { 0u };
INT8U PDA_V_VDataReadState = 0u;
//INT8U PDA_V_ReadStatus[11u] = { 0u };

/*areaIdx:7vital 8nvital*/
void CVC_BSW_ITF_PdaDataInitRead(INT8U areaIndex,INT8U* pData, INT32U iLength)
{
	PDA_T_PdaVitalMsg* cpyDest = NULL;
	//CVC_APP_Store_Data_t* pStore = NULL;

	//STD_F_MemsetEx(__FILE__, __LINE__, cpyDest, 0u, sizeof(PDA_T_PdaVitalMsg));
	if (7u == areaIndex)
	{
		cpyDest = &PDA_V_VitalData_R500;
	}
	else if (8u == areaIndex)
	{
		cpyDest = &PDA_V_NonVitalData_R500;
	}
	else
	{
		return;
	}
	
	STD_F_MemsetEx(__FILE__, __LINE__, cpyDest, 0u, sizeof(PDA_T_PdaVitalMsg));

	//PDA_V_AccessPermission[areaIndex] = 1u;
	//STD_F_MemcpyEx(__FILE__, __LINE__, cpyDest, sizeof(PDA_T_PdaMsgHeader), pData + (offset), sizeof(PDA_T_PdaMsgHeader));

	if (cpyDest->PdaMsgHeader.datasize > VITAL_DATA_MAX_SIZE || 0u == iLength)
	{
		cpyDest->PdaMsgHeader.datasize = 0u;
		return;
	}
	else
	{
		cpyDest->PdaMsgHeader.AreaIdex = areaIndex;
		cpyDest->PdaMsgHeader.datasize = iLength;
		STD_F_MemcpyEx(__FILE__, __LINE__, cpyDest->data, iLength, pData, iLength);
	}

	//PDA_V_AccessPermission[areaIndex] = 3u;
	return;
}

void CVC_BSW_ITF_MassDataInit(INT8U iArea, INT8U* pData, INT32U iLength)
{
	CVC_T_Status status = CVC_C_ERROR;
	//PDA_V_AccessPermission[iArea] = 1u;
	GDF_M_NULL_ASSERT(pData,__FILE__, __LINE__);
	if (iArea < 2u)
	{
		GDF_M_RANGE_ASSERT(iLength > PDA_MSG_MAX_SIZE - 4u, __FILE__, __LINE__);
	}
	else
	{
		GDF_M_RANGE_ASSERT(iLength > LONG_DATA_MAX_SIZE - 4u, __FILE__, __LINE__);
	}

	if (iArea < 2u)
	{
		STD_F_MemsetEx(__FILE__, __LINE__, PDA_V_MassData[iArea].data + 4u, 0u, PDA_MSG_MAX_SIZE);
		STD_F_MemcpyEx(__FILE__, __LINE__, PDA_V_MassData[iArea].data + 4u, PDA_MSG_MAX_SIZE, pData, iLength);
	}
	else
	{
		STD_F_MemsetEx(__FILE__, __LINE__, PDA_V_LongMassData[iArea - 2u].data + 4u, 0u, PDA_MSG_MAX_SIZE);
		STD_F_MemcpyEx(__FILE__, __LINE__, PDA_V_LongMassData[iArea - 2u].data + 4u, LONG_DATA_MAX_SIZE, pData, iLength);
	}
	//PDA_V_AccessPermission[iArea] = 3u;
	return;
}

void CVC_BSW_ITF_FileIDInitRead(INT8U* pData,INT32U size)
{
	INT8U area = 0u;
	GDF_M_RANGE_ASSERT(size != 28u, __FILE__, __LINE__);
	
	for (area = 0u; area < 7U; area++)
	{
		pData = pData + area * 4U;
		if (2u > area)
		{
			//PDA_V_AccessPermission[i] = 1u;
			STD_F_MemcpyEx(__FILE__, __LINE__, PDA_V_MassData[area].data, 4u, pData, 4u);
			//PDA_V_AccessPermission[i] = 3u;
		}
		else
		{
			//PDA_V_AccessPermission[i + 2u] = 1u;
			STD_F_MemcpyEx(__FILE__, __LINE__, PDA_V_LongMassData[area - 2u].data, 4u, pData, 4u);
			//PDA_V_AccessPermission[i + 2u] = 3u;
		}

		STD_F_MemcpyEx(__FILE__, __LINE__, lineID+area, 4u, pData, 4u);
	}

	return;
}

static CVC_APP_PDA_STATE_t judgePDAState(INT8U iArea)
{
	if (3u == PDA_V_OperationStatus[iArea])
	{
		PDA_V_OperationfailedNum[iArea]++;
		if (3u < PDA_V_OperationfailedNum[iArea]) {
			return CVC_PDA_NO_AVAILABLE;
		}
		return CVC_PDA_OPERATION_FAILED;
	}
	else if (2u == PDA_V_OperationStatus[iArea])
	{
		return CVC_PDA_OPERATION_PENDING;
	}

	return CVC_PDA_OPERATION_SUCCEED;
}

CVC_APP_PDA_STATE_t CVC_ReadPDAVital(CVC_APP_Store_Data_t* pData)
{
	CVC_APP_PDA_STATE_t result = CVC_PDA_OPERATION_FAILED;

	GDF_M_NULL_ASSERT(pData, __FILE__, __LINE__);

	result = judgePDAState(7u);
	if (CVC_PDA_OPERATION_SUCCEED != result)
	{
		return result;
	}
	PDA_V_OperationStatus[7u] = 2u;
	pData->CRC = PDA_V_VitalData_R500.PdaMsgHeader.crc;
	pData->datasize = PDA_V_VitalData_R500.PdaMsgHeader.datasize;
	STD_F_MemcpyEx(__FILE__, __LINE__, pData->data, MAX_NVRAM_DATA_LENGTH, PDA_V_VitalData_R500.data, PDA_V_VitalData_R500.PdaMsgHeader.datasize);
	PDA_V_OperationStatus[7u] = 0u;
	PDA_V_OperationfailedNum[7u] = 0u;
	result = CVC_PDA_OPERATION_SUCCEED;

	return result;
}

CVC_APP_PDA_STATE_t CVC_ReadPDAVitalStatus(void)
{

	CVC_APP_PDA_STATE_t state = CVC_PDA_NO_OPERATION;

	if ((PDA_V_OperationStatus[7u] == 3u) && (PDA_V_OperationfailedNum[7u] > 3u))
	{
		state = CVC_PDA_NO_AVAILABLE;
	}
	else
	{
		state = (CVC_APP_PDA_STATE_t)PDA_V_OperationStatus[7u];
	}
	if (PDA_V_OperationStatus[7u] != 2u)
	{
		PDA_V_OperationStatus[7] = 0u;
	}

	return state;
}

CVC_APP_PDA_STATE_t CVC_ReadPDANVital(CVC_APP_Store_Data_t* pData)
{
	CVC_APP_PDA_STATE_t result = CVC_PDA_NO_OPERATION;

	GDF_M_NULL_ASSERT(pData, __FILE__, __LINE__);

	result = judgePDAState(8u);
	if (CVC_PDA_OPERATION_SUCCEED != result)
	{
		return result;
	}
	PDA_V_OperationStatus[8u] = 2u;
	pData->CRC = PDA_V_NonVitalData_R500.PdaMsgHeader.crc;
	pData->datasize = PDA_V_NonVitalData_R500.PdaMsgHeader.datasize;
	STD_F_MemcpyEx(__FILE__, __LINE__, pData->data, MAX_NVRAM_DATA_LENGTH, PDA_V_NonVitalData_R500.data, PDA_V_NonVitalData_R500.PdaMsgHeader.datasize);
	result = CVC_PDA_OPERATION_SUCCEED;
	PDA_V_OperationfailedNum[8u] = 0u;
	PDA_V_OperationStatus[8u] = 0u;

	return result;
}


CVC_APP_PDA_STATE_t CVC_ReadPDANVitalStatus(void)
{

	CVC_APP_PDA_STATE_t state = CVC_PDA_NO_OPERATION;
	if ((PDA_V_OperationStatus[8u] == 3u) && (PDA_V_OperationfailedNum[8u] > 3u))
	{
		state = CVC_PDA_NO_AVAILABLE;
	}
	else
	{
		state = (CVC_APP_PDA_STATE_t)PDA_V_OperationStatus[8u];
	}
	if (PDA_V_OperationStatus[8u] != 2u)
	{
		PDA_V_OperationStatus[8u] = 0u;
	}
	return state;
}


CVC_APP_PDA_STATE_t CVC_ReadPDAMassStatus(const INT8U iArea)
{
	CVC_APP_PDA_STATE_t state = CVC_PDA_NO_OPERATION;
	if ((PDA_V_OperationStatus[iArea] == 3u) && (PDA_V_OperationfailedNum[iArea] > 3u))
	{
		state = CVC_PDA_NO_AVAILABLE;
	}
	else
	{
		state = (CVC_APP_PDA_STATE_t)PDA_V_OperationStatus[iArea];
	}
	if (PDA_V_OperationStatus[iArea] != 2u)
	{
		PDA_V_OperationStatus[iArea] = 0u;
	}
	return state;
}

void CVC_ReadPDAMassLineID(INT32U* opLineID)
{
	INT32U LineID[7u] = { 0u };
	INT8U i = 0u;

	for (i = 0u; i < 2u; i++)
	{
		if (PDA_V_OperationStatus[i] != 3u)
		{
			STD_F_MemcpyEx(__FILE__, __LINE__, &LineID[i], 4u, PDA_V_MassData[i].data, 4u);
		}
	}
	for (i = 0u; i < 5u; i++)
	{
		if (PDA_V_OperationStatus[i + 2u] != 3u)
		{
			STD_F_MemcpyEx(__FILE__, __LINE__, &LineID[i + 2u], 4u, PDA_V_LongMassData[i].data, 4u);
		}
	}
	STD_F_MemcpyEx(__FILE__, __LINE__, opLineID, sizeof(LineID), LineID, sizeof(LineID));
	return;
}
