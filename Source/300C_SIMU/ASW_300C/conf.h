#ifndef _VERSION_H_
#define _VERSION_H_

#include "cvc_datatypes.h"
#include "Interface_Data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONF_C_CONFIG_MAX_SIZE 131072u /*���������ļ�����󳤶ȣ�һ��ȫ�������ڴ��ȫ�ֱ�����*/
#define CONF_C_ASW1_TRAIN_BIN_ADDR 0u /*ASW1_TRAIN.bin��FLASH�д�ŵ���ʼ��ַ*/
#define CONF_C_ASW1_LINE_BIN_ADDR 0u /*ASW1_LINE.bin��FLASH�д�ŵ���ʼ��ַ*/
#define CONF_C_ASW2_TRAIN_BIN_ADDR 0u /*ASW2_TRAIN.bin��FLASH�д�ŵ���ʼ��ַ*/
#define CONF_C_ASW2_LINE_BIN_ADDR 0u /*ASW2_LINE.bin��FLASH�д�ŵ���ʼ��ַ*/
#define VERSION_C_300C_VERSION_BIN_ADDR 0u
#define DP_C_ASW1_CONFIG_BIN_ADDR 0u /*ASW1��DP�����ļ���FLASH�д�ŵ���ʼ��ַ*/
#define DP_C_ASW2_CONFIG_BIN_ADDR 0u /*ASW2��DP�����ļ���FLASH�д�ŵ���ʼ��ַ*/

#define CONF_C_APP_CONFIG_BIN_MAX_NUM (6u) /*Ӧ�������ļ���������ASW1��ASW2����DP.bin��TRAIN.bin��LINE.bin����ASW.bin�ļ���User_Param�û���������6��*/
#define CONF_C_APP_CONFIG_BIN_MAX_SIZE (100u*1024u+4u) /*����Ӧ�������ļ���󳤶ȣ����֧��100k�ٴ�4�ֽڳ���*/
#define CONF_C_USERPARAM_SIZE_MAX 2000u /*һ��.bin�ļ����� USER_PARAM ����󳤶ȣ���2000�ֽ�*/

#define DP_C_APP_CONFIG_BIN_MAX_SIZE (100u*1024u+4u) /*Ӧ�������ļ���󳤶ȣ����֧��100k�ٴ�4�ֽڳ���*/
#define DP_C_NET_LOCAL_NUM_MAX 12u    /* 12u? */
#define DP_C_RSSP1_LOCAL_NUM_MAX 20u  /* 20u? */
#define DP_C_CONFIG_MAX_SIZE (sizeof(DP_T_HEAD)+sizeof(DP_T_Net)*DP_C_NET_LOCAL_NUM_MAX+sizeof(DP_T_RSSP1Global)+sizeof(DP_T_RSSP1Head)*DP_C_RSSP1_LOCAL_NUM_MAX+sizeof(DP_T_DY037)+DP_C_APP_CONFIG_BIN_MAX_SIZE+CONF_C_USERPARAM_SIZE_MAX+sizeof(DP_T_CRC)) /*����DP�ļ��ĳ��ȣ�һ��ȫ�������ڴ��ȫ�ֱ�����*/
#define DP_C_TRDP_SIZE_MAX 6u 

#define VERSION_C_CONFIG_HARDWARE_VERSION_SIZE 4u /*��ƽ̨�İ汾�����ļ��У�һ��Ӳ���汾��Ϣ��ռ�õĿռ�*/
#define VERSION_C_CONFIG_VERSION_SIZE 9u /*��ƽ̨�İ汾�����ļ��У�һ�������汾��Ϣ��ռ�õĿռ�*/
#define VERSION_C_MD5_LEN 16u /*MD5����*/

#define CONF_C_ERROR_A 0x040au
#define CONF_C_ERROR_10 0x0410u
#define CONF_C_ERROR_25 0x0425u
#define CONF_C_ERROR_40 0x0440u

#define VERSION_C_ERROR_1A 0x011au
#define VERSION_C_ERROR_1B 0x011bu
#define VERSION_C_ERROR_1C 0x011cu
#define VERSION_C_ERROR_1D 0x011du
#define VERSION_C_ERROR_1E 0x011eu
#define VERSION_C_ERROR_1F 0x011fu
#define VERSION_C_ERROR_20 0x0120u
#define VERSION_C_ERROR_21 0x0121u
#define VERSION_C_ERROR_22 0x0122u
#define VERSION_C_ERROR_41 0x0141u
#define VERSION_C_ERROR_48 0x0148u
#define VERSION_C_ERROR_49 0x0148u
#define VERSION_C_ERROR_4A 0x014au
#define VERSION_C_ERROR_4B 0x014bu

