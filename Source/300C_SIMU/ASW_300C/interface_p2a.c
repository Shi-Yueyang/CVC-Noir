#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../BSW_CODE/mainsafety.h"
#include "../BSW_CODE/bswTime.h"
extern void SRV_ShutdownActiveCycle(void);
#include "interface_p2a.h"
#include "bswMath.h"
#include "pda.h"
#include "board_status.h"
#include "errorProc.h"
#include "conf.h"

#define SHUTDOWN_MSG_STARTR50      0u

extern INT16U INTERFACE_DATA_V_AppRunTime;
extern INT64U INTERFACE_DATA_V_CurTime;
extern INT32U INTERFACE_DATA_V_CurTimeTemp;
extern CONF_T_MinimumSet CONF_V_MinimumSet;

CVC_APP_PDA_STATE_t pda_3_states[FLASH_MAX_AREA_NUM] = { CVC_PDA_NO_OPERATION, CVC_PDA_NO_OPERATION };

BoardStatus_Config_t g_board_status_config = { 0 };
PDA_StorageConfig_t g_pda_storage_config = { 0 };

/* PDA state machine for async operations */
typedef struct _PDA_OperationState_t
{
    CVC_APP_PDA_STATE_t state;
    uint8_t fail_count;
    uint64_t done_at_ms;  /* monotonic completion time for delayed ops; 0 = immediate */
    uint8_t armed_op;     /* which op armed the current pending state */
} PDA_OperationState_t;

#define PDA_ARM_NONE  0U
#define PDA_ARM_READ  1U
#define PDA_ARM_WRITE 2U

static PDA_OperationState_t nrnw_state = { CVC_PDA_NO_OPERATION, 0, 0, PDA_ARM_NONE };
static PDA_OperationState_t araw_state = { CVC_PDA_NO_OPERATION, 0, 0, PDA_ARM_NONE };
static PDA_OperationState_t arnw_state = { CVC_PDA_NO_OPERATION, 0, 0, PDA_ARM_NONE };

/* monotonic milliseconds (timer_TickGet returns 0.1ms units) */
static uint64_t Pda_NowMs(void)
{
	return (uint64_t)(timer_TickGet() / 10.0);
}

static void Pda_ArmPending(PDA_OperationState_t* st, uint32_t delay_ms, uint8_t op)
{
	st->state = CVC_PDA_OPERATION_PENDING;
	st->armed_op = op;
	st->done_at_ms = (delay_ms == 0U) ? 0U : (Pda_NowMs() + (uint64_t)delay_ms);
}

static int Pda_DelayElapsed(const PDA_OperationState_t* st)
{
	return (st->done_at_ms == 0U) || (Pda_NowMs() >= st->done_at_ms);
}

CVC_T_Status API_ReadNewMsg(APPMSG_t* opAppMsg)
{
	INT32S iRet = 0;
	PFMSG_t PFMsg = { 0U };
	INT16U copySize = 0U;

	//GDF_M_NULL_ASSERT(opAppMsg);

	iRet = CVC_BSW_ITF_Read(CVC_BSW_APP_RX_TYPE, (INT8U*)&PFMsg);
	if (CVC_BUFFER_OPER_SUCCESS != iRet)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR3, iRet, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}

	if ((INT16U)MAX_BSWMSG_SIZE < (PFMsg.MsgSize))
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR4, PFMsg.MsgSize, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}
	else
	{
		opAppMsg->PeripheralNumber = PFMsg.PeripheralNumber;
		opAppMsg->Message[0U] = PFMsg.AppType;
		opAppMsg->Message[1U] = PFMsg.MsgID;
		//opAppMsg->Message[1U] = 1;
		opAppMsg->Message[2U] = PFMsg.ITFVer;

		if ((PFMsg.MsgSize + 3U) > MAX_BSWMSG_SIZE)
		{
			ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR1B, PFMsg.MsgSize, 0, 0, 0, 0, 0);
			return CVC_C_ERROR;
		}

		opAppMsg->MsgSize = PFMsg.MsgSize + 3U;
		copySize = PFMsg.MsgSize;
		STD_F_MemcpyEx(__FILE__, __LINE__, (void*)(&opAppMsg->Message[3U]), (INT32U)copySize, (const void*)(&PFMsg.Message[0U]), (INT32U)copySize);

		return CVC_C_NO_ERROR;
	}
}

CVC_T_Status API_WriteNewMsg(APPMSG_t* ipAppMsg)
{
	BOOLEAN ret = CVC_FALSE;
	PFMSG_t PFMsg = { 0U };

	//GDF_M_NULL_ASSERT(ipAppMsg);

	if (ipAppMsg->MsgSize <= 0U)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR0, ipAppMsg->MsgSize, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}

	STD_F_MemsetEx(__FILE__, __LINE__, (void*)&PFMsg, (INT8U)0U, (INT32U)sizeof(PFMSG_t));

	/*boardID MsgIndex LinkIndex暂未赋值，预留*/
	PFMsg.PeripheralNumber = ipAppMsg->PeripheralNumber;
	PFMsg.MsgSize = ipAppMsg->MsgSize - 3u;
	PFMsg.AppType = ipAppMsg->Message[0U];
	PFMsg.MsgID = ipAppMsg->Message[1U];
	PFMsg.ITFVer = ipAppMsg->Message[2U];
	PFMsg.GroupNum = 0U;

	if ((INT16U)MAX_BSWMSG_SIZE < (PFMsg.MsgSize))
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR1, PFMsg.MsgSize, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}

	STD_F_MemcpyEx(__FILE__, __LINE__, (void*)(PFMsg.Message), (INT32U)(PFMsg.MsgSize), (const void*)(&ipAppMsg->Message[3]), (INT32U)(PFMsg.MsgSize));

	ret = CVC_BSW_ITF_Write(CVC_BSW_APP_TX_TYPE, (INT8U*)&PFMsg, PFMsg.MsgSize + (INT16U)PFMSG_HEAD_SIZE);
	if (CVC_TRUE != ret)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR2, ret, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}
	else
	{
		return CVC_C_NO_ERROR;
	}
}

INT32U API_ReadNewMsgNum(void)
{
	INT16U num = 0U;
	BOOLEAN retBOOL = CVC_FALSE;

	retBOOL = CVC_BSW_ITF_GetMsgNum(CVC_BSW_APP_RX_TYPE, &num);
	if (CVC_FALSE == retBOOL)
	{
		return 0xFFFFFFFFU;
	}

	return (INT32U)num;
}

