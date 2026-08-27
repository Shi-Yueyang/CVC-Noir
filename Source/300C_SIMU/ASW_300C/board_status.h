#ifndef _BOARD_STATUS_H_
#define  _BOARD_STATUS_H_

#include "cvc_datatypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STATUS_C_BDST_LEN 6U
#define STATUS_C_BOARD_NUM 8U

#define CONF_C_MPU_GROUP_NUM_MAX 2u
#define CONF_C_MPU_NUM_MAX 2u

#pragma  pack(1)
typedef struct
{
	INT8U modeID;
	INT8U modeID_R;
}CONF_T_RedunModeInfo; 

typedef struct
{
	INT8U boardNum;
	CONF_T_RedunModeInfo modeInfo[CONF_C_MPU_GROUP_NUM_MAX * CONF_C_MPU_NUM_MAX];
}CONF_T_MinimumSetSingle; 

typedef struct
{
	CONF_T_MinimumSetSingle mpbSet;
	CONF_T_MinimumSetSingle gwbSet;
	CONF_T_MinimumSetSingle vvbSet;
	CONF_T_MinimumSetSingle vibSet;
	CONF_T_MinimumSetSingle vobSet;
	CONF_T_MinimumSetSingle btmSet;
}CONF_T_MinimumSet; 

typedef struct
{
	INT8U BTMStatus;
	INT8U SP1Status;
	INT8U SP2Status;
	INT8U ANTStatus;
}BTM_T_STATUS;

typedef enum
{
	STATUS_C_TYPE_MPB = 0u,
	STATUS_C_TYPE_GWB,
	STATUS_C_TYPE_VVB,
	STATUS_C_TYPE_VIB,
	STATUS_C_TYPE_VOB,
	STATUS_C_TYPE_BTM,
	STATUS_C_TYPE_MAX
}STATUS_E_BoardType;


typedef enum
{
	BTM_A = 0x01u,
	BTM_B = 0X02u,
	BTM_A_AND_B = 0x03u,
	NO_BTM = 0xFFu,
	DEFAULT = 0x00u,
}BTM_E_NUM;
/*BTM state*/
typedef enum
{
	BTM_C_TEST = 0x01u,
	BTM_C_NORMAL_MODE = 0x03u,
	BTM_C_Failure  = 0x06u,	
}BTM_E_STATE;
#pragma  pack()

void BS_Init(void);
void btm_status_proc(BTM_T_STATUS* BOARD_STATUS_V_LocalSysBTMStatus);
void vvb_SpeedSensorStatus_proc(INT8U* SpeedSensorStatus, INT8U statusSize, INT8U boradIdxInVVB);
void vvb_AccRaderStatus_proc(INT8U* AccRaderStatus, INT8U statusSize, INT8U boradIdxInVVB);

#ifdef __cplusplus
}
#endif

#endif