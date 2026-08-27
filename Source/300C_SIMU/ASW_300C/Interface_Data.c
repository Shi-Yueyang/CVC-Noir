#include "Interface_Data.h"
#include "bswMath.h"
#include "pda.h"
#include "errorProc.h"

INT8U BTM_SetUsedAntennaID = 0xFFu;/*0:BTM_A;1:BTM_B*/

static CVC_BSW_Pool_Struct INTERFACE_DATA_V_BSW_pool = { 0u };
static CVC_BSW_Index_Manager_Struct BSW_index_manager = { 0u };
static INT8U INTERFACE_DATA_V_MantBuf[MAX_MANT_BUF_SIZE] = { 0u };
static CVC_MANTINFO_Index_Struct MantInfo_Index_Manager = { 0u };

IPC_COM_T_MSG IPC_COM_V_ToASWSynMsg = { 0u }; /*store syndata*/
INT32U SYN_DATA_V_AswDataRxCurSize = 0u;

static BOOLEAN CVC_BSW_Get_MsgIndex(CVC_BSW_MSG_TYPE_ENUM msgType, INT16U *pIndex);
static INT32S CVC_BSW_Read_Msg_ByIndex(INT16U index, CVC_BSW_MSG_TYPE_ENUM msgType, INT8U *pMsg);
static BOOLEAN CVC_BSW_Add_MsgType(CVC_BSW_MSG_TYPE_ENUM msgType, INT16U num);
static INT16U CVC_BSW_Get_MsgTypeSize(CVC_BSW_MSG_TYPE_ENUM msgType);

INT16U INTERFACE_DATA_V_AppRunTime = 0U;
INT64U INTERFACE_DATA_V_CurTime = 0U;
INT32U INTERFACE_DATA_V_CurTimeTemp = 0U;

static INT64U NTERFACE_DATA_V_sysCurrentTime = 0u;
static INT32U VSN0 = 0u;
static INT32U VSN1 = 0u;
static INT32U VSN2 = 0u;