CVC_T_Status API_WriteMaintMsg(MANTMSG_t* ipMantMsg)
{
	BOOLEAN retBOOL = CVC_FALSE;

	//GDF_M_NULL_ASSERT(ipMantMsg);

	if ((0U >= ipMantMsg->MsgSize) || (MAX_BSWMSG_SIZE < ipMantMsg->MsgSize))
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORRORC, ipMantMsg->MsgSize, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}

	retBOOL = CVC_BSW_ITF_Write(CVC_BSW_MANT_TX_TYPE, (INT8U*)ipMantMsg, ipMantMsg->MsgSize + (INT16U)MantMSG_HEAD_SIZE);
	if (CVC_TRUE != retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR2, retBOOL, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}
	else
	{
		return CVC_C_NO_ERROR;
	}
}

CVC_T_Status API_WriteMaintInfo(INT32U iPeripheralNumber, INT16U iMantInfoLen, INT8U* ipMantInfo, INT8U iType)
{
	BOOLEAN retBOOL = CVC_FALSE;

	//GDF_M_NULL_ASSERT(ipMantInfo);

	retBOOL = CVC_BSW_ITF_MantInfo_Write(iPeripheralNumber, iMantInfoLen, ipMantInfo, iType);
	if (CVC_TRUE != retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR19, retBOOL, iPeripheralNumber, iMantInfoLen, iType, 0, 0);
		return CVC_C_ERROR;
	}

	return CVC_C_NO_ERROR;
}

CVC_T_Status API_ReadDIInfo(IOData_t* opVIBData)
{
	INT32S iRet = 0;

	//GDF_M_NULL_ASSERT(opVIBData);

	iRet = CVC_BSW_ITF_Read(CVC_BSW_VIB_RX_TYPE, (INT8U*)opVIBData);
	if (CVC_BUFFER_OPER_SUCCESS != iRet)
	{
		/*ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR10, iRet, 0, 0, 0, 0, 0);*/
		return CVC_C_ERROR;
	}

	return CVC_C_NO_ERROR;
}

CVC_T_Status API_WriteDOInfo(IOData_t* ipVOBData)
{
	BOOLEAN ret = CVC_FALSE;

	//GDF_M_NULL_ASSERT(ipVOBData);

	ret = CVC_BSW_ITF_Write(CVC_BSW_VOB_TX_TYPE, (INT8U*)ipVOBData, sizeof(IOData_t));
	if (CVC_TRUE != ret)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR11, ret, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}
	else
	{
		return CVC_C_NO_ERROR;
	}
}

CVC_T_Status API_ReadVVBInfo(SPDGRPData_t *opSPDGRPData, ADDData_t *opADDData, INT8U iLogicID)
{		
	INT32S iRet = 0;
	VVBData_t VVBData = {0U};
	INT8U i = 0U;
	INT8U j = 0U;
	INT8U index = 0U;
	INT8U boardNum = 0U;
	
	// GDF_M_NULL_ASSERT(opSPDGRPData);
	// GDF_M_NULL_ASSERT(opADDData);

	STD_F_MemsetEx(__FILE__,__LINE__,(void *)&VVBData, 0x0U, (INT32U)sizeof(VVBData_t));
	iRet = CVC_BSW_ITF_Read(CVC_BSW_VVB_RX_TYPE, (INT8U *)&VVBData);
	if(CVC_BUFFER_OPER_SUCCESS != iRet)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR12, iRet, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}
	
	/*把moduleID转换为逻辑ID*/
	boardNum = CONF_V_MinimumSet.vvbSet.boardNum;
	for(i=0U;i<boardNum;++i)
	{
		++index;
		if(CONF_V_MinimumSet.vvbSet.modeInfo[i].modeID==VVBData.SPDData.ID)
		{
			break;
		}
	}

	VVBData.SPDData.ID = index;
//	VVBData.ADDData.ID = index;
	
//	opSPDGRPData->LOGIC_ID = VVBData.SPDData.ID;
	opSPDGRPData->Timestamp = VVBData.SPDData.Timestamp;
	for(i=0U;i<MAX_SPD_NUM;++i)
	{
		for (j = 0U; j < 2; ++j)
		{
			opSPDGRPData->PULGRPData[i].SPDDirct[j] = VVBData.SPDData.SPDData[i].SPDDirct;
			opSPDGRPData->PULGRPData[i].PulesGroupNum[j] = 1U;
			opSPDGRPData->PULGRPData[i].SPDGRPPules[j][SPDPULES_GROUP_NUM - 1U] = VVBData.SPDData.SPDData[i].SPDPules;
			opSPDGRPData->PULGRPData[i].PulseTimestamp[j] = VVBData.SPDData.Timestamp;
		}
		
	}
	
	STD_F_MemcpyEx(__FILE__,__LINE__,(void *)opADDData, (INT32U)sizeof(ADDData_t), (void *)(&VVBData.ADDData), (INT32U)sizeof(ADDData_t));

	return CVC_C_NO_ERROR;
}

INT32U API_ReadVVBInfoNum(INT8U iLogicID)
{
	INT16U num = 0U;
	BOOLEAN retBOOL = CVC_FALSE;

	retBOOL = CVC_BSW_ITF_GetMsgNum(CVC_BSW_VVB_RX_TYPE, &num);
	if (CVC_FALSE == retBOOL)
	{
		return 0xFFFFFFFFU;
	}

	return (INT32U)num;
}

CVC_T_Status API_ReadBTMInfo(APP_BTMData_t*opBTMData, INT8U iLogicID)
{		
	INT32S iRet = 0;
	
	// GDF_M_NULL_ASSERT(opBTMData);

	iRet = CVC_BSW_ITF_Read(CVC_BSW_BTM_RX_TYPE, (INT8U *)opBTMData);
	if(CVC_BUFFER_OPER_SUCCESS != iRet)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR15, iRet, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}

	return CVC_C_NO_ERROR;
}

INT32U API_ReadBTMInfoNum(INT8U iLogicID)
{
	INT16U num = 0U;
	BOOLEAN retBOOL = CVC_FALSE;

	retBOOL = CVC_BSW_ITF_GetMsgNum(CVC_BSW_BTM_RX_TYPE, &num);
	if (CVC_FALSE == retBOOL)
	{
		return 0xFFFFFFFFU;
	}

	return (INT32U)num;
}

void API_SetUsedAntennaID(INT8U iAntennaID)
{
	if ((MAX_BTM_NUM - 1U) < iAntennaID)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR25, 0, 0, 0, 0, 0, 0);
		return;
	}
	CVC_BSW_ITF_SetUsedAntennaID(iAntennaID);
}

