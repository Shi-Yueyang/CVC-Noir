#include "application.h"
#include "interface_p2a.h"
//#include "../BSW_Code/bswTime.h"

#define CVC_MAINT_MSG_SIZE               2048U

BYTE_8 mantMsg[CVC_MAINT_MSG_SIZE] = { 0U };
//
//void APP_Memcpy(void *destination, UINT_32 length, const void *source, UINT_32 size)
//{
//	
//	if((NULL == destination) || (NULL == source)||(size>length))
//	{
//		return;
//	}
//	
//	if((UINT_32)0U == size)
//	{
//		return;
//	}
//	
//	while(size--)
//	{
//		*(char *)destination = *(char *)source;
//		destination = (char *)destination + 1;
//		source = (char *)source + 1;
//	}
//
//    return;
//}
//
//void APP_Memset(void *destination, BYTE_8 value, UINT_32 size)
//{
//	if(NULL == destination)
//	{
//		return;
//	}
//	
//	if((UINT_32)0U == size)
//	{
//		return;
//	}
//	
//	while(size--)
//	{
//		*(unsigned char *)destination = (unsigned char)value;
//		destination = (unsigned char *)destination + 1U;
//	}
//	
//    return;
//}
//
//CVC_T_Status SRV_Initialize(INT8U* const ipCfgBytes, const INT32U icfgBytes, INT32U* opAppVer)
//{
//	APP_T_Status ret = APP_C_ERROR;
//	if ((NULL == ipCfgBytes) || (NULL == opAppVer))
//	{
//		return APP_C_ERROR;
//	}
//
//	*opAppVer = 0x123456;
//
//	return APP_C_NO_ERROR;
//}
//
//static void newMsgSendTest(INT32U PeripheralNumber,INT8U type,INT8U id)
//{
//	PFMSG_t PFMsg = { 0U };
//	PFMsg.PeripheralNumber = PeripheralNumber;
//	PFMsg.MsgSize = 20;
//	PFMsg.AppType = 0u;
//	PFMsg.MsgID = id;
//	PFMsg.ITFVer = 0u;
//	PFMsg.GroupNum = 0U;
//	APP_Memset(PFMsg.Message, 0XAA, 20u);
//	CVC_BSW_ITF_Write(CVC_BSW_APP_TX_TYPE, (INT8U*)&PFMsg, PFMsg.MsgSize + (INT16U)PFMSG_HEAD_SIZE);
//}
//
//static void PROTO_F_ReadNewMsg(void)
//{
//	APP_APPMSG_t appRCVMSG = { 0U };
//	UINT_16 msgNum = 0U;
//	UINT_16 i = 0U;
//	APP_T_Status retStatus = APP_C_ERROR;
//	UINT_16 msgPINum = 0U;
//	UINT_16 msg037Num = 0U;
//	UINT_16 msgOTHNum = 0U;
//
//	msgNum = API_ReadNewMsgNum();
//	if (msgNum > 0U)
//	{
//		printf("proto num :%u\n", msgNum);
//	}
//	for (i = 0U; i < msgNum; ++i)
//	{
//		retStatus = API_ReadNewMsg(&appRCVMSG);
//		if (APP_C_NO_ERROR != retStatus)
//		{
//			printf("App ReadNewMsg error\n");
//			return;
//		}
//		switch (appRCVMSG.Message[0])
//		{
//		case CVC_APP_TYPE_RSSPI:
//			++msgPINum;
//			break;
//		case CVC_APP_TYPE_DY037:
//			//newMsgSendTest(appRCVMSG.PeripheralNumber, APP_TYPE_C_DY037, 1u);
//			++msg037Num;
//			break;
//		case APP_TYPE_C_RAW:
//			//newMsgSendTest(appRCVMSG.PeripheralNumber, APP_TYPE_C_RAW, 0u);
//			break;
//		default:
//			++msgOTHNum;
//			break;
//		}
//		printf("PROTO_F_ReadNewMsg, app:%d, PID:%x, msgID:%d, msgLen:%d\n",\
//			appRCVMSG.Message[0], appRCVMSG.PeripheralNumber, appRCVMSG.Message[1], appRCVMSG.MsgSize);
//		/*debug_printf("PROTO_F_ReadNewMsg, app:%d, PID:%x, msgID:%d, msgLen:%d\n", appRCVMSG.Message[0], appRCVMSG.PeripheralNumber, appRCVMSG.Message[1], appRCVMSG.MsgSize);*/
//	}
//	/*debug_printf("PROTO_F_ReadNewMsg, rssp1:%d, dy037:%d, other:%d\n", msgPINum, msg037Num, msgOTHNum);*/
//	return;
//}
//
//static void pda_test()
//{
//	static INT8U FLAG = 0U;
//	INT8U testData[10248] = { 0u };
//	INT8U testDataRead[1024] = { 0u };
//	INT32U actualSize = 0u;
//
//	if (0U == FLAG)
//	{
//		APP_Memset(testData, 0x2a, 1024u);
//		API_WritePDANVital(testData,128u);
//		API_WritePDAMass(0, testData, 128u, 0x80);
//
//		APP_Memset(testData, 0u, 10240u);
//		API_ReadPDAMass(0, testData,&actualSize);
//		printf("nv: SIZE %u,id %u, data %u\n", actualSize, *(INT32U*)testData, testData[actualSize - 1u]);
//		API_ReadPDANVital(testDataRead, 256u, &actualSize);
//		printf("nv: SIZE %u,data %u %u\n", actualSize, testDataRead[0], testDataRead[actualSize - 1u]);
//
//		FLAG = 1U;
//	}
//	if (1U == FLAG)
//	{
//		APP_Memset(testData, 0x11, 128);
//		API_WritePDAVital(testData, 128u);
//		APP_Memset(testData, 0x3a, 10240u);
//		API_WritePDAMass(2, testData, 10240u, 0x82);
//
//		APP_Memset(testData, 0u, 10240u);
//		API_ReadPDAMass(2, testData, &actualSize);
//		printf("nv: SIZE %u,id %u, data %u\n", actualSize, *(INT32U*)testData, testData[actualSize - 1u]);
//		API_ReadPDAVital(testDataRead, 256u, &actualSize);
//		printf("nv: SIZE %u,data %u %u\n", actualSize, testDataRead[0], testDataRead[actualSize - 1u]);
//
//		FLAG = 2U;
//	}
//}
//
//static void IO_TEST_F_RecvVIBData(void)
//{
//	APP_IOData_t VIBData = { 0 };
//	APP_T_Status retStatus = APP_C_ERROR;
//	BYTE_8 i = 0u;
//	UINT_32 VSN0 = 0U;
//	UINT_32 VSN1 = 0U;
//	UINT_32 VSN2 = 0U;
//	API_ReadSysVSN(&VSN0, &VSN1, &VSN2);
//	retStatus = API_ReadDIInfo(&VIBData);
//	if (APP_C_NO_ERROR != retStatus)
//	{
//		/*debug_printf("App ReadVIBData error\n");*/
//		return;
//	}
//	else
//	{
//		
//		for (i = 0; i < VIBData.Length; i++)
//		{
//			if (VIBData.IOPortData[i].PortValue == 1)
//			{
//				//printf("VIB DATA:%d\t%d\n",VIBData.IOPortData[i].PortIndex, VIBData.IOPortData[i].PortValue);
//			}
//		}
//		/*debug_printf("VIB Head:%d %d %d\n",VSN0,VIBData.InfoType,VIBData.Length);*/
//	}
//
//	return;
//}

