#ifndef _application_H_
#define _application_H_

#include "cvc_datatypes.h"
#include "Interface_Data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_MAX_BSWMSG_SIZE 2500U
#define APP_MAX_IOPORT_NUM 64U
#define APP_MAX_TLGMSG_SIZE 830U

#define APP_C_NO_ERROR               0u /**<Base not error return, eq. to OS_NO_ERR */
#define APP_C_TIMEOUT               10u /**<General timetout error, eq. OS_TIMEOUT  */
#define APP_C_ERROR                201u /**<Base error return*/

#define ASW_TASK_INFO_C_MAX_MAINTMSG_SIZE 2048u

//typedef struct _APP_APPMSG_t
//{
//	UINT_32 PeripheralNumber;
//	UINT_16 MsgSize;
//	BYTE_8  Message[APP_MAX_BSWMSG_SIZE];/*Message[0]:AppType Message[1]:MsgID Message[2]:ITFVer*/
//}APP_APPMSG_t;

//typedef struct _APP_IOPortData_t
//{
//	UINT_16 PortIndex;                  /*[1,32]*/
//	BYTE_8 PortValue;                   
//}APP_IOPortData_t;

//typedef struct _APP_IOData_t
//{
//	UINT_16 Length;                     /*size of IOPortdata*/
//	APP_IOPortData_t  IOPortData[APP_MAX_IOPORT_NUM];   /*num of ioPort*/
//}APP_IOData_t;
//
//typedef struct _APP_BTMData_t
//{
//	UINT_64 Timestamp1;                 /*bsw runTime(10ms）since entering main cycle*/
//	UINT_64 Timestamp2;                 /*0xFFFFFFFF*/
//	BYTE_8 UsedAntenna;                 /*ANTENNA_1:1，ANTENNA_2:2*/
//	UINT_32 Accuracy;                   /*1000 mm*/
//	UINT_16 TelgDataLen;                /*830 bit*/
//	BYTE_8 TelgData[APP_MAX_TLGMSG_SIZE];
//}APP_BTMData_t;

//typedef struct _APP_VTSData_t
//{
//	INT_32 CoordNomnl;                 /*cm*/
//	INT_32 SpeedNomnl;                 /*mm/s*/
//	INT_32 Accelerate;                 /*mm/s2*/
//	BYTE_8 MotionDirc;                 /*0=UNKNOWN, 1=CAB_A_FIRST,2=CAB_B_FIRST (back forward)*/
//	UINT_32 TimeStamp;                  /*runTime since electirc on (10ms)*/
//}APP_VTSData_t;

//void APP_Memcpy(void *destination, UINT_32 length, const void *source, UINT_32 size);
//void APP_Memset(void *destination, BYTE_8 value, UINT_32 size);
//
//extern CVC_T_Status SRV_Initialize(INT8U *const ipCfgBytes,const INT32U icfgBytes,INT32U *opAppVer);
//extern void SRV_ActiveCycle(void);
//extern APP_T_Status SRV_Teach(void);
//extern APP_T_Status SRV_Learn(void);
//extern void SRV_ShutdownActiveCycle(void);
//extern CVC_T_Status SRV_ReadMaintMsg(INT8U *ipMantMsg, INT16U iMantMsgLen);
//
////extern CVC_T_Status API_ReadNewMsg(APPMSG_t* opAppMsg);
//extern CVC_T_Status API_WriteNewMsg(APPMSG_t* ipAppMsg);
//extern INT32U API_ReadNewMsgNum(void);
////extern CVC_T_Status API_WriteMaintMsg(MANTMSG_t* ipMantMsg);
//extern CVC_T_Status API_WriteMaintInfo(INT32U iPeripheralNumber, INT16U iMantInfoLen, INT8U* ipMantInfo, INT8U iType);
//extern CVC_T_Status API_ReadDIInfo(IOData_t* opVIBData);
//extern CVC_T_Status API_WriteDOInfo(IOData_t* ipVOBData);
//extern CVC_T_Status API_ReadVVBInfo(SPDGRPData_t* opSPDGRPData, ADDData_t* opADDData, INT8U iLogicID);
//extern INT32U API_ReadVVBInfoNum(INT8U iLogicID);
//extern CVC_T_Status API_ReadBTMInfo(BTMData_t* opBTMData, INT8U iLogicID);
//extern INT32U API_ReadBTMInfoNum(INT8U iLogicID);
//extern void API_SetUsedAntennaID(INT8U iAntennaID);
//extern CVC_T_Status API_WriteVTSInfo(VTSData_t* ipVTSData);
//extern CVC_T_Status API_ReadSysStatusInfo(SysStatus_Info_t* opSTAData);
//extern INT32U API_ReadSysStatusInfoNum(void);
//extern CVC_T_Status API_ReadLastRunTime(INT32U* opTimeSpend, INT32U* opTimeLimit, INT8U* opTimePercent);
//extern void API_ReadCurrentRunTime(INT32U* const opValue);
//extern INT64U API_ReadCurTime(void);
//extern CVC_APP_PDA_STATE_t API_ReadPDAVital(INT8U* opData, INT16U iSize, INT16U* opActualSize);
//extern CVC_APP_PDA_STATE_t API_WritePDAVital(void* ipSource, INT16U iSize);
//extern CVC_APP_PDA_STATE_t API_GetPDAVitalStatus(void);
//extern CVC_APP_PDA_STATE_t API_ReadPDANVital(INT8U* opData, INT16U iSize, INT16U* opActualSize);
//extern CVC_APP_PDA_STATE_t API_WritePDANVital(void* ipSource, INT16U iSize);
//extern CVC_APP_PDA_STATE_t API_GetPDANVitalStatus(void);
//extern CVC_APP_PDA_STATE_t API_ReadPDAMass(INT8U iFileIndex, INT8U* opBufferAddress, INT32U* opLength);
//extern CVC_APP_PDA_STATE_t API_WritePDAMass(INT8U iFileIndex, INT8U* ipBufferAddress, INT32U iLength, INT32U iLineID);
//extern CVC_APP_PDA_STATE_t API_GetPDAMassStatus(INT8U iFileIndex);
//extern void API_ReadPDAMassLineID(INT32U* opLineID);
//extern CVC_T_Status API_WriteAppShutdownMsg(void* ipData, INT32U iDataSize);
//extern CVC_T_Status API_SetShutdown(INT16U iCode);
//extern CVC_T_Status API_ReadPFVersion(SysVersion_Info_t* opVersion);
//extern CVC_T_Status API_ExchangeCPUData(INT8U* opBufPeer, INT8U* ipBufLocal, INT16U iSize, INT16U iTimeout);
//extern CVC_T_Status API_WriteEducation(void* ipSource, INT16U iSize);
//extern CVC_T_Status API_ReadEducation(void* opDest, INT16U iSize, INT16U* opSize);
//extern void API_ReadSysVSN(INT32U* opVSN0, INT32U* opVSN1, INT32U* opVSN2);
//extern INT32S API_Printf(const CHAR* context, INT32S arg1, INT32S arg2, INT32S arg3, INT32S arg4, INT32S arg5, INT32S arg6);

#ifdef __cplusplus
}
#endif

#endif