extern unsigned char API_GetUsedAntennaID(void)
{
	return 0U;
} 

CVC_T_Status API_WriteVTSInfo(VTSData_t *ipVTSData)
{
	BOOLEAN ret = CVC_FALSE;

	// GDF_M_NULL_ASSERT(ipVTSData);

	ret = CVC_BSW_ITF_Write(CVC_BSW_VTS_TX_TYPE, (INT8U *)ipVTSData, sizeof(VTSData_t));
	if(CVC_TRUE != ret)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR16, ret, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}
	else
	{
		return CVC_C_NO_ERROR;
	}
}

CVC_T_Status API_ReadSysStatusInfo(SysStatus_Info_t* opSTAData)
{
	INT32S iRet = 0;
	INT8U board_type_idx = 0;
	INT8U board_num_idx = 0;
	INT8U board_status_idx = 0;

	memset(opSTAData, 0, sizeof(SysStatus_Info_t));

	opSTAData->BoardTypeNum = g_board_status_config.type_num;
	for (board_type_idx = 0; board_type_idx < opSTAData->BoardTypeNum; board_type_idx++)
	{
		const BoardStatus_ConfigType_t* config_type = &g_board_status_config.types[board_type_idx];
		const INT8U component_idx = config_type->type_id;
		opSTAData->Component[component_idx].TypeID = config_type->type_id;
		opSTAData->Component[component_idx].BoardNum = config_type->board_num;
		for (board_num_idx = 0; board_num_idx < config_type->board_num; board_num_idx++)
		{
			const BoardStatus_ConfigEntry_t* config_entry = &config_type->boards[board_num_idx];
			opSTAData->Component[component_idx].BoardStatus[board_num_idx].ModuleID = config_entry->id;
			opSTAData->Component[component_idx].BoardStatus[board_num_idx].ModuleID_R = config_entry->friend_id;
			if (config_entry->is_good)
			{
				memset(opSTAData->Component[component_idx].BoardStatus[board_num_idx].Status, 0xFF, sizeof(opSTAData->Component[component_idx].BoardStatus[board_num_idx].Status));
			}
			else
			{
				memset(opSTAData->Component[component_idx].BoardStatus[board_num_idx].Status, 0x00, sizeof(opSTAData->Component[component_idx].BoardStatus[board_num_idx].Status));
			}
		}
	}

	return CVC_C_NO_ERROR;
}

INT32U API_ReadSysStatusInfoNum(void)
{
	INT16U num = 0U;
	BOOLEAN retBOOL = CVC_FALSE;

	/* 暂时用于调试，待完善20240813 begin */
	return 1;
	/* 暂时用于调试，待完善20240813 end */

	retBOOL = CVC_BSW_ITF_GetMsgNum(CVC_BSW_STA_RX_TYPE, &num);
	if (CVC_FALSE == retBOOL)
	{
		return 0xFFFFFFFFU;
	}

	return (INT32U)num;
}

CVC_T_Status API_ReadLastRunTime(INT32U* opTimeSpend, INT32U* opTimeLimit, INT8U* opTimePercent)
{
	// GDF_M_NULL_ASSERT(opTimeSpend);
	// GDF_M_NULL_ASSERT(opTimeLimit);
	// GDF_M_NULL_ASSERT(opTimePercent);

	(*opTimeSpend) = INTERFACE_DATA_V_AppRunTime;
	(*opTimeLimit) = 200 * 1000U;//200ms

	if (0U == (*opTimeLimit))
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR29, 0, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}
	(*opTimePercent) = (INT8U)(((*opTimeSpend) * 100U) / (*opTimeLimit));

	/*up to 1ms when less than 1ms*/
	(*opTimeSpend) = (0U == (*opTimeSpend)) ? (1U) : (*opTimeSpend);
	(*opTimePercent) = (0U == (*opTimePercent)) ? (1U) : (*opTimePercent);
	INTERFACE_DATA_V_AppRunTime = 0U;

	return CVC_C_NO_ERROR;
}

/*10 ms*/
void API_ReadCurrentRunTime(INT32U* const opValue)
{
	*opValue = getcurrRunTime();
	return;
}

/*us*/
INT64U API_ReadCurTime(void)
{
	INT64U curTime = 0U;
	curTime = (INT64U)(timer_TickGet() * 1000.0);

	return curTime;
}

APP_T_Status API_ReadUTCTime(INT32U* const opUTCTime)
{
	time_t tt;
	time(&tt);

	*opUTCTime = (INT32U)(tt + 28800);

	return;
}

APP_T_Status API_WriteUTCTime(uint32_t* const ipUTCTime)
{
	return CVC_C_NO_ERROR;
}

// pda vital read
CVC_APP_PDA_STATE_t API_ReadPDAVital(INT8U* opData, INT16U iSize, INT16U* opActualSize)
{
	CVC_APP_PDA_STATE_t retState = CVC_PDA_NO_AVAILABLE;
	CVC_APP_Store_Data_t storeData = { 0U };

	//GDF_M_NULL_ASSERT(opData);
	//GDF_M_NULL_ASSERT(opActualSize);

	retState = CVC_ReadPDAVital(&storeData);
	if ((CVC_PDA_OPERATION_FAILED == retState) || (CVC_PDA_NO_AVAILABLE == retState))
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR5, retState, 0, 0, 0, 0, 0);
		return retState;
	}
	else if (CVC_PDA_OPERATION_SUCCEED == retState)
	{
		if ((iSize > MAX_NVRAM_DATA_LENGTH) || (iSize < storeData.datasize))
		{
			/*数据实际大小超出了应用buffer大小*/
			ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR17, iSize, 0, 0, 0, 0, 0);
			return CVC_PDA_OPERATION_FAILED;
		}

		*opActualSize = storeData.datasize;
		STD_F_MemcpyEx(__FILE__, __LINE__, (void*)opData, (INT32U)storeData.datasize, (const void*)storeData.data, (INT32U)storeData.datasize);
		return retState;
	}
	else
	{
		return retState;
	}
}


// pda vital status
CVC_APP_PDA_STATE_t API_GetPDAVitalStatus(void)
{
	CVC_APP_PDA_STATE_t retState = CVC_PDA_NO_AVAILABLE;
	retState = CVC_ReadPDAVitalStatus();
	return retState;
}

