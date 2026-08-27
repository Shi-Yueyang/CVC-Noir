#include "board_status.h"
#include "Interface_Data.h"
#include "bswMath.h"

#define VVB_COMMON_C_EACH_BAORD_FPGA_COUNT 2u

SysStatus_Info_t statusData = {0u};
CONF_T_MinimumSet CONF_V_MinimumSet = { 0u }; /*info of board MinimumSet*/
static void BS_InitAllBoardStatus(void);
static void BS_setBoardInfo(void);

/*init board info */
void BS_Init(void)
{
	BS_setBoardInfo();
	BS_InitAllBoardStatus();
}

/*set board number/moudID*/
static void BS_setBoardInfo(void)
{
	CONF_V_MinimumSet.mpbSet.boardNum = 1u;
	CONF_V_MinimumSet.mpbSet.modeInfo[0].modeID = 1u;

	CONF_V_MinimumSet.gwbSet.boardNum = 2u;
	CONF_V_MinimumSet.gwbSet.modeInfo[0].modeID = 2u;
	CONF_V_MinimumSet.gwbSet.modeInfo[1].modeID = 3u;

	CONF_V_MinimumSet.vvbSet.boardNum = 1u;
	CONF_V_MinimumSet.vvbSet.modeInfo[0].modeID = 4u;

	CONF_V_MinimumSet.vibSet.boardNum = 4u;
	CONF_V_MinimumSet.vibSet.modeInfo[0].modeID = 5u;
	CONF_V_MinimumSet.vibSet.modeInfo[1].modeID = 6u;
	CONF_V_MinimumSet.vibSet.modeInfo[0].modeID = 7u;
	CONF_V_MinimumSet.vibSet.modeInfo[1].modeID = 8u;

	CONF_V_MinimumSet.vobSet.boardNum = 4u;
	CONF_V_MinimumSet.vobSet.modeInfo[0].modeID = 9u;
	CONF_V_MinimumSet.vobSet.modeInfo[1].modeID = 10u;
	CONF_V_MinimumSet.vobSet.modeInfo[0].modeID = 11u;
	CONF_V_MinimumSet.vobSet.modeInfo[1].modeID = 12u;

	CONF_V_MinimumSet.btmSet.boardNum = 1u;
	CONF_V_MinimumSet.btmSet.modeInfo[0].modeID = 13u;
}

static void BS_InitAllBoardStatus(void)
{
	INT8U i = 0U;
	INT8U bdTpIdx = 0U;
	INT8U boardNum = 0U;
	INT8U modeID = 0U;
	INT8U* pBoardStatus = NULL;
	INT8U BoardTypeNum = 0U;
	INT32U boardStatus = 0U;
	SysStatus_Info_t sysStaInfo = { 0U };
	CONF_T_MinimumSetSingle* pBoardInfo = NULL;

	STD_F_MemsetEx(__FILE__, __LINE__, (void*)&sysStaInfo, (INT8U)0U, (INT32U)sizeof(SysStatus_Info_t));
	STD_F_MemsetEx(__FILE__, __LINE__, (void*)&CONF_V_MinimumSet, (INT8U)0U, (INT32U)sizeof(CONF_V_MinimumSet));

	for (bdTpIdx = 0U; bdTpIdx < (INT8U)STATUS_C_TYPE_MAX; ++bdTpIdx)
	{
		if ((INT8U)STATUS_C_TYPE_MPB == bdTpIdx)
		{
			pBoardInfo = &CONF_V_MinimumSet.mpbSet;
		}
		if ((INT8U)STATUS_C_TYPE_GWB == bdTpIdx)
		{
			pBoardInfo = &CONF_V_MinimumSet.gwbSet;
		}
		if ((INT8U)STATUS_C_TYPE_VVB == bdTpIdx)
		{
			pBoardInfo = &CONF_V_MinimumSet.vvbSet;
		}
		if ((INT8U)STATUS_C_TYPE_VIB == bdTpIdx)
		{
			pBoardInfo = &CONF_V_MinimumSet.vibSet;
		}
		if ((INT8U)STATUS_C_TYPE_VOB == bdTpIdx)
		{
			pBoardInfo = &CONF_V_MinimumSet.vobSet;
		}
		if ((INT8U)STATUS_C_TYPE_BTM == bdTpIdx)
		{
			pBoardInfo = &CONF_V_MinimumSet.btmSet;
		}

		boardNum = pBoardInfo->boardNum;
		if (0U != boardNum)
		{
			sysStaInfo.Component[bdTpIdx].TypeID = bdTpIdx;
			sysStaInfo.Component[bdTpIdx].BoardNum = boardNum;
			for (i = 0U; i < boardNum; ++i)
			{
				modeID = pBoardInfo->modeInfo[i].modeID;
				sysStaInfo.Component[bdTpIdx].BoardStatus[i].ModuleID = modeID;
				pBoardStatus = &(sysStaInfo.Component[bdTpIdx].BoardStatus[i].Status[0U]);

				if ((INT8U)STATUS_C_TYPE_MPB == bdTpIdx)
				{
					pBoardStatus[0] = 0xff;
					pBoardStatus[1] = 0xff;
					pBoardStatus[2] = 0xff;
					pBoardStatus[3] = 0xff;
					pBoardStatus[4] = 0xff;
					pBoardStatus[5] = 0x7f;//0111 1111
				}
				if ((INT8U)STATUS_C_TYPE_GWB == bdTpIdx)
				{
					pBoardStatus[0] = 0x7f;//use 1-7 bit
				}
				if ((INT8U)STATUS_C_TYPE_VVB == bdTpIdx)
				{
					pBoardStatus[0] = 0xff;
					pBoardStatus[1] = 0x0f;//9-12 bit
				}
				if (((INT8U)STATUS_C_TYPE_VIB == bdTpIdx) || ((INT8U)STATUS_C_TYPE_VOB == bdTpIdx))
				{
					pBoardStatus[0] = 0x07;//use 1-3 bit
				}
				if ((INT8U)STATUS_C_TYPE_BTM == bdTpIdx)
				{
					pBoardStatus[1] = 0x0f;//1-4 bit
				}
			}
			++BoardTypeNum;
		}
	}

	sysStaInfo.BoardTypeNum = BoardTypeNum;
	STD_F_MemsetEx(__FILE__, __LINE__, (void*)&statusData, 0x0u, (INT32U)sizeof(SysStatus_Info_t));
	STD_F_MemcpyEx(__FILE__, __LINE__, (INT8U*)&statusData, (INT32U)sizeof(SysStatus_Info_t), (INT8U*)&sysStaInfo, (INT32U)sizeof(SysStatus_Info_t));

	return;
}

