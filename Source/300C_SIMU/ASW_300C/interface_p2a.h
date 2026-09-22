#ifndef _INTERFACE_P2A_H_
#define _INTERFACE_P2A_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cvc_datatypes.h"
#include "Interface_Data.h"

#define CVC_EXCHANGE_CPUData_LEN             32U

#define INTF_BASW_C_EORROR0                  0xB000
#define INTF_BASW_C_EORROR1                  0xB001
#define INTF_BASW_C_EORROR2                  0xB002
#define INTF_BASW_C_EORROR3                  0xB003
#define INTF_BASW_C_EORROR4                  0xB004
#define INTF_BASW_C_EORROR5                  0xB005
#define INTF_BASW_C_EORROR6                  0xB006
#define INTF_BASW_C_EORROR7                  0xB007
#define INTF_BASW_C_EORROR8                  0xB008
#define INTF_BASW_C_EORROR9                  0xB009
#define INTF_BASW_C_EORRORA                  0xB00A
#define INTF_BASW_C_EORRORB                  0xB00B
#define INTF_BASW_C_EORRORC                  0xB00C
#define INTF_BASW_C_EORRORD                  0xB00D
#define INTF_BASW_C_EORRORE                  0xB00E
#define INTF_BASW_C_EORRORF                  0xB00F
#define INTF_BASW_C_EORROR10                 0xB010
#define INTF_BASW_C_EORROR11                 0xB011
#define INTF_BASW_C_EORROR12                 0xB012
#define INTF_BASW_C_EORROR14                 0xB014
#define INTF_BASW_C_EORROR15                 0xB015
#define INTF_BASW_C_EORROR16                 0xB016
#define INTF_BASW_C_EORROR17                 0xB017
#define INTF_BASW_C_EORROR18                 0xB018
#define INTF_BASW_C_EORROR19                 0xB019
#define INTF_BASW_C_EORROR1A                 0xB01A
#define INTF_BASW_C_EORROR1B                 0xB01B
#define INTF_BASW_C_EORROR1C                 0xB01C
#define INTF_BASW_C_EORROR1D                 0xB01D
#define INTF_BASW_C_EORROR1E                 0xB01E
#define INTF_BASW_C_EORROR1F                 0xB01F
#define INTF_BASW_C_EORROR20                 0xB020
#define INTF_BASW_C_EORROR21                 0xB021
#define INTF_BASW_C_EORROR22                 0xB022
#define INTF_BASW_C_EORROR23                 0xB023
#define INTF_BASW_C_EORROR24                 0xB024
#define INTF_BASW_C_EORROR25                 0xB025
#define INTF_BASW_C_EORROR27                 0xB027
#define INTF_BASW_C_EORROR28                 0xB028
#define INTF_BASW_C_EORROR29                 0xB029
#define INTF_BASW_C_EORROR30                 0xB030
#define INTF_BASW_C_EORROR31                 0xB031
#define INTF_BASW_C_EORROR32                 0xB032
#define INTF_BASW_C_EORROR33                 0xB033
#define INTF_BASW_C_EORROR34                 0xB034
#define INTF_BASW_C_EORROR35                 0xB035
#define INTF_BASW_C_EORROR36                 0xB036

#define BOARD_STATUS_CONFIG_TYPE_MAX    7U
#define BOARD_STATUS_CONFIG_BOARD_MAX   8U

#define BOARD_STATUS_TYPE_MPB           0U
#define BOARD_STATUS_TYPE_GWB           1U
#define BOARD_STATUS_TYPE_VVB           2U
#define BOARD_STATUS_TYPE_VIB           3U
#define BOARD_STATUS_TYPE_VOB           4U
#define BOARD_STATUS_TYPE_BTM           5U
#define BOARD_STATUS_TYPE_AIOB          6U

/* Board status configuration structs for simu_config.json parsing */
typedef struct _BoardStatus_ConfigEntry_t
{
    BYTE_8 id;
    BYTE_8 friend_id;
    BYTE_8 is_good; /* 1 = good (0xFF), 0 = bad (0x00) */
} BoardStatus_ConfigEntry_t;

typedef struct _BoardStatus_ConfigType_t
{
    BYTE_8 type_id;
    BYTE_8 board_num;
    BoardStatus_ConfigEntry_t boards[BOARD_STATUS_CONFIG_BOARD_MAX];
} BoardStatus_ConfigType_t;

typedef struct _BoardStatus_Config_t
{
    BYTE_8 type_num;
    BoardStatus_ConfigType_t types[BOARD_STATUS_CONFIG_TYPE_MAX];
} BoardStatus_Config_t;

extern BoardStatus_Config_t g_board_status_config;

/* PDA storage configuration for simu_config.json parsing */
#define PDA_CONFIG_MAX_FLASH_FILES 7U
#define PDA_CONFIG_MAX_NVRAM_SIZE 1024U
#define PDA_CONFIG_MAX_DATAPLUG_SIZE 1024U
#define PDA_CONFIG_MAX_FLASH_SIZE (15 * 1024 * 1024)  // 15MB per flash area

typedef struct _PDA_SingleFileConfig_t
{
    char file[256];
    unsigned int read_ms;   /* simulated read delay, 0 = immediate */
    unsigned int write_ms;  /* simulated write delay, 0 = immediate */
} PDA_SingleFileConfig_t;

typedef struct _PDA_FlashFileConfig_t
{
    INT8U id;
    char file[256];
    unsigned int read_ms;   /* simulated read delay, 0 = immediate */
    unsigned int write_ms;  /* simulated write delay, 0 = immediate */
} PDA_FlashFileConfig_t;

typedef struct _PDA_StorageConfig_t
{
    PDA_SingleFileConfig_t nrnw;
    PDA_SingleFileConfig_t araw;
    PDA_SingleFileConfig_t arnw;
    INT8U flash_count;
    PDA_FlashFileConfig_t flash[PDA_CONFIG_MAX_FLASH_FILES];
} PDA_StorageConfig_t;

extern PDA_StorageConfig_t g_pda_storage_config;

BOOLEAN writePFMsg(INT8U msgID, INT8U appType, INT32U ctcsID, INT8U* buff, INT16U buffSize);
int GetShutDownState();

#ifdef __cplusplus
}
#endif

#endif