// pda nvital read
CVC_APP_PDA_STATE_t API_ReadPDANVital(INT8U* opData, INT16U iSize, INT16U* opActualSize)
{
	CVC_APP_PDA_STATE_t retState = CVC_PDA_NO_AVAILABLE;
	CVC_APP_Store_Data_t storeData = { 0U };

	//GDF_M_NULL_ASSERT(opData);
	//GDF_M_NULL_ASSERT(opActualSize);

	retState = CVC_ReadPDANVital(&storeData);
	if ((CVC_PDA_OPERATION_FAILED == retState) || (CVC_PDA_NO_AVAILABLE == retState))
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR6, retState, 0, 0, 0, 0, 0);
		return retState;
	}
	else if (CVC_PDA_OPERATION_SUCCEED == retState)
	{
		if ((iSize > MAX_NVRAM_DATA_LENGTH) || (iSize < storeData.datasize))
		{
			/*数据实际大小超出了应用buffer大小*/
			ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR18, iSize, 0, 0, 0, 0, 0);
			return CVC_PDA_OPERATION_FAILED;
		}

		*opActualSize = storeData.datasize;
		STD_F_MemcpyEx(__FILE__, __LINE__, (void*)opData, (INT32U)storeData.datasize, (const void*)storeData.data, (INT32U)storeData.datasize);
		return retState;
	}
	else
	{
		return retState;
	}
}


// pda nvital status
CVC_APP_PDA_STATE_t API_GetPDANVitalStatus(void)
{
	CVC_APP_PDA_STATE_t retState = CVC_PDA_NO_AVAILABLE;
	retState = CVC_ReadPDANVitalStatus();
	return retState;
}

// A helper function to set the state with proper error message logging
void SetPdaState(INT8U iFileIndex, CVC_APP_PDA_STATE_t newState)
{
	if (iFileIndex < MAX_MASS_FLASH_NUM) {
		pda_3_states[iFileIndex] = newState;
	}
}

const char* getFlashFileName(int iFileIndex)
{
	// Find the file for the given id in the configuration
	for (uint8_t i = 0; i < g_pda_storage_config.flash_count; i++)
	{
		if (g_pda_storage_config.flash[i].id == iFileIndex)
		{
			return g_pda_storage_config.flash[i].file;
		}
	}

	// Fallback to hardcoded values if not found in config
	if (iFileIndex == 0) {
		return "Plug/db.dat";
	}
	else if (iFileIndex == 1) {
		return "Plug/db2.dat";
	}

	return NULL;
}

/* configured read/write delay (ms) for a flash area; 0 when not configured */
static uint32_t getFlashDelayMs(int iFileIndex, int isWrite)
{
	for (uint8_t i = 0; i < g_pda_storage_config.flash_count; i++)
	{
		if (g_pda_storage_config.flash[i].id == iFileIndex)
		{
			return isWrite ? g_pda_storage_config.flash[i].write_ms
				: g_pda_storage_config.flash[i].read_ms;
		}
	}
	return 0U;
}

static uint64_t pda_3_read_done_at[MAX_MASS_FLASH_NUM] = { 0 };
static uint64_t pda_3_write_done_at[MAX_MASS_FLASH_NUM] = { 0 };

// pda 1 read
CVC_APP_PDA_STATE_t API_ReadNRNWData(unsigned char* opData, uint16_t iSize, uint16_t* opActualSize, unsigned char iMedia)
{
	// Validate parameters
	if (opData == NULL || opActualSize == NULL)
	{
		printf("API_ReadNRNWData: invalid parameters\n");
		return CVC_PDA_OPERATION_FAILED;
	}

	// Check size limit (max 1KB per core)
	if (iSize > PDA_CONFIG_MAX_NVRAM_SIZE)
	{
		printf("API_ReadNRNWData: iSize %d exceeds max %d\n", iSize, PDA_CONFIG_MAX_NVRAM_SIZE);
		return CVC_PDA_OPERATION_FAILED;
	}

	/* delayed read poll: succeed once read_ms elapsed (buffer filled at arm time) */
	if (g_pda_storage_config.nrnw.read_ms > 0U &&
		nrnw_state.state == CVC_PDA_OPERATION_PENDING &&
		nrnw_state.armed_op == PDA_ARM_READ)
	{
		if (!Pda_DelayElapsed(&nrnw_state))
		{
			return CVC_PDA_OPERATION_PENDING;
		}
		nrnw_state.state = CVC_PDA_OPERATION_SUCCEED;
		nrnw_state.armed_op = PDA_ARM_NONE;
		return CVC_PDA_OPERATION_SUCCEED;
	}

	// Open file
	FILE* fp = fopen(g_pda_storage_config.nrnw.file, "rb");
	if (fp == NULL)
	{
		printf("API_ReadNRNWData: failed to open file %s\n", g_pda_storage_config.nrnw.file);
		nrnw_state.fail_count++;
		if (nrnw_state.fail_count >= 3)
		{
			return CVC_PDA_NO_AVAILABLE;
		}
		return CVC_PDA_OPERATION_FAILED;
	}

	// Get file size
	fseek(fp, 0, SEEK_END);
	long fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// Check if buffer is large enough
	if (fileSize > iSize)
	{
		printf("API_ReadNRNWData: file size %ld exceeds buffer size %d\n", fileSize, iSize);
		fclose(fp);
		nrnw_state.fail_count++;
		if (nrnw_state.fail_count >= 3)
		{
			return CVC_PDA_NO_AVAILABLE;
		}
		return CVC_PDA_OPERATION_FAILED;
	}

	// Read file
	size_t bytesRead = fread(opData, 1, fileSize, fp);
	fclose(fp);

	if (bytesRead != fileSize)
	{
		printf("API_ReadNRNWData: read error, expected %ld, got %zu\n", fileSize, bytesRead);
		nrnw_state.fail_count++;
		if (nrnw_state.fail_count >= 3)
		{
			return CVC_PDA_NO_AVAILABLE;
		}
		return CVC_PDA_OPERATION_FAILED;
	}

	*opActualSize = (uint16_t)bytesRead;
	nrnw_state.fail_count = 0;
	if (g_pda_storage_config.nrnw.read_ms > 0U)
	{
		Pda_ArmPending(&nrnw_state, g_pda_storage_config.nrnw.read_ms, PDA_ARM_READ);
		return CVC_PDA_OPERATION_PENDING;
	}
	nrnw_state.state = CVC_PDA_OPERATION_SUCCEED;
	nrnw_state.armed_op = PDA_ARM_NONE;
	return CVC_PDA_OPERATION_SUCCEED;
}