/*val 0:useful 1:nouse*/
static void setStatusSomeBit(INT8U* pStatusInfo,INT8U bitIdx, INT8U val)
{
	INT8U statusIdx = 0u;
	INT8U isUseful = 0XFF;

	statusIdx = bitIdx / 8u;
	if (0U == val)
	{
		isUseful = 1 << (bitIdx % 8U);
		pStatusInfo[statusIdx] = pStatusInfo[statusIdx] | isUseful;
	}
	else
	{
		isUseful = 0XFF;
		isUseful = (1 << (bitIdx % 8U)) ^ isUseful;
		pStatusInfo[statusIdx] = pStatusInfo[statusIdx] & isUseful;
	}
	return;

}

void btm_status_proc(BTM_T_STATUS* BOARD_STATUS_V_LocalSysBTMStatus)
{
	// INT8U i = 0U;	
	BTM_T_STATUS* pBTMStatus = NULL;
	INT8U* pBoardStatus = NULL;

	// GDF_M_NULL_ASSERT(pBoardStatus);
	pBTMStatus = BOARD_STATUS_V_LocalSysBTMStatus;
	pBoardStatus = statusData.Component[STATUS_C_TYPE_BTM].BoardStatus[0].Status;

	// i = modeID/16U;
	if (BTM_C_NORMAL_MODE == pBTMStatus->BTMStatus)
	{
		pBoardStatus[0U] = pBoardStatus[0U] | 0x01U;
	}
	else
	{
		return;
	}
	if (BTM_C_NORMAL_MODE == pBTMStatus->SP1Status)
	{
		pBoardStatus[0U] = pBoardStatus[0U] | (0x01U << 1U);
	}
	if (BTM_C_NORMAL_MODE == pBTMStatus->SP2Status)
	{
		pBoardStatus[0U] = pBoardStatus[0U] | (0x01U << 2U);
	}
	if (BTM_C_NORMAL_MODE == pBTMStatus->ANTStatus)
	{
		pBoardStatus[0U] = pBoardStatus[0U] | (0x01U << 3U);
	}

	return;
}

// sensorState update
void vvb_SpeedSensorStatus_proc(INT8U* SpeedSensorStatus, INT8U statusSize,INT8U boradIdxInVVB)
{
	INT8U* pStatusInfo = NULL;
	INT8U i = 0u;
	INT8U bitIdx = 0u;
	INT8U SensorUse = 0u;

	if (SpeedSensorStatus == NULL) 
	{
		return;
	}
	SensorUse = statusSize;
	if (SensorUse > CVC_CH_SERIAL_NUM)
	{
		SensorUse = CVC_CH_SERIAL_NUM;

	}
	pStatusInfo = statusData.Component[STATUS_C_TYPE_VVB].BoardStatus[boradIdxInVVB].Status;
	/*SPEED STATE 4-6 bit*/
	for (i = 0U; i < SensorUse; ++i)
	{
		bitIdx = 4u + i;
		setStatusSomeBit(pStatusInfo, bitIdx, SpeedSensorStatus[i]);
	}

	return;
}

/*AccRaderStatus update*/
void vvb_AccRaderStatus_proc(INT8U* AccRaderStatus,INT8U statusSize,INT8U boradIdxInVVB)
{
	INT8U* pStatusInfo = NULL;
	INT8U i = 0u;
	INT8U bitIdx = 0u;
	INT8U SensorUse = 0u;

	if (AccRaderStatus == NULL) 
	{
		return;
	}

	SensorUse = statusSize;
	if (SensorUse > CVC_CH_SERIAL_NUM * 2u)
	{
		SensorUse = CVC_CH_SERIAL_NUM * 2u;

	}
	pStatusInfo = statusData.Component[STATUS_C_TYPE_VVB].BoardStatus[boradIdxInVVB].Status;
	/*add state 7-9 10-12 bit*/
	for (i = 0U; i < SensorUse; ++i)
	{
		bitIdx = 7u + i;
		setStatusSomeBit(pStatusInfo, bitIdx, AccRaderStatus[i]);
	}

	return;
}

