#ifndef SIMULATOR_API_TYPES_H
#define SIMULATOR_API_TYPES_H

#include "cvc_datatypes.h"
#include "board_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef char S_CHAR;
typedef unsigned char BYTE_8;
typedef short INT_16;
typedef unsigned short UINT_16;
typedef int INT_32;
typedef unsigned int UINT_32;
typedef long long INT_64;
typedef unsigned long long UINT_64;

#define FLASH_MAX_AREA_NUM 2U
#define APP_MAX_BSWMSG_SIZE 2500U
#define APP_MAX_IOPORT_NUM 96U
#define APP_MAX_TLGMSG_SIZE 128U
#define MAX_SPD_NUM 3U
#define SPD_CH_NUM 2U
#define SPDPULES_GROUP_NUM 50U
#define CVC_CH_SERIAL_NUM 3U
#define VERSION_C_API_SWVER_LEN 25U
#define VERSION_C_API_HDVER_LEN 10U
#define ASW_COM_ID_NUM_MAX 6U
#define ASW_COM_DATA_SIZE 1400U
#define ATP_SDLU_IN_ATP_SESSION_ID 0x0803U
#define CVC_BOARD_TYPE_MAX 7U

typedef enum _CVC_APP_PDA_STATE_t
{
    CVC_PDA_NO_OPERATION = 0U,
    CVC_PDA_OPERATION_SUCCEED = 1U,
    CVC_PDA_OPERATION_PENDING = 2U,
    CVC_PDA_OPERATION_FAILED = 3U,
    CVC_PDA_NO_AVAILABLE = 4U
} CVC_APP_PDA_STATE_t;

typedef enum _CVC_APP_BSW_MSG_TYPE_ENUM
{
    CVC_BSW_APP_RX_TYPE = 0x1U,
    CVC_BSW_APP_TX_TYPE = 0x2U,
    CVC_BSW_MANT_RX_TYPE = 0x3U,
    CVC_BSW_MANT_TX_TYPE = 0x4U,
    CVC_BSW_VIB_RX_TYPE = 0x5U,
    CVC_BSW_VOB_TX_TYPE = 0x6U,
    CVC_BSW_VVB_RX_TYPE = 0x7U,
    CVC_BSW_BTM_RX_TYPE = 0x8U,
    CVC_BSW_VTS_TX_TYPE = 0x9U,
    CVC_BSW_STA_RX_TYPE = 0xAU,
    CVC_BSW_ASW_COM_RX_TYPE = 0xBU,
    CVC_BSW_ASW_COM_TX_TYPE = 0xCU,
    CVC_BSW_VVB1_RX_TYPE = 0xDU,
    CVC_BSW_VVB2_RX_TYPE = 0xEU,
    CVC_BSW_BTM1_RX_TYPE = 0xFU,
    CVC_BSW_BTM2_RX_TYPE = 0x10U,
    CVC_BSW_ASW_COM_TYPE_COUNT = 0x11U
} CVC_BSW_MSG_TYPE_ENUM;

#pragma pack(push, 1)
typedef struct _PULData_t
{
    S_CHAR SPDDirct;
    INT_32 SPDPules;
} PULData_t;

typedef struct _SPDData_t
{
    BYTE_8 ID;
    INT_64 Timestamp;
    PULData_t SPDData[MAX_SPD_NUM];
} SPDData_t;

typedef struct _SerialData_t
{
    BYTE_8 size;
    BYTE_8 ADDData[64U];
} SerialData_t;

typedef struct _ADDData_t
{
    SerialData_t SerialData[CVC_CH_SERIAL_NUM];
} ADDData_t;

typedef struct _VVBData_t
{
    SPDData_t SPDData;
    ADDData_t ADDData;
} VVBData_t;

typedef struct _PULGRPData_t
{
    BYTE_8 SPDDirct[SPD_CH_NUM];
    BYTE_8 PulesGroupNum[SPD_CH_NUM];
    UINT_64 PulseTimestamp[SPD_CH_NUM];
    INT_32 SPDGRPPules[SPD_CH_NUM][SPDPULES_GROUP_NUM];
} PULGRPData_t;

typedef struct _SPDGRPData_t
{
    UINT_64 Timestamp;
    PULGRPData_t PULGRPData[MAX_SPD_NUM];
} SPDGRPData_t;

typedef struct _VVBGRPData_t
{
    SPDGRPData_t SPDGRPData;
    ADDData_t ADDData;
} VVBGRPData_t;

typedef struct _APP_BTMData_t
{
    UINT_64 Timestamp1;
    UINT_64 Timestamp2;
    INT_32 Location;
    UINT_32 Accuracy;
    UINT_16 TelgDataLen;
    BYTE_8 TelgData[APP_MAX_TLGMSG_SIZE];
} APP_BTMData_t;

typedef struct _ASWRxData_t
{
    UINT_16 SrcID;
    UINT_16 Size;
    BYTE_8 Data[ASW_COM_DATA_SIZE];
} ASWRxData_t;

typedef struct _ASWTxData_t
{
    UINT_16 DistIDS[ASW_COM_ID_NUM_MAX];
    BYTE_8 IDNum;
    UINT_16 Size;
    BYTE_8 Data[ASW_COM_DATA_SIZE];
} ASWTxData_t;

typedef struct _SysVersion_Info_Head_t
{
    BYTE_8 TypeID;
    BYTE_8 SwVersionNum;
    BYTE_8 HdVersionNum;
} SysVersion_Info_Head_t;