// pda 1 write
CVC_APP_PDA_STATE_t API_WriteNRNWData(void* ipSource, uint16_t iSize, unsigned char iMedia)
{
	// Validate parameters
	if (ipSource == NULL)
	{
		printf("API_WriteNRNWData: invalid source pointer\n");
		return CVC_PDA_OPERATION_FAILED;
	}

	// Check size limit (max 1KB)
	if (iSize > PDA_CONFIG_MAX_NVRAM_SIZE)
	{
		printf("API_WriteNRNWData: iSize %d exceeds max %d\n", iSize, PDA_CONFIG_MAX_NVRAM_SIZE);
		return CVC_PDA_OPERATION_FAILED;
	}

	// Check if operation is in progress
	if (nrnw_state.state == CVC_PDA_OPERATION_PENDING)
	{
		return CVC_PDA_OPERATION_PENDING;
	}

	// Open file
	FILE* fp = fopen(g_pda_storage_config.nrnw.file, "wb");
	if (fp == NULL)
	{
		printf("API_WriteNRNWData: failed to open file %s\n", g_pda_storage_config.nrnw.file);
		nrnw_state.fail_count++;
		if (nrnw_state.fail_count >= 3)
		{
			return CVC_PDA_NO_AVAILABLE;
		}
		return CVC_PDA_OPERATION_FAILED;
	}

	// Write data
	size_t writtenSize = fwrite(ipSource, 1, iSize, fp);
	fclose(fp);

	if (writtenSize != iSize)
	{
		printf("API_WriteNRNWData: write error, expected %d, got %zu\n", iSize, writtenSize);
		nrnw_state.fail_count++;
		if (nrnw_state.fail_count >= 3)
		{
			return CVC_PDA_NO_AVAILABLE;
		}
		return CVC_PDA_OPERATION_FAILED;
	}

	// Set state to pending, will be checked by GetNRNWStatus (respects write_ms)
	Pda_ArmPending(&nrnw_state, g_pda_storage_config.nrnw.write_ms, PDA_ARM_WRITE);
	return CVC_PDA_OPERATION_PENDING;
}

// pda 1 status
CVC_APP_PDA_STATE_t API_GetNRNWStatus(unsigned char iMedia)
{
	// Async completion: stay pending until configured delay elapsed
	if (nrnw_state.state == CVC_PDA_OPERATION_PENDING)
	{
		if (!Pda_DelayElapsed(&nrnw_state))
		{
			return CVC_PDA_OPERATION_PENDING;
		}
		nrnw_state.state = CVC_PDA_OPERATION_SUCCEED;
		nrnw_state.fail_count = 0;
		nrnw_state.armed_op = PDA_ARM_NONE;
		return CVC_PDA_OPERATION_SUCCEED;
	}

	// Return current state
	CVC_APP_PDA_STATE_t ret = nrnw_state.state;
	nrnw_state.state = CVC_PDA_NO_OPERATION;
	return ret;
}

// pda 2 read
CVC_APP_PDA_STATE_t API_ReadARAWData(unsigned char* opData, uint16_t iSize, uint16_t* opActualSize, unsigned char iMedia)
{
	// Validate parameters
	if (opData == NULL || opActualSize == NULL)
	{
		printf("API_ReadARAWData: invalid parameters\n");
		return CVC_PDA_OPERATION_FAILED;
	}

	// Check size limit (max 1KB per core)
	if (iSize > PDA_CONFIG_MAX_DATAPLUG_SIZE)
	{
		printf("API_ReadARAWData: iSize %d exceeds max %d\n", iSize, PDA_CONFIG_MAX_DATAPLUG_SIZE);
		return CVC_PDA_OPERATION_FAILED;
	}

	/* delayed read poll: succeed once read_ms elapsed (buffer filled at arm time) */
	if (g_pda_storage_config.araw.read_ms > 0U &&
		araw_state.state == CVC_PDA_OPERATION_PENDING &&
		araw_state.armed_op == PDA_ARM_READ)
	{
		if (!Pda_DelayElapsed(&araw_state))
		{
			return CVC_PDA_OPERATION_PENDING;
		}
		araw_state.state = CVC_PDA_OPERATION_SUCCEED;
		araw_state.armed_op = PDA_ARM_NONE;
		return CVC_PDA_OPERATION_SUCCEED;
	}

	// Open file
	FILE* fp = fopen(g_pda_storage_config.araw.file, "rb");
	if (fp == NULL)
	{
		printf("API_ReadARAWData: failed to open file %s\n", g_pda_storage_config.araw.file);
		araw_state.fail_count++;
		if (araw_state.fail_count >= 3)
		{
			return CVC_PDA_NO_AVAILABLE;
		}
		return CVC_PDA_OPERATION_FAILED;
	}

	// Get file size
	fseek(fp, 0, SEEK_END);
	long fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// Check if buffer is large enough
	if (fileSize > iSize)
	{
		printf("API_ReadARAWData: file size %ld exceeds buffer size %d\n", fileSize, iSize);
		fclose(fp);
		araw_state.fail_count++;
		if (araw_state.fail_count >= 3)
		{
			return CVC_PDA_NO_AVAILABLE;
		}
		return CVC_PDA_OPERATION_FAILED;
	}

	// Read file
	size_t bytesRead = fread(opData, 1, fileSize, fp);
	fclose(fp);

	if (bytesRead != fileSize)
	{
		printf("API_ReadARAWData: read error, expected %ld, got %zu\n", fileSize, bytesRead);
		araw_state.fail_count++;
		if (araw_state.fail_count >= 3)
		{
			return CVC_PDA_NO_AVAILABLE;
		}
		return CVC_PDA_OPERATION_FAILED;
	}

	*opActualSize = (uint16_t)bytesRead;
	araw_state.fail_count = 0;
	if (g_pda_storage_config.araw.read_ms > 0U)
	{
		Pda_ArmPending(&araw_state, g_pda_storage_config.araw.read_ms, PDA_ARM_READ);
		return CVC_PDA_OPERATION_PENDING;
	}
	araw_state.state = CVC_PDA_OPERATION_SUCCEED;
	araw_state.armed_op = PDA_ARM_NONE;
	return CVC_PDA_OPERATION_SUCCEED;
}

