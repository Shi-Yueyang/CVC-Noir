#ifndef _CVC_INTERFACE_DATA_H_INCLUDED_
#define _CVC_INTERFACE_DATA_H_INCLUDED_
#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include "cvc_datatypes.h"
#include "board_status.h"
#include "simulator_api_types.h"
#define ASW_ATP
#define MAX_BSWMSG_SIZE				   2500U  
#define MAX_BSWBUF_SIZE				   2600U 
#define MAX_ADDMSG_SIZE                64U 
#define MAX_TLGMSG_SIZE                830U 
#define MAX_BSWMSG_NUM                 2000U 
#define MAX_BSWMSG_IDXMGR_NUM          50U 
#define MAX_IOPORT_NUM				   64U 
#define MAX_SPD_NUM                    3U    
#define MAX_NVRAM_DATA_LENGTH          1024U 
#define MAX_DPLAG_DATA_LENGTH          256U   
#define MAX_MANT_BUF_SIZE              7000U 
#define MAX_MANT_MSG_SIZE              1200U 
#define MAX_MANT_MSG_NUM               5U  
#define MAX_SYN_DATA_SIZE              10240U 
#define MAX_MASS_FLASH_NUM             7U  
#define MAX_MASS_MEMORY_NUM            7U 
#define MAX_BTM_NUM                    2U  
#define CVC_RX_APP_NUM_MAX		       150U    
#define CVC_TX_APP_NUM_MAX		       150U
#define CVC_RX_ADD_NUM_MAX		       36U   
#define CVC_TX_ADD_NUM_MAX		       32U  
#define CVC_CH_SERIAL_NUM		       3U 
#define PFMSG_HEAD_SIZE                sizeof(PFMSG_t) - MAX_BSWMSG_SIZE
#define MantMSG_HEAD_SIZE              sizeof(MANTMSG_t) - MAX_BSWMSG_SIZE
#define CVC_SHUTDOWN_MSG_SIZE            0x100000U
#define VERSION_C_API_SWVER_LEN        25U   
#define VERSION_C_API_HDVER_LEN        10U  
#define SPDPULES_GROUP_NUM             50U 
#define CVC_SYN_DATA_SIZE                10240U
#define IPC_COM_C_ALL_DATA_SIZE_MAX 20490u
#define SYN_DATA_C_ASWSYNDATA_SIZE_MAX 10240u