/// <summary>
/// init mempool
/// </summary>
/// <param name=""></param>
/// <returns></returns>
BOOLEAN CVC_BSW_ITF_Init(void)
{
	INT32U i = 0U;
	CVC_T_Status retStatus = CVC_C_ERROR;
	BOOLEAN retBOOL = CVC_FALSE;
	//struct mutex_attr mta = {0};

	/*INIT memPool*/
	for(i = 0U; i < MAX_BSWMSG_NUM; ++i)
	{
		STD_F_MemsetEx(__FILE__,__LINE__,(void *)&(INTERFACE_DATA_V_BSW_pool.GMsg[i]), (INT8U)0U, (INT32U)sizeof(CVC_BSW_Msg_Struct));
	}

	for(i = 0U; i < MAX_BSWMSG_IDXMGR_NUM; ++i)
	{
		STD_F_MemsetEx(__FILE__,__LINE__,(void *)&(BSW_index_manager.GMsgIndex[i]), (INT8U)0U, (INT32U)sizeof(CVC_BSW_Msg_Index_Struct));
		// mta.ceiling = 255U;
		// mta.flags = MUTEX_PRIO_INHERIT;
		/*
		retStatus = OSW_F_MutexCreate(BSW_index_manager.GMsgIndex[i].Singal_Flag);
		if(CVC_C_NO_ERROR!=retStatus)
		{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR0, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
		}
		*/
	}

	INTERFACE_DATA_V_BSW_pool.curPos = 0U;
	BSW_index_manager.curPos = 0U;

	/*添加消息类型*/
	retBOOL = CVC_BSW_Add_MsgType(CVC_BSW_APP_RX_TYPE, (INT16U)CVC_RX_APP_NUM_MAX);
	if(CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR11, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	retBOOL = CVC_BSW_Add_MsgType(CVC_BSW_APP_TX_TYPE, (INT16U)CVC_TX_APP_NUM_MAX);
	if(CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR12, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	retBOOL = CVC_BSW_Add_MsgType(CVC_BSW_MANT_RX_TYPE, (INT16U)CVC_RX_ADD_NUM_MAX);
	if(CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR13, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	retBOOL = CVC_BSW_Add_MsgType(CVC_BSW_MANT_TX_TYPE, (INT16U)CVC_TX_ADD_NUM_MAX);
	if(CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR14, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	retBOOL = CVC_BSW_Add_MsgType(CVC_BSW_VIB_RX_TYPE, (INT16U)CVC_RX_ADD_NUM_MAX);
	if(CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR15, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	retBOOL = CVC_BSW_Add_MsgType(CVC_BSW_VOB_TX_TYPE, (INT16U)CVC_TX_ADD_NUM_MAX);
	if(CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR16, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	retBOOL = CVC_BSW_Add_MsgType(CVC_BSW_VVB_RX_TYPE, (INT16U)CVC_RX_ADD_NUM_MAX);
	if(CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR17, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	retBOOL = CVC_BSW_Add_MsgType(CVC_BSW_BTM_RX_TYPE, (INT16U)CVC_RX_ADD_NUM_MAX);
	if(CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR19, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	retBOOL = CVC_BSW_Add_MsgType(CVC_BSW_VTS_TX_TYPE, (INT16U)CVC_TX_ADD_NUM_MAX);
	if(CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR1A, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	retBOOL = CVC_BSW_Add_MsgType(CVC_BSW_STA_RX_TYPE, (INT16U)CVC_RX_ADD_NUM_MAX);
	if(CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR1F, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	retBOOL = CVC_BSW_Add_MsgType(CVC_BSW_ASW_COM_TX_TYPE, (INT16U)CVC_TX_ADD_NUM_MAX);
	if (CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR22, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	retBOOL = CVC_BSW_Add_MsgType(CVC_BSW_ASW_COM_RX_TYPE, (INT16U)CVC_RX_ADD_NUM_MAX);
	if (CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR23, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	/*持续写入的维护消息内存初始化*/
	STD_F_MemsetEx(__FILE__,__LINE__,(void *)INTERFACE_DATA_V_MantBuf, (INT8U)0U, (INT32U)MAX_MANT_BUF_SIZE);

	// mta.ceiling = 255u;
	// mta.flags = MUTEX_PRIO_INHERIT;	

	// retStatus = OSW_F_MutexCreate(MantInfo_Index_Manager.Singal_Flag);
	// if(CVC_C_NO_ERROR!=retStatus)
	// {
	// 	ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR1B, 0, 0, 0, 0, 0, 0);
	// 	return CVC_FALSE;
	// }

	MantInfo_Index_Manager.readPos = 0U;
	MantInfo_Index_Manager.writePos = 0U;
	MantInfo_Index_Manager.msgNum = 0U;

	return CVC_TRUE;
}

BOOLEAN CVC_BSW_Add_MsgType(CVC_BSW_MSG_TYPE_ENUM msgType, INT16U num)
{

	INT32U i = 0U;

	/*检查该消息类型是否已经存在*/
	for(i = 0U; i < BSW_index_manager.curPos; ++i)
	{
		if(BSW_index_manager.GMsgIndex[i].msgType == msgType)
		{
			break;
		}
	}

	if(i < BSW_index_manager.curPos) /*该消息类型已经存�?*/
	{
		return CVC_TRUE;
	}
	else
	{
		if((INTERFACE_DATA_V_BSW_pool.curPos + num) > MAX_BSWMSG_NUM)
		{
			ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR1, 0, 0, 0, 0, 0, 0);
			return CVC_FALSE;
		}

		/*添加一种消息类�?*/
		BSW_index_manager.GMsgIndex[BSW_index_manager.curPos].msgType = msgType;
		BSW_index_manager.GMsgIndex[BSW_index_manager.curPos].startPos = INTERFACE_DATA_V_BSW_pool.curPos;
		BSW_index_manager.GMsgIndex[BSW_index_manager.curPos].endPos = INTERFACE_DATA_V_BSW_pool.curPos + num - 1U;
		BSW_index_manager.GMsgIndex[BSW_index_manager.curPos].readPos = BSW_index_manager.GMsgIndex[BSW_index_manager.curPos].startPos;
		BSW_index_manager.GMsgIndex[BSW_index_manager.curPos].writePos = BSW_index_manager.GMsgIndex[BSW_index_manager.curPos].startPos;
		BSW_index_manager.GMsgIndex[BSW_index_manager.curPos].msgNum = 0U;
		/*更新内存池当前的位置*/
		INTERFACE_DATA_V_BSW_pool.curPos = BSW_index_manager.GMsgIndex[BSW_index_manager.curPos].endPos + 1U;

		++(BSW_index_manager.curPos);
	}

	return CVC_TRUE;
}

static BOOLEAN CVC_BSW_Get_MsgIndex(CVC_BSW_MSG_TYPE_ENUM msgType, INT16U *pIndex)
{
	INT16U i = 0U;

	//GDF_M_NULL_ASSERT(pIndex);

	for(i = 0U; i < BSW_index_manager.curPos; ++i)
	{
		if(msgType == BSW_index_manager.GMsgIndex[i].msgType)
		{
			break;
		}
	}

	if(i == BSW_index_manager.curPos)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR3, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}
	else
	{
		*pIndex = i;
	}

	return CVC_TRUE;
}

static INT16U CVC_BSW_Get_MsgTypeSize(CVC_BSW_MSG_TYPE_ENUM msgType)
{
	if((CVC_BSW_APP_RX_TYPE == msgType)||(CVC_BSW_APP_TX_TYPE == msgType))
	{
		return (INT16U)sizeof(PFMSG_t);
	}
	else if((CVC_BSW_MANT_RX_TYPE == msgType)||(CVC_BSW_MANT_TX_TYPE == msgType))
	{
		return (INT16U)sizeof(MANTMSG_t);
	}
	else if((CVC_BSW_VIB_RX_TYPE == msgType)||(CVC_BSW_VOB_TX_TYPE == msgType))
	{
		return (INT16U)sizeof(IOData_t);
	}
	else if(CVC_BSW_VVB_RX_TYPE == msgType)
	{
		return (INT16U)sizeof(VVBData_t);
	}
	else if(CVC_BSW_BTM_RX_TYPE == msgType)
	{
		//return (INT16U)sizeof(BTMData_t);
		return (INT16U)sizeof(APP_BTMData_t);
	}
	else if(CVC_BSW_VTS_TX_TYPE == msgType)
	{
		return (INT16U)sizeof(VTSData_t);
	}
	else if(CVC_BSW_STA_RX_TYPE == msgType)
	{
		return (INT16U)sizeof(SysStatus_Info_t);
	}
	else if (CVC_BSW_ASW_COM_TX_TYPE == msgType)
	{
		return (INT16U)sizeof(ASWTxData_t);
	}
	else if (CVC_BSW_ASW_COM_RX_TYPE == msgType)
	{
		return (INT16U)sizeof(ASWRxData_t);
	}
	else
	{
		return (INT16U)0U;
	}
}

static INT32S CVC_BSW_Read_Msg_ByIndex(INT16U index, CVC_BSW_MSG_TYPE_ENUM msgType, INT8U *pMsg)
{
	INT16U pos = 0U;
	INT16U msgSize = 0U;
	CVC_T_Status retStatus = CVC_C_ERROR;

	//GDF_M_NULL_ASSERT(pMsg);

	msgSize = CVC_BSW_Get_MsgTypeSize(msgType);
	if((index >= MAX_BSWMSG_IDXMGR_NUM)||(0U==msgSize))
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR4, 0, 0, 0, 0, 0, 0);
		return CVC_BUFFER_OPER_FAILED;
	}

	// retStatus = OSW_F_MutexPend(BSW_index_manager.GMsgIndex[index].Singal_Flag, (INT16U)50U, CVC_TRUE);
	// if(CVC_C_NO_ERROR != retStatus)
	// {
	// 	ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR5, 0, 0, 0, 0, 0, 0);
	// 	OSW_F_MutexPost(BSW_index_manager.GMsgIndex[index].Singal_Flag);
	// 	return CVC_C_ERROR;
	// }

	pos = BSW_index_manager.GMsgIndex[index].readPos;

	if(pos > BSW_index_manager.GMsgIndex[index].endPos)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR6, 0, 0, 0, 0, 0, 0);
		// OSW_F_MutexPost(BSW_index_manager.GMsgIndex[index].Singal_Flag);
		return CVC_BUFFER_OPER_FAILED;
	}

	/*read the message */
	if((CVC_TRUE == INTERFACE_DATA_V_BSW_pool.GMsg[pos].bRecieved) && (BSW_index_manager.GMsgIndex[index].msgNum > 0U))
	{
		STD_F_MemcpyEx(__FILE__,__LINE__,(void*)pMsg, (INT32U)msgSize, (const void*)&(INTERFACE_DATA_V_BSW_pool.GMsg[pos].gMsg), (INT32U)msgSize);
		STD_F_MemsetEx(__FILE__,__LINE__,&(INTERFACE_DATA_V_BSW_pool.GMsg[pos].gMsg),(INT8U)0U,(INT32U)msgSize);
		INTERFACE_DATA_V_BSW_pool.GMsg[pos].bRecieved = CVC_FALSE;

		--BSW_index_manager.GMsgIndex[index].msgNum;
		BSW_index_manager.GMsgIndex[index].readPos = BSW_index_manager.GMsgIndex[index].startPos + (BSW_index_manager.GMsgIndex[index].readPos -
			BSW_index_manager.GMsgIndex[index].startPos + 1U)%(BSW_index_manager.GMsgIndex[index].endPos - BSW_index_manager.GMsgIndex[index].startPos + 1U);

		// OSW_F_MutexPost(BSW_index_manager.GMsgIndex[index].Singal_Flag);
	}
	else
	{
		// OSW_F_MutexPost(BSW_index_manager.GMsgIndex[index].Singal_Flag);
		return CVC_BUFFER_OPER_EMPTY;
	}

	return CVC_BUFFER_OPER_SUCCESS;
}

BOOLEAN CVC_BSW_ITF_GetMsgNum(CVC_BSW_MSG_TYPE_ENUM msgType, INT16U *pNum)
{

	BOOLEAN retBOOL=CVC_FALSE;
	CVC_T_Status retStatus = CVC_C_ERROR;
	INT16U index = 0U;

	//GDF_M_NULL_ASSERT(pNum);

	retBOOL =CVC_BSW_Get_MsgIndex(msgType, &index);
	if(CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR8, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;

	}

	// retStatus = OSW_F_MutexPend(BSW_index_manager.GMsgIndex[index].Singal_Flag, (INT16U)50U, CVC_TRUE);
	// if(CVC_C_NO_ERROR != retStatus)
	// {
	// 	ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR9, 0, 0, 0, 0, 0, 0);
	// 	OSW_F_MutexPost(BSW_index_manager.GMsgIndex[index].Singal_Flag);
	// 	return CVC_FALSE;
	// }

	*pNum = BSW_index_manager.GMsgIndex[index].msgNum;
	// OSW_F_MutexPost(BSW_index_manager.GMsgIndex[index].Singal_Flag);

	return CVC_TRUE;
}

INT32S CVC_BSW_ITF_Read(CVC_BSW_MSG_TYPE_ENUM msgType, INT8U *pMsg)
{

	INT16U index = 0U;
	BOOLEAN retBOOL=CVC_FALSE;
	INT32S retValue = CVC_BUFFER_OPER_FAILED;

	//GDF_M_NULL_ASSERT(pMsg);

	retBOOL = CVC_BSW_Get_MsgIndex(msgType, &index);
	if(CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORRORB, 0, 0, 0, 0, 0, 0);
		return CVC_BUFFER_OPER_FAILED;
	}

	retValue = CVC_BSW_Read_Msg_ByIndex(index, msgType, pMsg);

	return retValue;
}

BOOLEAN CVC_BSW_ITF_Write(CVC_BSW_MSG_TYPE_ENUM msgType, INT8U *pData, INT16U dataSize)
{
	BOOLEAN retBOOL = CVC_FALSE;
	CVC_T_Status retStatus = CVC_C_ERROR;
	INT16U index = 0U;
	INT16U msgSize = 0U;
	INT32U pos = 0U;

	//GDF_M_NULL_ASSERT(pData);

	msgSize = CVC_BSW_Get_MsgTypeSize(msgType);
	if((0U == msgSize) || (dataSize > msgSize))
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORRORC, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	retBOOL = CVC_BSW_Get_MsgIndex(msgType, &index);
	if(CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORRORD, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	// retStatus = OSW_F_MutexPend(BSW_index_manager.GMsgIndex[index].Singal_Flag, (INT16U)50U, CVC_TRUE);
	// if(CVC_C_NO_ERROR != retStatus)
	// {
	// 	ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORRORE, 0, 0, 0, 0, 0, 0);
	// 	OSW_F_MutexPost(BSW_index_manager.GMsgIndex[index].Singal_Flag);
	// 	return CVC_FALSE;
	// }

	pos = BSW_index_manager.GMsgIndex[index].writePos;

	if(pos > BSW_index_manager.GMsgIndex[index].endPos)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORRORF, 0, 0, 0, 0, 0, 0);
		// OSW_F_MutexPost(BSW_index_manager.GMsgIndex[index].Singal_Flag);
		return CVC_FALSE;
	}

	STD_F_MemcpyEx(__FILE__,__LINE__,(void*)&(INTERFACE_DATA_V_BSW_pool.GMsg[pos].gMsg), (INT32U)dataSize, (const void*)pData, (INT32U)dataSize);
	INTERFACE_DATA_V_BSW_pool.GMsg[pos].bRecieved = CVC_TRUE;

	if(BSW_index_manager.GMsgIndex[index].msgNum < (BSW_index_manager.GMsgIndex[index].endPos - BSW_index_manager.GMsgIndex[index].startPos + 1U))
	{
		/*消息条数小于最上限*/
		++BSW_index_manager.GMsgIndex[index].msgNum;

		BSW_index_manager.GMsgIndex[index].writePos = BSW_index_manager.GMsgIndex[index].startPos + (BSW_index_manager.GMsgIndex[index].writePos -
			BSW_index_manager.GMsgIndex[index].startPos + 1U)%(BSW_index_manager.GMsgIndex[index].endPos - BSW_index_manager.GMsgIndex[index].startPos + 1U);
	}
	else
	{
		/*消息条数已满，覆盖原数据，读写指针均向后偏移一�?*/
		BSW_index_manager.GMsgIndex[index].writePos = BSW_index_manager.GMsgIndex[index].startPos + (BSW_index_manager.GMsgIndex[index].writePos -
			BSW_index_manager.GMsgIndex[index].startPos + 1U)%(BSW_index_manager.GMsgIndex[index].endPos - BSW_index_manager.GMsgIndex[index].startPos + 1U);
		BSW_index_manager.GMsgIndex[index].readPos = BSW_index_manager.GMsgIndex[index].startPos + (BSW_index_manager.GMsgIndex[index].readPos -
			BSW_index_manager.GMsgIndex[index].startPos + 1U)%(BSW_index_manager.GMsgIndex[index].endPos - BSW_index_manager.GMsgIndex[index].startPos + 1U);
	}

	// OSW_F_MutexPost(BSW_index_manager.GMsgIndex[index].Singal_Flag);

	return CVC_TRUE;
}

BOOLEAN CVC_BSW_ITF_Reset(CVC_BSW_MSG_TYPE_ENUM msgType)
{
	BOOLEAN retBOOL = CVC_FALSE;
	INT16U i = 0U;
	INT16U index = 0U;
	INT16U StrIdx = 0U;
	INT16U EndIdx = 0U;

	retBOOL =CVC_BSW_Get_MsgIndex(msgType, &index);
	if(CVC_FALSE == retBOOL)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR10, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;

	}
	else
	{
		StrIdx = BSW_index_manager.GMsgIndex[index].startPos;
		EndIdx = BSW_index_manager.GMsgIndex[index].endPos;
		for(i = StrIdx; i <= EndIdx; ++i)
		{
			INTERFACE_DATA_V_BSW_pool.GMsg[i].bRecieved = CVC_FALSE;
			STD_F_MemsetEx(__FILE__,__LINE__,&(INTERFACE_DATA_V_BSW_pool.GMsg[i].gMsg),(INT8U)0U,(INT32U)MAX_BSWBUF_SIZE);
		}

		BSW_index_manager.GMsgIndex[index].msgNum = 0U;
		BSW_index_manager.GMsgIndex[index].readPos = BSW_index_manager.GMsgIndex[index].startPos;
		BSW_index_manager.GMsgIndex[index].writePos = BSW_index_manager.GMsgIndex[index].startPos;
	}

	return CVC_TRUE;
}

BOOLEAN CVC_BSW_ITF_MantInfo_Read(INT8U *pMantInfo)
{
	CVC_T_Status retStatus = CVC_C_ERROR;
	MANTINFO_HEAD_t *pMantInfoHead = NULL;

	//GDF_M_NULL_ASSERT(pMantInfo);

	if(0U!=MantInfo_Index_Manager.msgNum)
	{
		// retStatus = OSW_F_MutexPend(MantInfo_Index_Manager.Singal_Flag, (INT16U)50U, CVC_TRUE);
		// if(CVC_C_NO_ERROR != retStatus)
		// {
		// 	ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR1D, 0, 0, 0, 0, 0, 0);
		// 	OSW_F_MutexPost(MantInfo_Index_Manager.Singal_Flag);
		// 	return CVC_FALSE;
		// }

		pMantInfoHead = (MANTINFO_HEAD_t*)&INTERFACE_DATA_V_MantBuf[MantInfo_Index_Manager.readPos];
		STD_F_MemcpyEx(__FILE__,__LINE__,(void*)pMantInfo, (INT32U)(sizeof(MANTINFO_HEAD_t)+pMantInfoHead->MsgSize), (const void*)&INTERFACE_DATA_V_MantBuf[MantInfo_Index_Manager.readPos], (INT32U)(sizeof(MANTINFO_HEAD_t)+pMantInfoHead->MsgSize));
		MantInfo_Index_Manager.readPos = MantInfo_Index_Manager.readPos+(INT16U)(sizeof(MANTINFO_HEAD_t)+pMantInfoHead->MsgSize);
		--MantInfo_Index_Manager.msgNum;

		/*应用每周期最多写MAX_MANT_BUF_SIZE字节的维护数据，如果全部读出来后就重置buffer*/
		if(0U == MantInfo_Index_Manager.msgNum)
		{
			MantInfo_Index_Manager.readPos = 0U;
			MantInfo_Index_Manager.writePos = 0U;
			STD_F_MemsetEx(__FILE__,__LINE__,(void *)INTERFACE_DATA_V_MantBuf, (INT8U)0U, (INT32U)MAX_MANT_BUF_SIZE);
		}
		// OSW_F_MutexPost(MantInfo_Index_Manager.Singal_Flag);
	}

	return CVC_TRUE;
}

BOOLEAN CVC_BSW_ITF_MantInfo_Write(INT32U PeripheralNumber, INT16U mantInfoLen, INT8U *pMantInfo, INT8U type)
{
	INT16U writeLen = 0U;
	CVC_T_Status retStatus = CVC_C_ERROR;
	MANTINFO_HEAD_t mantInfoHead = {0};

	//GDF_M_NULL_ASSERT(pMantInfo);

	if(mantInfoLen>MAX_MANT_MSG_SIZE)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR20, writeLen, MAX_MANT_BUF_SIZE, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	writeLen = MantInfo_Index_Manager.writePos+mantInfoLen+(INT16U)sizeof(MANTINFO_HEAD_t);
	if(MAX_MANT_BUF_SIZE<writeLen)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR1C, writeLen, MAX_MANT_BUF_SIZE, 0, 0, 0, 0);
		return CVC_FALSE;
	}
	else if(MantInfo_Index_Manager.msgNum>=MAX_MANT_MSG_NUM)
	{
		ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR21, writeLen, MAX_MANT_BUF_SIZE, 0, 0, 0, 0);
		return CVC_FALSE;
	}
	else
	{
		// retStatus = OSW_F_MutexPend(MantInfo_Index_Manager.Singal_Flag, (INT16U)50U, CVC_TRUE);
		// if(CVC_C_NO_ERROR != retStatus)
		// {
		// 	ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR1D, 0, 0, 0, 0, 0, 0);
		// 	OSW_F_MutexPost(MantInfo_Index_Manager.Singal_Flag);
		// 	return CVC_FALSE;
		// }

		mantInfoHead.PeripheralNumber = PeripheralNumber;
		mantInfoHead.MsgSize = mantInfoLen;
		mantInfoHead.type = type;
#ifdef CVC_CONF_CPU_A
		mantInfoHead.cpuID = 0x5Au;
#elif CVC_CONF_CPU_B
		mantInfoHead.cpuID = 0xA5u;
#endif
#ifdef CVC_CONF_CPU_R50
		mantInfoHead.coreID = 0x5Au;
#elif CVC_CONF_CPU_R51
		mantInfoHead.coreID = 0xA5u;
#endif

		STD_F_MemcpyEx(__FILE__,__LINE__,(void*)&INTERFACE_DATA_V_MantBuf[MantInfo_Index_Manager.writePos], (INT32U)sizeof(MANTINFO_HEAD_t), (const void*)&mantInfoHead, (INT32U)sizeof(MANTINFO_HEAD_t));
		MantInfo_Index_Manager.writePos = MantInfo_Index_Manager.writePos+(INT16U)sizeof(MANTINFO_HEAD_t);
		STD_F_MemcpyEx(__FILE__,__LINE__,(void*)&INTERFACE_DATA_V_MantBuf[MantInfo_Index_Manager.writePos], (INT32U)mantInfoLen, (const void*)pMantInfo, (INT32U)mantInfoLen);
		MantInfo_Index_Manager.writePos = MantInfo_Index_Manager.writePos+(INT16U)mantInfoLen;
		++MantInfo_Index_Manager.msgNum;

		// OSW_F_MutexPost(MantInfo_Index_Manager.Singal_Flag);
	}

	return CVC_TRUE;
}