// pda 3 read
CVC_APP_PDA_STATE_t API_ReadPDAMass(INT8U iFileIndex, INT8U* opBufferAddress, INT32U* opLength)
{
	if (opBufferAddress == NULL || opLength == NULL) {
		printf("Invalid buffer or length pointer.\n");
		// Only Write operation will affect pda_state, call API_ReadPDAMass directly to get read state
		//SetPdaState(iFileIndex, CVC_PDA_OPERATION_FAILED);
		return CVC_PDA_OPERATION_FAILED;
	}

	if (iFileIndex >= MAX_MASS_FLASH_NUM) {
		printf("Invalid Flash ID [%d]. It should be smaller than [%d].\n", iFileIndex, MAX_MASS_FLASH_NUM);
		//SetPdaState(iFileIndex, CVC_PDA_OPERATION_FAILED);
		return CVC_PDA_OPERATION_FAILED;
	}

	/* delayed read poll: succeed once read_ms elapsed (buffer filled at arm time) */
	const uint32_t readDelayMs = getFlashDelayMs(iFileIndex, 0);
	if (readDelayMs > 0U && pda_3_read_done_at[iFileIndex] != 0U) {
		if (Pda_NowMs() < pda_3_read_done_at[iFileIndex]) {
			return CVC_PDA_OPERATION_PENDING;
		}
		pda_3_read_done_at[iFileIndex] = 0U;
		return CVC_PDA_OPERATION_SUCCEED;
	}

	const char* FlashFileName = getFlashFileName(iFileIndex);
	if (FlashFileName == NULL) {
		printf("Invalid file index [%d].\n", iFileIndex);
		//SetPdaState(iFileIndex, CVC_PDA_OPERATION_FAILED);
		return CVC_PDA_OPERATION_FAILED;
	}

	FILE* fp = fopen(FlashFileName, "rb");
	if (fp == NULL) {
		printf("Failed to open flash file %s\n", FlashFileName);
		//SetPdaState(iFileIndex, CVC_PDA_OPERATION_FAILED);
		return CVC_PDA_OPERATION_FAILED;
	}

	// Get file size
	fseek(fp, 0, SEEK_END);
	size_t fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);  // Reset file pointer to the beginning.

	// Ensure we don't read more than the buffer can handle
	const size_t mapBufferSizeLimit = 8 * 1024 * 1024;
	if (fileSize > mapBufferSizeLimit) {
		printf("File size [%zu] bytes exceeds buffer limit [%zu] bytes. Truncating.\n", fileSize, mapBufferSizeLimit);
		fileSize = mapBufferSizeLimit;
	}

	// Allocate temporary buffer
	unsigned char* mapBuffer = (unsigned char*)malloc(fileSize);
	if (mapBuffer == NULL) {
		printf("Memory allocation failed for buffer of size [%zu] bytes.\n", fileSize);
		fclose(fp);
		//SetPdaState(iFileIndex, CVC_PDA_OPERATION_FAILED);
		return CVC_PDA_OPERATION_FAILED;
	}

	// Read file into buffer
	size_t bytesRead = fread(mapBuffer, 1, fileSize, fp);
	if (bytesRead != fileSize) {
		printf("Failed to read the entire file. Expected [%zu] bytes, but only [%zu] bytes were read.\n", fileSize, bytesRead);
		free(mapBuffer);
		fclose(fp);
		//SetPdaState(iFileIndex, CVC_PDA_OPERATION_FAILED);
		return CVC_PDA_OPERATION_FAILED;
	}

	// Copy data to output buffer and set the length
	memcpy(opBufferAddress, mapBuffer, bytesRead);
	*opLength = (INT32U)bytesRead;  // Set the actual length read.

	// Clean up
	free(mapBuffer);
	fclose(fp);

	if (readDelayMs > 0U)
	{
		pda_3_read_done_at[iFileIndex] = Pda_NowMs() + (uint64_t)readDelayMs;
		return CVC_PDA_OPERATION_PENDING;
	}

	// Update the state to successful operation
	//SetPdaState(iFileIndex, CVC_PDA_OPERATION_SUCCEED);
	return CVC_PDA_OPERATION_SUCCEED;
}


// pda 3 write
CVC_APP_PDA_STATE_t API_WritePDAMass(INT8U iFileIndex, INT8U* ipBufferAddress, INT32U iLength, INT32U iLineID)
{


	if (iFileIndex >= MAX_MASS_FLASH_NUM) {
		printf("Invalid Flash ID: %u. Should be less than %u.\n", iFileIndex, MAX_MASS_FLASH_NUM);
		SetPdaState(iFileIndex, CVC_PDA_OPERATION_FAILED);
		return CVC_PDA_OPERATION_FAILED;
	}

	if (ipBufferAddress == NULL || iLength == 0) {
		printf("Invalid buffer or length.\n");
		SetPdaState(iFileIndex, CVC_PDA_OPERATION_FAILED);
		return CVC_PDA_OPERATION_FAILED;
	}

	const char* FlashFileName = getFlashFileName(iFileIndex);
	if (FlashFileName == NULL) {
		printf("Invalid file index %u\n", iFileIndex);
		SetPdaState(iFileIndex, CVC_PDA_OPERATION_FAILED);
		return CVC_PDA_OPERATION_FAILED;
	}

	FILE* file = fopen(FlashFileName, "wb");
	if (file == NULL) {
		perror("Failed to open file");
		SetPdaState(iFileIndex, CVC_PDA_OPERATION_FAILED);
		return CVC_PDA_OPERATION_FAILED;
	}

	size_t writtenSize = fwrite(ipBufferAddress, 1, iLength, file);
	if (writtenSize != iLength) {
		printf("Expected to write %u bytes, but wrote %zu bytes.\n", iLength, writtenSize);
		fclose(file);
		SetPdaState(iFileIndex, CVC_PDA_OPERATION_FAILED);
		return CVC_PDA_OPERATION_FAILED;
	}

	fclose(file);

	const uint32_t writeDelayMs = getFlashDelayMs(iFileIndex, 1);
	if (writeDelayMs > 0U) {
		pda_3_write_done_at[iFileIndex] = Pda_NowMs() + (uint64_t)writeDelayMs;
		SetPdaState(iFileIndex, CVC_PDA_OPERATION_PENDING);
		return CVC_PDA_OPERATION_PENDING;
	}

	SetPdaState(iFileIndex, CVC_PDA_OPERATION_SUCCEED);
	return CVC_PDA_OPERATION_SUCCEED;

}



// pda 3 status
CVC_APP_PDA_STATE_t API_GetPDAMassStatus(INT8U iFileIndex)
{
	CVC_APP_PDA_STATE_t retState = CVC_PDA_NO_AVAILABLE;
	if (iFileIndex < MAX_MASS_FLASH_NUM) {
		retState = pda_3_states[iFileIndex];
		if (retState == CVC_PDA_OPERATION_PENDING && pda_3_write_done_at[iFileIndex] != 0U) {
			if (Pda_NowMs() < pda_3_write_done_at[iFileIndex]) {
				return CVC_PDA_OPERATION_PENDING;
			}
			pda_3_write_done_at[iFileIndex] = 0U;
			SetPdaState(iFileIndex, CVC_PDA_OPERATION_SUCCEED);
			return CVC_PDA_OPERATION_SUCCEED;
		}
	}
	return retState;
}