#define DP_C_ERROR_1 0x0501u
#define DP_C_ERROR_2 0x0502u

#pragma pack(1)

typedef struct
{
	INT32U crc1;
	INT32U crc2;
}DP_T_CRC; /*�����ļ�ĩβ��CRC*/

typedef struct
{
	INT8U netIndex;
	INT8U netUse;
	INT32U localIp;
	INT32U subnetMask;
	INT32U gtwAddress;
}DP_T_Net;

typedef struct
{
	INT16U sourceAddr;
	INT32U locSidA;
	INT32U locSinitA;
	INT32U locDataVerA;
	INT32U localSysChkA;
	INT32U locSidB;
	INT32U locSinitB;
	INT32U locDataVerB;
	INT32U localSysChkB;
}DP_T_RSSP1Global;

typedef struct
{
	INT8U RSSP1Num;
	DP_T_RSSP1Global RSSP1GlobalConfig;
}DP_T_RSSP1Head;

typedef struct
{
	INT32U dpSize;
	INT32U aswAddr;
	INT32U aswSize;
	INT32U userAddr;
	INT32U userSize;
	INT32U userNum;
	INT8U netNum;
}DP_T_HEAD;

typedef struct
{
	INT8U netIndex;
	INT8U netUse;
	INT32U localIp;
	INT32U subnetMask;
	INT32U gtwAddress;
}DP_T_TRDP;

typedef struct
{
	INT32U configAddr; /*.bin �ĵ�ַ*/
	INT32U configSize; /*.bin �Ĵ�С*/
	INT32U aswAddr; /*.bin �е�Ӧ�������ļ���ַ*/
	INT32U aswSize; /*.bin �е�Ӧ�������ļ���С*/
}CONF_T_TRAIN_LINE_HEAD; /*TRAIN/LINE.bin ��ͷ��*/

typedef struct
{
	INT16U sessionID;
	INT32U locSidA;
	INT32U locSinitA;
	INT32U locDataVerA;
	INT32U localSysChkA;
	INT32U locSidB;
	INT32U locSinitB;
	INT32U locDataVerB;
	INT32U localSysChkB;
}DP_T_RSSP1Channel;

typedef struct
{
	INT32U locCTCSid;
	INT8U locCTCSidType;
}DP_T_DY037;

typedef enum
{
	CONF_C_ASW_INDEX_1 = 1u,
	CONF_C_ASW_INDEX_2 = 2u,
}CONF_E_ASW_INDEX;

typedef struct
{
	INT8U Board_Version[VERSION_C_CONFIG_HARDWARE_VERSION_SIZE];
	INT8U U_Boot_Version[VERSION_C_CONFIG_HARDWARE_VERSION_SIZE];
	INT8U SPL_Boot_Version[VERSION_C_CONFIG_HARDWARE_VERSION_SIZE];
	INT8U Driver_Version[VERSION_C_CONFIG_HARDWARE_VERSION_SIZE];
	INT8U TSN_EP_Version[VERSION_C_CONFIG_HARDWARE_VERSION_SIZE];
	INT8U TSN_Boot_Version[VERSION_C_CONFIG_HARDWARE_VERSION_SIZE];
	INT8U FPGA1_Version[VERSION_C_CONFIG_HARDWARE_VERSION_SIZE];
	INT8U FPGA2_Version[VERSION_C_CONFIG_HARDWARE_VERSION_SIZE];
	INT8U FPGA3_Version[VERSION_C_CONFIG_HARDWARE_VERSION_SIZE];
}VERSION_T_HardWareVersion; /*Ӳ���汾��Ϣ����ʽ�� X.Y.Z��������Ϊ4�ֽڣ�ʹ�ø�3λ����λ��0*/