#define INTF_DATA_C_EORROR0                  0xA000
#define INTF_DATA_C_EORROR1                  0xA001
#define INTF_DATA_C_EORROR2                  0xA002
#define INTF_DATA_C_EORROR3                  0xA003
#define INTF_DATA_C_EORROR4                  0xA004
#define INTF_DATA_C_EORROR5                  0xA005
#define INTF_DATA_C_EORROR6                  0xA006
#define INTF_DATA_C_EORROR7                  0xA007
#define INTF_DATA_C_EORROR8                  0xA008
#define INTF_DATA_C_EORROR9                  0xA009
#define INTF_DATA_C_EORRORA                  0xA00A
#define INTF_DATA_C_EORRORB                  0xA00B
#define INTF_DATA_C_EORRORC                  0xA00C
#define INTF_DATA_C_EORRORD                  0xA00D
#define INTF_DATA_C_EORRORE                  0xA00E
#define INTF_DATA_C_EORRORF                  0xA00F
#define INTF_DATA_C_EORROR10                 0xA010
#define INTF_DATA_C_EORROR11                 0xA011
#define INTF_DATA_C_EORROR12                 0xA012
#define INTF_DATA_C_EORROR13                 0xA013
#define INTF_DATA_C_EORROR14                 0xA014
#define INTF_DATA_C_EORROR15                 0xA015
#define INTF_DATA_C_EORROR16                 0xA016
#define INTF_DATA_C_EORROR17                 0xA017
#define INTF_DATA_C_EORROR18                 0xA018
#define INTF_DATA_C_EORROR19                 0xA019
#define INTF_DATA_C_EORROR1A                 0xA01A
#define INTF_DATA_C_EORROR1B                 0xA01B
#define INTF_DATA_C_EORROR1C                 0xA01C
#define INTF_DATA_C_EORROR1D                 0xA01D
#define INTF_DATA_C_EORROR1E                 0xA01E
#define INTF_DATA_C_EORROR1F                 0xA01F
#define INTF_DATA_C_EORROR20                 0xA020
#define INTF_DATA_C_EORROR21                 0xA021
#define INTF_DATA_C_EORROR22                 0xA022
#define INTF_DATA_C_EORROR23                 0xA023
#pragma  pack(1)
	typedef enum _CVC_BUFFER_OPER_t
	{
		CVC_BUFFER_OPER_FAILED = -1,
		CVC_BUFFER_OPER_SUCCESS = 0,
		CVC_BUFFER_OPER_EMPTY = 1,
		CVC_BUFFER_OPER_FULL = 2
	} CVC_BUFFER_OPER_t;
									
								
	typedef enum
	{
		APP_TYPE_C_UNKNOWN = 0U,
		APP_TYPE_C_RSSP1 = 1U,
		APP_TYPE_C_DY037 = 2U,
		APP_TYPE_C_RAW = 3U,
		APP_TYPE_C_SNMP = 5U,
		APP_TYPE_C_EXT = 10U,
		APP_TYPE_C_MAX = 11U
	} PROTO_E_APP_TYPE;
	typedef enum
	{
		DY037_MSG_C_CONNECTION_SUCCESS = 0,
		DY037_MSG_C_CONNECTION_LOST = 1,
		DY037_MSG_C_CONNECTION_FAILURE = 2,
		DY037_MSG_C_TO_APP_DATA = 4
	} DY037_E_TO_APP_MSG;
	typedef enum
	{
		DY037_MSG_C_CONNECT = 0,
		DY037_MSG_C_FROM_APP_DATA = 1,
		DY037_MSG_C_DISCONNECT = 2,
	} DY037_E_FROM_APP_MSG;
	typedef enum
	{
		RSSPI_MSG_C_FROM_APP_DATA = 1,
		RSSPI_MSG_C_DISCONNECT = 2
	} RSSPI_E_FROM_APP_MSG;
	typedef enum
	{
		RSSPI_MSG_C_TO_APP_DATA = 1
	} RSSPI_E_TO_APP_MSG;
	typedef enum
	{
		RAW_MSG_C_FROM_APP_DATA = 0,
		RAW_MSG_C_DISCONNECT = 2
	} RAW_E_FROM_APP_MSG;
	typedef enum
	{
		RAW_MSG_C_TO_APP_DATA = 0
	} RAW_E_TO_APP_MSG;
	typedef struct _APPMSG_t
	{
		INT32U PeripheralNumber;
		INT16U MsgSize;
		INT8U  Message[MAX_BSWMSG_SIZE];
	}APPMSG_t;
	typedef struct _PFMSG_t
	{
		INT16U BoardID;
		INT8U  GroupNum;
		INT8U  CfmType;
		INT8U  ReservedType;
		INT16U MsgIndex;
		INT32U PeripheralNumber;
		INT16U MsgSize;
		INT16U LinkIndex;
		INT8U  AppType;
		INT8U  MsgID;
		INT8U  ITFVer;
		INT8U  MsgSN;
		INT8U  Message[MAX_BSWMSG_SIZE];
	}PFMSG_t;
	typedef struct _MANTMSG_t
	{
		INT32U PeripheralNumber;
		INT16U MsgSize;
		INT8U  AppType;
		INT8U  Message[MAX_BSWMSG_SIZE];
	}MANTMSG_t;
	typedef struct _MANTINFO_HEAD_t
	{
		INT32U PeripheralNumber;
		INT16U MsgSize;
		INT8U  type;
		INT8U  cpuID;
		INT8U  coreID;
	}MANTINFO_HEAD_t;
	typedef struct _CVC_MANTINFO_Index_Struct
	{
		INT16U readPos;
		INT16U writePos;
		INT16U msgNum;
	} CVC_MANTINFO_Index_Struct;
	typedef struct _IOPortData_t
	{
		INT16U PortIndex;                  
		INT8U PortValue;               
	}IOPortData_t;

	typedef enum
	{
		VibPort_EB = 30,
		VibPort_SB,
		VibPort_CutOffTraction,
		VibPort_Isolation,
		VibPort_DRIVER_DIRECTION_FORWARD,
		VibPort_DRIVER_DIRECTION_BACKWARD,
		VibPort_DESK_I,
		VibPort_DESK_II,
		VibPort_tractionInfo_traction,
		VibPort_tractionInfo_brake,
		VibPort_motionDirection_I,
		VibPort_motionDirection_II,
		VibPort_TheSessionNBR_I,
		VibPort_TheSessionNBR_II,
		VibPort_sleepSignal,
		VibPort_trainFront_high,
		VibPort_trainFront_low,
		VibPort_EbReCollect,
		VibPort_LkjAuth,
		VibPort_LkjBrake,
	}VibPort;
	typedef enum
	{
		VobPort_SB = 0,
		VobPort_EB,
		VobPort_CutOffTraction,
		VobPort_BrakesTests,
		VobPort_Sb4Command,
		VobPort_Sb1Command,
		VobPort_LkjAuth,
	}VobPort;
	typedef struct _IOData_t
	{
		INT16U Length;                 
		IOPortData_t  IOPortData[MAX_IOPORT_NUM];  
	}IOData_t;
						
		
	typedef struct _BTMData_t
	{
		INT64U Timestamp1;         
		INT64U Timestamp2;               
		INT32U Location; 
		INT32U Accuracy;                
		INT16U TelgDataLen;              
		INT8U TelgData[MAX_TLGMSG_SIZE];
	}BTMData_t;
	typedef struct _VTSData_t
	{
		INT32S CoordNomnl;               
		INT32S SpeedNomnl; 
		INT32S Accelerate; 
		INT8U MotionDirc;        
		INT32U TimeStamp;        
	}VTSData_t;
	typedef struct _CVC_APP_Store_Data_t
	{
		INT32U CRC;
		INT32U datasize;
		INT8U data[MAX_NVRAM_DATA_LENGTH];
	}CVC_APP_Store_Data_t;
	typedef struct _CVC_BSW_Msg_Struct
	{
		BOOLEAN bRecieved;
		INT8U gMsg[MAX_BSWBUF_SIZE];
	} CVC_BSW_Msg_Struct;
	typedef struct _CVC_BSW_Pool_Struct
	{
		INT16U curPos;
		CVC_BSW_Msg_Struct GMsg[MAX_BSWMSG_NUM];
	} CVC_BSW_Pool_Struct;
	/*define the type of  global buffer message */
													
	typedef struct _CVC_BSW_Msg_Index_Struct
	{
		CVC_BSW_MSG_TYPE_ENUM msgType;  
		INT16U startPos;				
		INT16U endPos;				 
		INT16U readPos;
		INT16U writePos;
		INT16U msgNum;
	} CVC_BSW_Msg_Index_Struct;

	extern INT16U INTERFACE_DATA_V_AppRunTime;
	extern SysStatus_Info_t statusData;
	typedef struct _CVC_BSW_Index_Manager_Struct
	{
		INT16U curPos;
		CVC_BSW_Msg_Index_Struct GMsgIndex[MAX_BSWMSG_IDXMGR_NUM];
	} CVC_BSW_Index_Manager_Struct;
						
	
	typedef struct
	{
		INT32U scrCoreId;
		INT32U dstCoreId;
		INT32U vsn;
		INT32U len;
		INT8U icmMsgType;
		INT32U  packNoAndEndFlga;
	}IPC_COM_T_HEAD;
	typedef struct
	{
		IPC_COM_T_HEAD head;
		INT8U synDatas[IPC_COM_C_ALL_DATA_SIZE_MAX];
	}IPC_COM_T_MSG;
						
								