//void SRV_ActiveCycle()
//{
////	UINT_32 timeSpend = 0U;
////	UINT_32 timeLimit = 0U;
////	UINT_32 timePerct = 0U;
////	UINT_32 curRunTime = 0U;
////	UINT_64 curTime = 0U;
////#ifdef CVC_CONF_CPU_A
////	UINT_32 dataLocal = 0x1234U;
////#else
////	UINT_32 dataLocal = 0x5678U;
////#endif
////	UINT_32 dataPeer = 0U;
////	APP_APPMSG_t appRCVMSG = {0U};
////	APP_IOData_t  VIBData = {0U};
//	VVBGRPData_t VVBGRPData = {0U};
//	APP_BTMData_t BTMData = {0U};
////	APP_APPMSG_t appSNDMSG = {0U};
////	APP_IOData_t  VOBData = {0U};
////	APP_VTSData_t VTSData = {0U};
////	SysStatus_Info_t STAData = {0U};
////	BYTE_8 MNTInfo[50] = {0x0U};
////	UINT_16 msgNum = 0U;
////	UINT_16 msgPINum = 0U;
////	UINT_16 msg037Num = 0U;
////	UINT_16 msgOTHNum = 0U;
//	UINT_16 msgVVBNum = 0U;
////	UINT_16 msgADDNum = 0U;
//	UINT_16 msgBTMNum = 0U;
////	UINT_16 msgSTANum = 0U;
//	UINT_32 i = 0U;
//	UINT_32 VSN0 = 0U;
//	UINT_32 VSN1 = 0U;
//	UINT_32 VSN2 = 0U;
////	UINT_16 msgSize = 0U;
////	BYTE_8 msgData = 0U;
////	BYTE_8 MsgID = 0U;
////	BYTE_8 ITFVer = 0U;
//	BYTE_8 logicID = 0U;
//	APP_T_Status retStatus = APP_C_ERROR;
//	
//	API_ReadSysVSN(&VSN0, &VSN1, &VSN2);
//	//printf("vsn: %d %u\n", VSN0, timer_bswCurrTickGet());
//	//printf("vsn: %d\n", VSN0);
////	/*read proto data*/
//	PROTO_F_ReadNewMsg();
//	//if(VSN0 == 10u)
//
//	//pda_test();
//
////#ifndef RSSP1_SEND_ENABLE
////	//PI MSG
////	for (i = 0U; i < 10U; i++)
////	{
////		RSSPI_F_SendData(i + 1U, i + 1U, 500U);		
////	}	
////#endif
////
////#ifndef DY037_SEND_ENABLE
////	/*037 clientMsg*/
////	for (i = 0U; i < 5U; i++)
////	{
////		DY037_F_SendData(0x1004001 + i * 2, i + 1U, 500U);		
////	}		
////	/*037 serverMsg*/
////	for (i = 0U; i < 5U; i++)
////	{
////		DY037_F_SendData(0x6004001 + i * 2, i + 1U, 500U);	
////	}	
////#endif
//
////
////#ifndef DY037_CONNECT_ENABLE
////	if (201U == VSN0)
////	{
////		for (i = 0U; i < 1U; i++)
////		{
////			DY037_F_Connect(0x6004001 + i, 0xC0A8046F, 20001 + i);	
////		}	
////	}
////#endif
////#ifndef DY037_SWITCH_HEADER
////	/*037 swich header first*/
////	if (501U == VSN0)
////	{
////		//192.168.4.111:20001
////		DY037_F_Connect(0x6004001, 0xC0A8046F, 20001);		
////	}	
////	/*037 swich header sencond*/
////	if (1001U == VSN0)
////	{
////		DY037_F_Disconnect(0x6004001);		
////	}
////#endif
////#ifdef DY037_SWITCH_NODE
////	UINT_32 switchTime = 0;
////	BYTE_8  switchFlag = 0;
////	switchTime = VSN0 / 200;
////	switchFlag = VSN0 % 200;
////	if (1U == switchFlag)
////	{
////		switchFlag = switchTime % 2;		
////		DY037_F_Disconnect(0x6004001 + switchFlag);	
////		switchFlag = (switchFlag + 1) % 2;		
////		//192.168.4.111:20001
////		DY037_F_Connect(0x6004001 + switchFlag, 0xC0A8046F, 20001 + switchFlag);		
////	}	
////#endif	
////
////	
////#ifdef IO_TEST
//	IO_TEST_F_RecvVIBData();
////#else
////	retStatus = API_ReadDIInfo(&VIBData);
////	if(APP_C_NO_ERROR != retStatus)
////	{
////		/*debug_printf("App ReadVIBData error\n");*/
////		/*return;*/
////	}
////#endif
//	/*read vvb data*/
//	msgVVBNum = API_ReadVVBInfoNum(logicID);
//	for(i = 0U;i<msgVVBNum;++i)
//	{
//		retStatus = API_ReadVVBInfo(&VVBGRPData.SPDGRPData,&VVBGRPData.ADDData,logicID);
//		if(APP_C_NO_ERROR != retStatus)
//		{
//			printf("App ReadVVBData error\n");
//			return;
//		}
//		else 
//		{
//			//printf("App ReadVVBData num:%d idx:%d,spdTime:%llu,dir0:%d \n", msgVVBNum,i, VVBGRPData.SPDGRPData.Timestamp, VVBGRPData.SPDGRPData.PULGRPData[0].SPDDirct);
//		}
//	}
////	
//	/*read btm data*/
//	msgBTMNum = API_ReadBTMInfoNum(logicID);
//	for(i = 0U;i<msgBTMNum;++i)
//	{
//		retStatus = API_ReadBTMInfo(&BTMData, logicID);
//		if(APP_C_NO_ERROR != retStatus)
//		{
//			printf("App ReadBTMData error\n");
//			return;
//		}
//		else
//		{
//			printf("App ReadBTMData time:%llu,tele:0x%x\n", BTMData.Timestamp1, BTMData.TelgData[0]);
//		}
//	}
////	
////	/*read state data*/
////	msgSTANum = API_ReadSysStatusInfoNum();
////	for(i = 0U;i<msgSTANum;++i)
////	{
////		retStatus = API_ReadSysStatusInfo(&STAData);
////		if(APP_C_NO_ERROR != retStatus)
////		{
////			debug_printf("APP ReadSysStatusInfo error\n");
////			return;
////		}
////
////	}
////	
////	API_ReadLastRunTime(&timeSpend, &timeLimit, &timePerct);
////	API_ReadCurrentRunTime(&curRunTime);
////	API_ExchangeCPUData(&dataPeer, &dataLocal, 4U, 20U);
////	curTime = API_ReadCurTime();
////	
////	
////	/*write VOB msg*/
////#ifdef IO_TEST
//	//IO_TEST_F_SendVOBdata();
////#else
/////*	VOBData.VSN = VSN0;*/
////	retStatus = API_WriteDOInfo(&VOBData);
////	if(APP_C_NO_ERROR != retStatus)
////	{
////		debug_printf("App WriteDOInfo error\n");
////		return;
////	}
////#endif	
////	/*write VTS msg*/
////	memset(&VTSData,0,sizeof(VTSData));
////	VTSData.TimeStamp = VSN0;
////	retStatus = API_WriteVTSInfo(&VTSData);
////	if(APP_C_NO_ERROR != retStatus)
////	{
////		debug_printf("App WriteVTSInfo error\n");
////		return;
////	}
////	
////	/*write maintenance msg*/
////#ifdef CVC_CONF_CPU_R50
////	*(INT32U*)MNTInfo = VSN0;
////	APP_Memset(MNTInfo+4,(BYTE_8)0x11U,46U);
////	
// (0x0030U, 50U, MNTInfo, 0x01U);
////#endif
////// #ifdef CVC_CONF_CPU_R51
////	*(INT32U*)MNTInfo = VSN0;
////	APP_Memset(MNTInfo+4,(BYTE_8)0x44U,46U);
////	API_WriteMaintInfo(0x0030U, 50U, MNTInfo, 0x01U);
////
////// #endif
////	
//
//}
//
//APP_T_Status SRV_Teach(void)
//{
//	APP_T_Status ret = APP_C_ERROR;
//	BYTE_8 synData[CVC_SYN_DATA_SIZE] = { 0 };
//
//	memset(&synData[0], 0xaau, sizeof(synData));
//	ret = API_WriteEducation((void*)synData, CVC_SYN_DATA_SIZE);
//
//	return ret;
//}
//
//APP_T_Status SRV_Learn(void)
//{
//	APP_T_Status ret = APP_C_ERROR;
//	UINT_16 synDataLen = 0U;
//	BYTE_8 synData[CVC_SYN_DATA_SIZE] = { 0 };
//
//	ret = API_ReadEducation((void*)synData, (UINT_16)CVC_SYN_DATA_SIZE, &synDataLen);
//
//	return ret;
//}
//
//void SRV_ShutdownActiveCycle(void)
//{
//	APP_T_Status ret = APP_C_ERROR;
//	static BYTE_8 shutDownMsg[CVC_SHUTDOWN_MSG_SIZE] = { 0 };
//
//	APP_Memset(shutDownMsg, 0x22, CVC_SHUTDOWN_MSG_SIZE - 300 * 0x400);
//	ret = API_WriteAppShutdownMsg((void*)shutDownMsg, (UINT_32)CVC_SHUTDOWN_MSG_SIZE - 300 * 0x400);
//	if (APP_C_ERROR == ret)
//	{
//		printf("App write shutdown msg error\n");
//	}
//	printf("App write shutdown msg\n");
//
//	return;
//}
//
//APP_T_Status SRV_ReadMaintMsg(INT8U* ipMantMsg, INT16U iMantMsgLen)
//{
//	if (CVC_MAINT_MSG_SIZE < iMantMsgLen)
//	{
//		printf("Read PF maintenance msg length error %d\n", iMantMsgLen);
//		return APP_C_ERROR;
//	}
//	APP_Memcpy((void*)mantMsg, (UINT_32)iMantMsgLen, (const void*)ipMantMsg, (UINT_32)iMantMsgLen);
//	/*debug_printf("111Read PF maintenance msg %x %x\n",ipMantMsg[0],ipMantMsg[1]);
//	API_Printf("222Read PF maintenance msg %x %x\n",ipMantMsg[0],ipMantMsg[1],0,0,0,0);*/
//	return APP_C_NO_ERROR;
//}