// pda 3 line id
void API_ReadPDAMassLineID(INT32U* opLineID)
{

	CVC_ReadPDAMassLineID(opLineID);
}

// pda 4 read
CVC_APP_PDA_STATE_t API_ReadARNWData(unsigned char* opData, uint16_t iSize, uint16_t* opActualSize, unsigned char iMedia)
{
	// Validate parameters
	if (opData == NULL || opActualSize == NULL)
	{
		printf("API_ReadARNWData: invalid parameters\n");
		return CVC_PDA_OPERATION_FAILED;
	}

	// Check size limit (max 1KB per core)
	if (iSize > PDA_CONFIG_MAX_NVRAM_SIZE)
	{
		printf("API_ReadARNWData: iSize %d exceeds max %d\n", iSize, PDA_CONFIG_MAX_NVRAM_SIZE);
		return CVC_PDA_OPERATION_FAILED;
	}

	/* delayed read poll: succeed once read_ms elapsed (buffer filled at arm time) */
	if (g_pda_storage_config.arnw.read_ms > 0U &&
		arnw_state.state == CVC_PDA_OPERATION_PENDING &&
		arnw_state.armed_op == PDA_ARM_READ)
	{
		if (!Pda_DelayElapsed(&arnw_state))
		{
			return CVC_PDA_OPERATION_PENDING;
		}
		arnw_state.state = CVC_PDA_OPERATION_SUCCEED;
		arnw_state.armed_op = PDA_ARM_NONE;
		return CVC_PDA_OPERATION_SUCCEED;
	}

	// Open file
	FILE* fp = fopen(g_pda_storage_config.arnw.file, "rb");
	if (fp == NULL)
	{
		printf("API_ReadARNWData: failed to open file %s\n", g_pda_storage_config.arnw.file);
		arnw_state.fail_count++;
		if (arnw_state.fail_count >= 3)
		{
			return CVC_PDA_NO_AVAILABLE;
		}
		return CVC_PDA_OPERATION_FAILED;
	}

	// Get file size
	fseek(fp, 0, SEEK_END);
	long fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// Check if buffer is large enough
	if (fileSize > iSize)
	{
		printf("API_ReadARNWData: file size %ld exceeds buffer size %d\n", fileSize, iSize);
		fclose(fp);
		arnw_state.fail_count++;
		if (arnw_state.fail_count >= 3)
		{
			return CVC_PDA_NO_AVAILABLE;
		}
		return CVC_PDA_OPERATION_FAILED;
	}

	// Read file
	size_t bytesRead = fread(opData, 1, fileSize, fp);
	fclose(fp);

	if (bytesRead != fileSize)
	{
		printf("API_ReadARNWData: read error, expected %ld, got %zu\n", fileSize, bytesRead);
		arnw_state.fail_count++;
		if (arnw_state.fail_count >= 3)
		{
			return CVC_PDA_NO_AVAILABLE;
		}
		return CVC_PDA_OPERATION_FAILED;
	}

	*opActualSize = (uint16_t)bytesRead;
	arnw_state.fail_count = 0;
	if (g_pda_storage_config.arnw.read_ms > 0U)
	{
		Pda_ArmPending(&arnw_state, g_pda_storage_config.arnw.read_ms, PDA_ARM_READ);
		return CVC_PDA_OPERATION_PENDING;
	}
	arnw_state.state = CVC_PDA_OPERATION_SUCCEED;
	arnw_state.armed_op = PDA_ARM_NONE;
	return CVC_PDA_OPERATION_SUCCEED;
}

// pda 4 write (dataplug file)
CVC_APP_PDA_STATE_t API_WriteARNWData(void* ipSource, uint16_t iSize, unsigned char iMedia)
{
	// Validate parameters
	if (ipSource == NULL)
	{
		printf("API_WriteARNWData: invalid source pointer\n");
		return CVC_PDA_OPERATION_FAILED;
	}

	// Check size limit (max 1KB)
	if (iSize > PDA_CONFIG_MAX_DATAPLUG_SIZE)
	{
		printf("API_WriteARNWData: iSize %d exceeds max %d\n", iSize, PDA_CONFIG_MAX_DATAPLUG_SIZE);
		return CVC_PDA_OPERATION_FAILED;
	}

	// Check if operation is in progress
	if (araw_state.state == CVC_PDA_OPERATION_PENDING)
	{
		return CVC_PDA_OPERATION_PENDING;
	}

	// Open file
	FILE* fp = fopen(g_pda_storage_config.araw.file, "wb");
	if (fp == NULL)
	{
		printf("API_WriteARNWData: failed to open file %s\n", g_pda_storage_config.araw.file);
		araw_state.fail_count++;
		if (araw_state.fail_count >= 3)
		{
			return CVC_PDA_NO_AVAILABLE;
		}
		return CVC_PDA_OPERATION_FAILED;
	}

	// Write data
	size_t writtenSize = fwrite(ipSource, 1, iSize, fp);
	fclose(fp);

	if (writtenSize != iSize)
	{
		printf("API_WriteARNWData: write error, expected %d, got %zu\n", iSize, writtenSize);
		araw_state.fail_count++;
		if (araw_state.fail_count >= 3)
		{
			return CVC_PDA_NO_AVAILABLE;
		}
		return CVC_PDA_OPERATION_FAILED;
	}

	// Set state to pending, will be checked by GetARNWStatus
	araw_state.state = CVC_PDA_OPERATION_PENDING;
	araw_state.armed_op = PDA_ARM_WRITE;
	if (g_pda_storage_config.arnw.write_ms > 0U)
	{
		/* let GetARNWStatus report this write pending until write_ms elapsed */
		Pda_ArmPending(&arnw_state, g_pda_storage_config.arnw.write_ms, PDA_ARM_WRITE);
	}
	return CVC_PDA_OPERATION_PENDING;
}

// pda 4 status
CVC_APP_PDA_STATE_t API_GetARNWStatus(unsigned char iMedia)
{
	// Async completion: stay pending until configured delay elapsed
	if (arnw_state.state == CVC_PDA_OPERATION_PENDING)
	{
		if (!Pda_DelayElapsed(&arnw_state))
		{
			return CVC_PDA_OPERATION_PENDING;
		}
		arnw_state.armed_op = PDA_ARM_NONE;
		arnw_state.state = CVC_PDA_NO_OPERATION;
		return CVC_PDA_OPERATION_SUCCEED;
	}

	// Return current state (fourth type is read-only, no write operations)
	CVC_APP_PDA_STATE_t ret = arnw_state.state;
	arnw_state.state = CVC_PDA_NO_OPERATION;
	return ret;
}