BOOLEAN CVC_BSW_ITF_MantInfo_GetMsgNum(INT16U *pNum)
{
	CVC_T_Status retStatus = CVC_C_ERROR;

	//GDF_M_NULL_ASSERT(pNum);

	if(0U!=MantInfo_Index_Manager.msgNum)
	{	
		*pNum = MantInfo_Index_Manager.msgNum;
	}
	else
	{
		//ERROR_PROCESS_F_ErrMsg_Add(ERROR_PROCESS_C_ALARM_NO_FATAL, INTF_DATA_C_EORROR1E, 0, 0, 0, 0, 0, 0);
		return CVC_FALSE;
	}

	return CVC_TRUE;
}

void CVC_BSW_ITF_CalCurTime(INT32U iCurTime)
{
	INT64U curTime = 0U;
	static INT32U curTimeTemp = 0U;

	INTERFACE_DATA_V_CurTimeTemp = iCurTime;
	curTime = (INTERFACE_DATA_V_CurTimeTemp-curTimeTemp+1U)+0xFFFFFFFFU;
	curTime = curTime%0xFFFFFFFFU;
	INTERFACE_DATA_V_CurTime = INTERFACE_DATA_V_CurTime+(curTime/100U);

	curTimeTemp = INTERFACE_DATA_V_CurTimeTemp;

	return;
}