typedef struct _SysVersion_Info_MPU_t
{
    SysVersion_Info_Head_t head;
    BYTE_8 MPU_ELF_A53_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 MPU_BSW_A53_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 MPU_CBIT_A53_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 MPU_ELF_R50_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 MPU_BSW_R50_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 MPU_CBIT_R50_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 MPU_ELF_R51_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 MPU_BSW_R51_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 MPU_CBIT_R51_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 MPU_ELF_R52_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 MPU_BSW_R52_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 MPU_CBIT_R52_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 MPU_CFGData_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 MPU_Board_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 MPU_U_Boot_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 MPU_SPL_Boot_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 MPU_Driver_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 MPU_TSN_EP_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 MPU_TSN_Boot_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 MPB_BMC_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 MPU_FPGA1_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 MPU_FPGA2_Version[VERSION_C_API_HDVER_LEN];
} SysVersion_Info_MPU_t;

typedef struct _SysVersion_Info_GWU_t
{
    SysVersion_Info_Head_t head;
    BYTE_8 GWU_ELF_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 GWU_BSW_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 GWU_Board_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 GWU_U_Boot_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 GWU_SPL_Boot_Version_R50[VERSION_C_API_HDVER_LEN];
    BYTE_8 GWU_SPL_Boot_Version_A53[VERSION_C_API_HDVER_LEN];
    BYTE_8 GWU_Driver_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 GWU_TSN_EP_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 GWU_TSN_Boot_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 GWU_FPGA1_Version[VERSION_C_API_HDVER_LEN];
} SysVersion_Info_GWU_t;

typedef struct _SysVersion_Info_VVB_t
{
    SysVersion_Info_Head_t head;
    BYTE_8 VVB_Board_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 VVB_TSN_EP_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 VVB_TSN_Boot_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 VVB_FPGA1_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 VVB_FPGA2_Version[VERSION_C_API_HDVER_LEN];
} SysVersion_Info_VVB_t;

typedef struct _SysVersion_Info_VIB_t
{
    SysVersion_Info_Head_t head;
    BYTE_8 VIB_Board_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 VIB_TSN_EP_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 VIB_TSN_Boot_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 VIB_FPGA1_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 VIB_FPGA2_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 VIB_FPGA3_Version[VERSION_C_API_HDVER_LEN];
} SysVersion_Info_VIB_t;

typedef struct _SysVersion_Info_VOB_t
{
    SysVersion_Info_Head_t head;
    BYTE_8 VOB_Board_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 VOB_TSN_EP_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 VOB_TSN_Boot_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 VOB_FPGA1_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 VOB_FPGA2_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 VOB_FPGA3_Version[VERSION_C_API_HDVER_LEN];
} SysVersion_Info_VOB_t;

typedef struct _SysVersion_Info_SBTM_t
{
    SysVersion_Info_Head_t head;
    BYTE_8 BTM_Version[VERSION_C_API_HDVER_LEN];
} SysVersion_Info_SBTM_t;

typedef struct _SysVersion_Info_CBTM_t
{
    SysVersion_Info_Head_t head;
    BYTE_8 BTM_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 BTM_TSN_EP_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 BTM_TSN_BOOT_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 BTM_FPGA1_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 BTM_FPGA2_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 BTM_FPGA3_Version[VERSION_C_API_HDVER_LEN];
} SysVersion_Info_CBTM_t;

typedef struct _SysVersion_Info_AIOB_t
{
    SysVersion_Info_Head_t head;
    BYTE_8 AIOB_Board_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 AIOB_TSN_EP_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 AIOB_TSN_Boot_Version[VERSION_C_API_HDVER_LEN];
    BYTE_8 AIOB_FPGA1_Version[VERSION_C_API_HDVER_LEN];
} SysVersion_Info_AIOB_t;

typedef struct _SysVersion_Info_t
{
    BYTE_8 SYS_Version[VERSION_C_API_SWVER_LEN];
    BYTE_8 BoardTypeNum;
    SysVersion_Info_MPU_t MPUVersionInfo;
    SysVersion_Info_GWU_t GWUVersionInfo;
    SysVersion_Info_VVB_t VVBVersionInfo;
    SysVersion_Info_VIB_t VIBVersionInfo;
    SysVersion_Info_VOB_t VOBVersionInfo;
    SysVersion_Info_SBTM_t SBTMVersionInfo;
    SysVersion_Info_CBTM_t CBTMVersionInfo;
    SysVersion_Info_AIOB_t AIOBVersionInfo;
} SysVersion_Info_t;

typedef struct _SysStatus_Info_Board_t
{
    INT8U ModuleID;
    INT8U ModuleID_R;
    INT8U Status[STATUS_C_BDST_LEN];
} SysStatus_Info_Board_t;

typedef struct _SysStatus_Info_Component_t
{
    INT8U TypeID;
    INT8U BoardNum;
    SysStatus_Info_Board_t BoardStatus[STATUS_C_BOARD_NUM];
} SysStatus_Info_Component_t;

typedef struct _SysStatus_Info_t
{
    INT8U BoardTypeNum;
    SysStatus_Info_Component_t Component[CVC_BOARD_TYPE_MAX];
} SysStatus_Info_t;
#pragma pack(pop)

#if defined(__cplusplus)
static_assert(sizeof(SysStatus_Info_t) == 463U, "System status ABI size changed");
#else
_Static_assert(sizeof(SysStatus_Info_t) == 463U, "System status ABI size changed");
#endif

#ifdef __cplusplus
}
#endif

#endif