int32_t API_Printf(const char* context, int32_t arg1, int32_t arg2, int32_t arg3, int32_t arg4, int32_t arg5, int32_t arg6)
{
#ifdef API_PRINTF_IS_PRINTF
	printf(context, arg1, arg2, arg3, arg4, arg5, arg6);
#endif
	return 0;
}

static int shut_down_state = 0;

CVC_T_Status API_SetShutdown(INT16U iCode)
{
	SRV_ShutdownActiveCycle();
	printf("EVC exit reason : %d\n", (int)iCode);
	storeFatalError(iCode, 0u);
	shut_down_state = 1;
	return CVC_C_NO_ERROR;
}

int GetShutDownState()
{
	return shut_down_state;
}

CVC_T_Status API_ReadPFVersion(SysVersion_Info_t* opVersion)
{
	SysVersion_Info_t* pVerTemp = NULL;

	//GDF_M_NULL_ASSERT(opVersion);

	pVerTemp = VERSION_F_ReadPFVersion();
	STD_F_MemcpyEx(__FILE__, __LINE__, opVersion, sizeof(SysVersion_Info_t), pVerTemp, sizeof(SysVersion_Info_t));

	return CVC_C_NO_ERROR;
}

CVC_T_Status API_ExchangeCPUData(INT8U* opBufPeer, INT8U* ipBufLocal, INT16U iSize, INT16U iTimeout)
{
	//GDF_M_NULL_ASSERT(opBufPeer);
	//GDF_M_NULL_ASSERT(ipBufLocal);

	if (CVC_EXCHANGE_CPUData_LEN < iSize)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR33, 0, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}
	memcpy(opBufPeer, ipBufLocal, iSize);

	//VOTE_F_CpuExchangeExt((INT8U* const)opBufPeer, (const INT8U*)ipBufLocal, (const INT32U)iSize, (const INT16U)iTimeout, VOTE_C_FUNCTION_ID_CPU_EXCHANGE, (INT16U)INTF_BASW_C_EORROR31);

	return CVC_C_NO_ERROR;
}

CVC_T_Status API_WriteEducation(void* ipSource, INT16U iSize)
{
	BOOLEAN retBOOL = CVC_FALSE;

	//GDF_M_NULL_ASSERT(ipSource);
	if (MAX_SYN_DATA_SIZE < iSize)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR1A, 0, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}

	retBOOL = CVC_BSW_ITF_PutEducation(ipSource, (INT32U)iSize);
	if (retBOOL != CVC_TRUE)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR28, 0, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}

	return CVC_C_NO_ERROR;
}

CVC_T_Status API_ReadEducation(void* opDest, INT16U iSize, INT16U* opSize)
{
	BOOLEAN retBOOL = CVC_FALSE;

	//GDF_M_NULL_ASSERT(opDest);
	//GDF_M_NULL_ASSERT(opSize);

	retBOOL = CVC_BSW_ITF_F_GetEducation(opDest, (INT32U)iSize, (INT32U*)opSize);
	if (retBOOL != CVC_TRUE)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR27, 0, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}

	return CVC_C_NO_ERROR;
}

void API_ReadSysVSN(INT32U* opVSN0, INT32U* opVSN1, INT32U* opVSN2)
{
	// GDF_M_NULL_ASSERT(opVSN0);
	// GDF_M_NULL_ASSERT(opVSN1);
	// GDF_M_NULL_ASSERT(opVSN2);

	CVC_BSW_ITF_getVSN(opVSN0, opVSN1, opVSN2);

	return;
}

CVC_T_Status API_WriteAswInfo(ASWTxData_t* pAswTxData)
{
	BOOLEAN ret = CVC_FALSE;
	INT16U dataSize = 0u;

	//GDF_M_NULL_ASSERT(pAswTxData);

	dataSize = sizeof(ASWTxData_t) - ASW_COM_DATA_SIZE + pAswTxData->Size;
	ret = CVC_BSW_ITF_Write(CVC_BSW_ASW_COM_TX_TYPE, (INT8U*)pAswTxData, dataSize);
	if (CVC_TRUE != ret)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR35, ret, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}
	else
	{
		return CVC_C_NO_ERROR;
	}
}

CVC_T_Status API_ReadAswInfo(ASWRxData_t* pAswRxData)
{
	BOOLEAN ret = CVC_FALSE;
	INT16U dataSize = 0u;

	//GDF_M_NULL_ASSERT(pAswRxData);

	ret = CVC_BSW_ITF_Read(CVC_BSW_ASW_COM_RX_TYPE, (INT8U*)pAswRxData);
	if (CVC_BUFFER_OPER_SUCCESS != ret)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_BASW_C_EORROR36, ret, 0, 0, 0, 0, 0);
		return CVC_C_ERROR;
	}

	return CVC_C_NO_ERROR;
}

INT32U API_ReadAswInfoNum(void)
{
	INT16U num = 0U;
	BOOLEAN retBOOL = CVC_FALSE;

	retBOOL = CVC_BSW_ITF_GetMsgNum(CVC_BSW_ASW_COM_RX_TYPE, &num);
	if (CVC_FALSE == retBOOL)
	{
		return 0xFFFFFFFFU;
	}

	return (INT32U)num;
}

//change 037 networkData to aswData
BOOLEAN writePFMsg(INT8U msgID, INT8U appType, INT32U ctcsID, INT8U* buff, INT16U buffSize)
{
	PFMSG_t pfMsg_in = { 0u };
	INT16U len = 0u;

	STD_F_MemsetEx(__FILE__, __LINE__, &pfMsg_in, 0u, sizeof(pfMsg_in));

	pfMsg_in.AppType = appType;
	pfMsg_in.MsgID = msgID;
	pfMsg_in.PeripheralNumber = ctcsID;
	pfMsg_in.MsgSize = buffSize;
	if (0 < pfMsg_in.MsgSize && CVC_NULL != buff)
	{
		STD_F_MemcpyEx(__FILE__, __LINE__, pfMsg_in.Message, pfMsg_in.MsgSize, buff, pfMsg_in.MsgSize);
	}
	len = sizeof(PFMSG_t) - MAX_BSWMSG_SIZE + pfMsg_in.MsgSize;
	return CVC_BSW_ITF_Write(CVC_BSW_APP_RX_TYPE, (INT8U*)&pfMsg_in, len);

}