typedef struct
{
	INT8U ELF_A53_Version[VERSION_C_CONFIG_VERSION_SIZE];
	INT8U BSW_A53_Version[VERSION_C_CONFIG_VERSION_SIZE];
	INT8U CBIT_A53_Version[VERSION_C_CONFIG_VERSION_SIZE];
	INT8U ELF_R50_Version[VERSION_C_CONFIG_VERSION_SIZE];
	INT8U BSW_R50_Version[VERSION_C_CONFIG_VERSION_SIZE];
	INT8U CBIT_R50_Version[VERSION_C_CONFIG_VERSION_SIZE];
	INT8U ELF_R51_Version[VERSION_C_CONFIG_VERSION_SIZE];
	INT8U BSW_R51_Version[VERSION_C_CONFIG_VERSION_SIZE];
	INT8U CBIT_R51_Version[VERSION_C_CONFIG_VERSION_SIZE];
	INT8U ELF_R52_Version[VERSION_C_CONFIG_VERSION_SIZE];
	INT8U BSW_R52_Version[VERSION_C_CONFIG_VERSION_SIZE];
	INT8U CBIT_R52_Version[VERSION_C_CONFIG_VERSION_SIZE];
	INT8U ELF_A53_MD5[VERSION_C_MD5_LEN];
	INT8U BSW_A53_MD5[VERSION_C_MD5_LEN];
	INT8U CBIT_A53_MD5[VERSION_C_MD5_LEN];
	INT8U ELF_R50_MD5[VERSION_C_MD5_LEN];
	INT8U BSW_R50_MD5[VERSION_C_MD5_LEN];
	INT8U CBIT_R50_MD5[VERSION_C_MD5_LEN];
	INT8U ELF_R51_MD5[VERSION_C_MD5_LEN];
	INT8U BSW_R51_MD5[VERSION_C_MD5_LEN];
	INT8U CBIT_R51_MD5[VERSION_C_MD5_LEN];
	INT8U ELF_R52_MD5[VERSION_C_MD5_LEN];
	INT8U BSW_R52_MD5[VERSION_C_MD5_LEN];
	INT8U CBIT_R52_MD5[VERSION_C_MD5_LEN];
	VERSION_T_HardWareVersion hardWareVersion;
}VERSION_T_MPU_Version; /*MPU�汾��Ϣ*/

typedef struct
{
	INT8U ELF_Version[VERSION_C_CONFIG_VERSION_SIZE];
	INT8U OUT_Version[VERSION_C_CONFIG_VERSION_SIZE];
	INT8U ELF_GWU_MD5[VERSION_C_MD5_LEN];
	INT8U OUT_GWU_MD5[VERSION_C_MD5_LEN];
	VERSION_T_HardWareVersion hardWareVersion;
}VERSION_T_GWU_Version; /*GWU�汾��Ϣ*/

typedef struct
{
	INT8U Version[VERSION_C_CONFIG_HARDWARE_VERSION_SIZE];
}VERSION_T_BTM_Version; /*BTM�汾��Ϣ*/

typedef struct
{
	VERSION_T_MPU_Version MPU1_Version;
	VERSION_T_MPU_Version MPU2_Version;
	VERSION_T_GWU_Version GWU_Version;
	VERSION_T_HardWareVersion VVB_Version;
	VERSION_T_HardWareVersion VIB_Version;
	VERSION_T_HardWareVersion VOB_Version;
	VERSION_T_BTM_Version BTM_Version;
	INT32U A53_CH1_CRC;
	INT32U A53_CH2_CRC;
	INT32U R50_CH1_CRC;
	INT32U R50_CH2_CRC;
	INT32U R51_CH1_CRC;
	INT32U R51_CH2_CRC;
	INT32U R52_CH1_CRC;
	INT32U R52_CH2_CRC;
	INT32U CRC1; /*�������г�Ա��������õ���CRC*/
	INT32U CRC2;
}VERSION_T_300C_Version; /*ƽ̨�汾�ļ��Ľṹ*/

typedef enum
{
	VERSION_C_TYPE_MPU = 0u,
	VERSION_C_TYPE_GWU,
	VERSION_C_TYPE_VVB,
	VERSION_C_TYPE_VIB,
	VERSION_C_TYPE_VOB,
	VERSION_C_TYPE_BTM,
}VERSION_E_BoardType;

void CONF_F_ConfigInit(void);
SysVersion_Info_t* VERSION_F_ReadPFVersion(void);
extern void* CONF_V_ASW1ConfigPtr;

#ifdef __cplusplus
}
#endif

#pragma pack()
#endif /*_VERSION_H_*/