#pragma  pack()
	BOOLEAN CVC_BSW_ITF_Init(void);
	BOOLEAN CVC_BSW_ITF_GetMsgNum(CVC_BSW_MSG_TYPE_ENUM msgType, INT16U* pNum);
	INT32S CVC_BSW_ITF_Read(CVC_BSW_MSG_TYPE_ENUM msgType, INT8U* pMsg);
	BOOLEAN CVC_BSW_ITF_Write(CVC_BSW_MSG_TYPE_ENUM msgType, INT8U* pData, INT16U dataSize);
	BOOLEAN CVC_BSW_ITF_Reset(CVC_BSW_MSG_TYPE_ENUM msgType);
	BOOLEAN CVC_BSW_ITF_MantInfo_Read(INT8U* pMantInfo);
	BOOLEAN CVC_BSW_ITF_MantInfo_Write(INT32U PeripheralNumber, INT16U mantInfoLen, INT8U* pMantInfo, INT8U type);
	BOOLEAN CVC_BSW_ITF_MantInfo_GetMsgNum(INT16U* pNum);
	void CVC_BSW_ITF_CalCurTime(INT32U iCurTime);
	void CVC_BSW_ITF_SetUsedAntennaID(INT8U iAntennaID);
	void CVC_BSW_ITF_writeSysCurrentTime(INT64U time);
	void CVC_BSW_ITF_getVSN(INT32U* V0,INT32U* V1,INT32U* V2);
	INT32U CVC_BSW_ITF_getVSN0();
	void CVC_BSW_ITF_writeVSN(INT32U V0,INT32U V1,INT32U V2);
	void CVC_BSW_ITF_updateVSN(void);
	BOOLEAN CVC_BSW_ITF_PutEducation(void* Source, INT32U Size);
	void CVC_BSW_ITF_F_RestASWSynMsgBuffer(void);
	BOOLEAN CVC_BSW_ITF_F_GetEducation(void* Dest, INT32U size, INT32U* pSize);
#ifdef __cplusplus
}
#endif
#endif