void CVC_BSW_ITF_SetUsedAntennaID(INT8U iAntennaID)
{
	BTM_SetUsedAntennaID = iAntennaID;
	return;
}

void CVC_BSW_ITF_writeSysCurrentTime(INT64U time)
{
	NTERFACE_DATA_V_sysCurrentTime = time;
}

void CVC_BSW_ITF_getVSN(INT32U* V0,INT32U* V1,INT32U* V2)
{
	*V0 = VSN0;
	*V1 = VSN1;
	*V2 = VSN2;
}

INT32U CVC_BSW_ITF_getVSN0() 
{
	return VSN0;
}
void CVC_BSW_ITF_writeVSN(INT32U V0,INT32U V1,INT32U V2)
{
	VSN0 = V0;
	VSN0 = V1;
	VSN0 = V2;
}

void CVC_BSW_ITF_updateVSN(void)
{
	VSN0++;
	VSN1++;
	VSN2++;
}

BOOLEAN CVC_BSW_ITF_PutEducation(void* Source, INT32U Size)
{
	//GDF_M_NULL_ASSERT(Source);

	if ((Size + IPC_COM_V_ToASWSynMsg.head.len) > SYN_DATA_C_ASWSYNDATA_SIZE_MAX) /*应用同步数据超过平台支持的性能*/
	{
		return CVC_FALSE;
	}

	IPC_COM_V_ToASWSynMsg.synDatas[0] = 0xFFu;
	STD_F_MemcpyEx(__FILE__, __LINE__, (void*)(&(IPC_COM_V_ToASWSynMsg.synDatas[IPC_COM_V_ToASWSynMsg.head.len + 1u])), SYN_DATA_C_ASWSYNDATA_SIZE_MAX - IPC_COM_V_ToASWSynMsg.head.len, (const void*)Source, Size);
	IPC_COM_V_ToASWSynMsg.head.len += Size;

	return CVC_TRUE;
}

void CVC_BSW_ITF_F_RestASWSynMsgBuffer(void)
{
	STD_F_MemsetEx(__FILE__, __LINE__, &IPC_COM_V_ToASWSynMsg, 0x0u, sizeof(IPC_COM_V_ToASWSynMsg));
	return;
}

BOOLEAN CVC_BSW_ITF_F_GetEducation(void* Dest, INT32U size, INT32U* pSize)
{
	BOOLEAN ret = CVC_FALSE;
	INT32U dataSize = 0u;

	//GDF_M_NULL_ASSERT(Dest);
	//GDF_M_NULL_ASSERT(pSize);

	while (CVC_TRUE == CVC_TRUE)
	{
		if (IPC_COM_V_ToASWSynMsg.head.len > 1u)
		{
			dataSize = (IPC_COM_V_ToASWSynMsg.head.len - 1u) - SYN_DATA_V_AswDataRxCurSize;
			if (dataSize > size)
			{
				/*数据实际大小超出了应用给的缓存区大小*/
				ret = CVC_FALSE;
				break;
			}
			if ((IPC_COM_V_ToASWSynMsg.head.len - 1u) > SYN_DATA_C_ASWSYNDATA_SIZE_MAX)
			{
				ret = CVC_FALSE;
				break;
			}

			*pSize = dataSize;
			STD_F_MemcpyEx(__FILE__, __LINE__, (void*)Dest, dataSize, (const void*)&(IPC_COM_V_ToASWSynMsg.synDatas[SYN_DATA_V_AswDataRxCurSize + 1u]), dataSize);
			SYN_DATA_V_AswDataRxCurSize += dataSize;

			ret = CVC_TRUE;
			break;
		}
		else if (1u == IPC_COM_V_ToASWSynMsg.head.len)
		{
			*pSize = 0u;

			ret = CVC_TRUE;
			break;
		}
		else
		{
			ret = CVC_FALSE;
			break;
		}
	}

	return ret;
}