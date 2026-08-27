#include "conf.h"
#include "../ASW_300C/bswMath.h"
#include "errorProc.h"

void* CONF_V_ASW1ConfigPtr = CVC_NULL; /*addr of aswConfig*/
void* CONF_V_ASW2ConfigPtr = CVC_NULL; /*addr of aswConfig*/
static INT8U SharedMemForAsw1App[CONF_C_APP_CONFIG_BIN_MAX_NUM * CONF_C_APP_CONFIG_BIN_MAX_SIZE] = { 0u };/*store configData for app(ASW1)*/
static INT8U SharedMemForAsw2App[CONF_C_APP_CONFIG_BIN_MAX_NUM * CONF_C_APP_CONFIG_BIN_MAX_SIZE] = { 0u };/*store configData for app(ASW1)*/
SysVersion_Info_t VERSION_V_API_VersionInfo = { 0u }; /*version info for application*/
static char swVersion[25u] = "V1.0.1.0.20240307.R";
static char hwVersion[10u] = "V1.0.0";
static char fpgaVersion[10u] = "V1.0.0";
static VERSION_T_300C_Version VERSION_V_VersionConfig = { 0u };

static INT8U DP_V_ASW1_Config[DP_C_CONFIG_MAX_SIZE] = { 0u }; /*ASW1整体配置文件*/
static INT8U DP_V_ASW2_Config[DP_C_CONFIG_MAX_SIZE] = { 0u }; /*ASW2整体配置文件*/
static DP_T_HEAD DP_V_ASW1_HeadConfig = { 0u };
static DP_T_HEAD DP_V_ASW2_HeadConfig = { 0u };
static DP_T_Net DP_V_ASW1_NetConfig[DP_C_NET_LOCAL_NUM_MAX] = { 0u };
static DP_T_Net DP_V_ASW2_NetConfig[DP_C_NET_LOCAL_NUM_MAX] = { 0u };
static DP_T_RSSP1Head DP_V_ASW1_RSSP1Head = { 0u };
static DP_T_RSSP1Head DP_V_ASW2_RSSP1Head = { 0u };
static DP_T_RSSP1Channel DP_V_ASW1_RSSP1Config[DP_C_RSSP1_LOCAL_NUM_MAX] = { 0u };
static DP_T_RSSP1Channel DP_V_ASW2_RSSP1Config[DP_C_RSSP1_LOCAL_NUM_MAX] = { 0u };
static DP_T_DY037 DP_V_ASW1_DY037Config = { 0u };
static DP_T_DY037 DP_V_ASW2_DY037Config = { 0u };
static INT8U DP_V_ASW1_ASWConfig[CONF_C_APP_CONFIG_BIN_MAX_SIZE] = { 0u };
static INT8U DP_V_ASW2_ASWConfig[CONF_C_APP_CONFIG_BIN_MAX_SIZE] = { 0u };
static INT8U DP_V_ASW1_UserParamConfig[CONF_C_USERPARAM_SIZE_MAX] = { 0u };
static INT8U DP_V_ASW2_UserParamConfig[CONF_C_USERPARAM_SIZE_MAX] = { 0u };

INT8U DP_V_ASW1_TRDPNum = 0u;
INT8U DP_V_ASW2_TRDPNum = 0u;
DP_T_TRDP DP_V_ASW1_TRDPConfig[DP_C_TRDP_SIZE_MAX] = { 0u };
DP_T_TRDP DP_V_ASW2_TRDPConfig[DP_C_TRDP_SIZE_MAX] = { 0u };

static INT8U CONF_V_Config_ASW1_TRAIN[CONF_C_CONFIG_MAX_SIZE] = { 0u }; /*ASW1_TRAIN.bin*/
static BOOLEAN CONF_V_Config_ASW1_TRAIN_ExistFlag = CVC_FALSE; /*ASW1_TRAIN.bin文件是否存在的标识，初始默认不存在*/
static INT8U CONF_V_Config_ASW1_LINE[CONF_C_CONFIG_MAX_SIZE] = { 0u }; /*ASW1_LINE.bin*/
static BOOLEAN CONF_V_Config_ASW1_LINE_ExistFlag = CVC_FALSE;
static INT8U CONF_V_Config_ASW2_TRAIN[CONF_C_CONFIG_MAX_SIZE] = { 0u }; /*ASW2_TRAIN.bin*/
static BOOLEAN CONF_V_Config_ASW2_TRAIN_ExistFlag = CVC_FALSE;
static INT8U CONF_V_Config_ASW2_LINE[CONF_C_CONFIG_MAX_SIZE] = { 0u }; /*ASW2_LINE.bin*/
static BOOLEAN CONF_V_Config_ASW2_LINE_ExistFlag = CVC_FALSE;

static void CONF_F_App_ConfigParse(void);
static void VERSION_F_300C_VersionParse(void);

void CONF_F_ConfigInit(void)
{
	///*版本配置解析处理，CVC300C_VersionList.bin 和 ASW_MD5.bin*/
	VERSION_F_300C_VersionParse();
	//VERSION_F_VersionInfoAPIFormat();

	/*解析配置文件并将相应配置放在内存中*/
	//DP_F_readFromFlash();

	//otherAswConfig_ReadParse();

	/*应用配置处理*/
	CONF_F_App_ConfigParse();
	return;
}


//DP中的应用配置文件
static INT8U* DP_F_GetAsw1ConfigFile(INT8U aswIndex)
{
	if (aswIndex == CONF_C_ASW_INDEX_1)
	{
		return DP_V_ASW1_ASWConfig;
	}
	else if (aswIndex == CONF_C_ASW_INDEX_2)
	{
		return DP_V_ASW2_ASWConfig;
	}
	else
	{
		return CVC_NULL;
	}
}

/// Parse config for app
static void CONF_F_App_ConfigParse(void)
{
	BOOLEAN ret = CVC_FALSE;
	CONF_T_TRAIN_LINE_HEAD* pTrainHead1 = CVC_NULL;
	CONF_T_TRAIN_LINE_HEAD* pTrainHead2 = CVC_NULL;
	CONF_T_TRAIN_LINE_HEAD* pLineHead1 = CVC_NULL;
	CONF_T_TRAIN_LINE_HEAD* pLineHead2 = CVC_NULL;
	INT8U* pAppConfigHead1 = CVC_NULL;
	INT8U* pAppConfigHead2 = CVC_NULL;
	INT8U asw1Size = 0u;
	INT8U asw2Size = 0u;
	INT32U asw1CRC = 0u;
	INT32U asw2CRC = 0u;
	INT8U i = 0u;

	/*开辟内存，存放应用配置文件*/
	CONF_V_ASW1ConfigPtr = (void*)SharedMemForAsw1App;
	CONF_V_ASW2ConfigPtr = (void*)SharedMemForAsw2App;

	/*应用配置文件Asw.bin最多有6个*/
	/*对于DP/TRAIN/LINE而言，此时已经知道了所有的Asw.bin，所以可以直接获取*/
	/*ASW1_DP.bin 里面的Asw.bin*/
	if (0u != DP_V_ASW1_HeadConfig.aswSize)
	{
		STD_F_MemcpyEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW1ConfigPtr + 4u + asw1Size, DP_V_ASW1_HeadConfig.aswSize, DP_F_GetAsw1ConfigFile(CONF_C_ASW_INDEX_1), DP_V_ASW1_HeadConfig.aswSize);
		asw1Size += DP_V_ASW1_HeadConfig.aswSize;
	}

	/*ASW1_LINE.bin中的Asw.bin*/
	pLineHead1 = (CONF_T_TRAIN_LINE_HEAD*)CONF_V_Config_ASW1_LINE;
	if (0u != pLineHead1->aswSize)
	{
		pAppConfigHead1 = &CONF_V_Config_ASW1_LINE[pLineHead1->aswAddr];
		STD_F_MemcpyEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW1ConfigPtr + 4u + asw1Size, pLineHead1->aswSize, (void*)pAppConfigHead1, pLineHead1->aswSize);
		asw1Size += pLineHead1->aswSize;
	}

	/*ASW1_TRAIN.bin中的Asw.bin*/
	pTrainHead1 = (CONF_T_TRAIN_LINE_HEAD*)CONF_V_Config_ASW1_TRAIN;
	if (0u != pTrainHead1->aswSize)
	{
		pAppConfigHead1 = &CONF_V_Config_ASW1_TRAIN[pTrainHead1->aswAddr];
		STD_F_MemcpyEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW1ConfigPtr + 4u + asw1Size, pTrainHead1->aswSize, (void*)pAppConfigHead1, pTrainHead1->aswSize);
		asw1Size += pTrainHead1->aswSize;
	}

	switch (asw1Size % 4u)
	{
	case 1u:
		STD_F_MemsetEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW1ConfigPtr + 4u + asw1Size, 0u, 3u);
		asw1Size += 3u;
		break;
	case 2u:
		STD_F_MemsetEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW1ConfigPtr + 4u + asw1Size, 0u, 2u);
		asw1Size += 2u;
		break;
	case 3u:
		STD_F_MemsetEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW1ConfigPtr + 4u + asw1Size, 0u, 1u);
		asw1Size += 1u;
		break;
	}

	asw1CRC = CRC_F_InlineCalculateCRC32_0x04C11DB7((INT8U*)CONF_V_ASW1ConfigPtr + 4u, asw1Size); /*计算CRC*/
	STD_F_MemsetEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW1ConfigPtr + 4u + asw1Size, asw1CRC, 4u);
	asw1Size += 4u;
	STD_F_MemsetEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW1ConfigPtr + 4u + asw1Size, 0u, 64u);
	asw1Size += 64u;
	*(INT32U*)CONF_V_ASW1ConfigPtr = asw1Size;

	/*ASW2_DP.bin 里面的Asw.bin*/
	if (0u != DP_V_ASW2_HeadConfig.aswSize)
	{
		STD_F_MemcpyEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW2ConfigPtr + 4u + asw2Size, DP_V_ASW2_HeadConfig.aswSize, DP_F_GetAsw1ConfigFile(CONF_C_ASW_INDEX_2), DP_V_ASW2_HeadConfig.aswSize);
		asw2Size += DP_V_ASW2_HeadConfig.aswSize;
	}

	/*ASW2_LINE.bin中的Asw.bin*/
	pLineHead2 = (CONF_T_TRAIN_LINE_HEAD*)CONF_V_Config_ASW2_LINE;
	if (0u != pLineHead2->aswSize)
	{
		pAppConfigHead2 = &CONF_V_Config_ASW2_LINE[pLineHead2->aswAddr];
		STD_F_MemcpyEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW2ConfigPtr + 4u + asw2Size, pLineHead2->aswSize, (void*)pAppConfigHead2, pLineHead2->aswSize);
		asw2Size += pLineHead2->aswSize;
	}

	/*ASW2_TRAIN.bin中的Asw.bin*/
	pTrainHead2 = (CONF_T_TRAIN_LINE_HEAD*)CONF_V_Config_ASW2_TRAIN;
	if (0u != pTrainHead2->aswSize)
	{
		pAppConfigHead2 = &CONF_V_Config_ASW2_TRAIN[pTrainHead2->aswAddr];
		STD_F_MemcpyEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW2ConfigPtr + 4u + asw2Size, pTrainHead2->aswSize, (void*)pAppConfigHead2, pTrainHead2->aswSize);
		asw2Size += pTrainHead2->aswSize;
	}

	switch (asw2Size % 4u)
	{
	case 1u:
		STD_F_MemsetEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW2ConfigPtr + 4u + asw2Size, 0u, 3u);
		asw2Size += 3u;
		break;
	case 2u:
		STD_F_MemsetEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW2ConfigPtr + 4u + asw2Size, 0u, 2u);
		asw2Size += 2u;
		break;
	case 3u:
		STD_F_MemsetEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW2ConfigPtr + 4u + asw2Size, 0u, 1u);
		asw2Size += 1u;
		break;
	}

	asw2CRC = CRC_F_InlineCalculateCRC32_0x04C11DB7((INT8U*)CONF_V_ASW2ConfigPtr + 4u, asw2Size); /*计算CRC*/
	STD_F_MemsetEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW2ConfigPtr + 4u + asw1Size, asw1CRC, 4u);
	asw2Size += 4u;
	STD_F_MemsetEx(__FILE__, __LINE__, (INT8U*)CONF_V_ASW2ConfigPtr + 4u + asw2Size, 0u, 64u);
	asw2Size += 64u;
	*(INT32U*)CONF_V_ASW2ConfigPtr = asw2Size;
	return;
}

/// privide version info to application
SysVersion_Info_t* VERSION_F_ReadPFVersion(void)
{
	return &VERSION_V_API_VersionInfo;
}

#pragma region hello
static void mpuVersion_Init(void)
{

	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_ELF_A53_Version, swVersion, sizeof(swVersion));
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_BSW_A53_Version, swVersion, sizeof(swVersion));
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_CBIT_A53_Version, swVersion, sizeof(swVersion));

	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_BSW_R50_Version, swVersion, sizeof(swVersion));
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_CBIT_R50_Version, swVersion, sizeof(swVersion));
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_ELF_R50_Version, swVersion, sizeof(swVersion));

	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_BSW_R51_Version, swVersion, sizeof(swVersion));
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_CBIT_R51_Version, swVersion, sizeof(swVersion));
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_ELF_R51_Version, swVersion, sizeof(swVersion));

	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_BSW_R52_Version, swVersion, sizeof(swVersion));
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_CBIT_R52_Version, swVersion, sizeof(swVersion));
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_ELF_R52_Version, swVersion, sizeof(swVersion));

	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_Board_Version, hwVersion, sizeof(hwVersion));
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_U_Boot_Version, hwVersion, sizeof(hwVersion));
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_SPL_Boot_Version, hwVersion, sizeof(hwVersion));
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_Driver_Version, hwVersion, sizeof(hwVersion));
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_TSN_Boot_Version, "V1.0.1", 6U);
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_TSN_EP_Version, hwVersion, sizeof(hwVersion));

	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_FPGA1_Version, fpgaVersion, sizeof(fpgaVersion));
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_FPGA2_Version, "0", 1U);
	memcpy(VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_CFGData_Version, "V1.0.1.0", 8u);
	//printf("ver %s\n", VERSION_V_API_VersionInfo.MPUVersionInfo.MPU_ELF_A53_Version);
}

static void gwuVersion_Init(void)
{
	memcpy(VERSION_V_API_VersionInfo.GWUVersionInfo.GWU_ELF_Version, swVersion, sizeof(swVersion));
	memcpy(VERSION_V_API_VersionInfo.GWUVersionInfo.GWU_BSW_Version, swVersion, sizeof(swVersion));

	memcpy(VERSION_V_API_VersionInfo.GWUVersionInfo.GWU_Board_Version, hwVersion, sizeof(hwVersion));
	memcpy(VERSION_V_API_VersionInfo.GWUVersionInfo.GWU_U_Boot_Version, hwVersion, sizeof(hwVersion));
	memcpy(VERSION_V_API_VersionInfo.GWUVersionInfo.GWU_SPL_Boot_Version_R50, hwVersion, sizeof(hwVersion));
	memcpy(VERSION_V_API_VersionInfo.GWUVersionInfo.GWU_SPL_Boot_Version_A53, hwVersion, sizeof(hwVersion));
	memcpy(VERSION_V_API_VersionInfo.GWUVersionInfo.GWU_Driver_Version, hwVersion, sizeof(hwVersion));
	memcpy(VERSION_V_API_VersionInfo.GWUVersionInfo.GWU_TSN_Boot_Version, "V1.0.1", 6U);
	memcpy(VERSION_V_API_VersionInfo.GWUVersionInfo.GWU_TSN_EP_Version, hwVersion, sizeof(hwVersion));

	memcpy(VERSION_V_API_VersionInfo.GWUVersionInfo.GWU_FPGA1_Version, fpgaVersion, sizeof(fpgaVersion));
}

static void vvbVersion_Init(void)
{
	memcpy(VERSION_V_API_VersionInfo.VVBVersionInfo.VVB_Board_Version, hwVersion, sizeof(hwVersion));
	memcpy(VERSION_V_API_VersionInfo.VVBVersionInfo.VVB_TSN_Boot_Version, "V1.0.1", 6U);
	memcpy(VERSION_V_API_VersionInfo.VVBVersionInfo.VVB_TSN_EP_Version, hwVersion, sizeof(hwVersion));

	memcpy(VERSION_V_API_VersionInfo.VVBVersionInfo.VVB_FPGA1_Version, fpgaVersion, sizeof(fpgaVersion));
	memcpy(VERSION_V_API_VersionInfo.VVBVersionInfo.VVB_FPGA2_Version, fpgaVersion, sizeof(fpgaVersion));
}

static void vibVersion_Init(void)
{
	memcpy(VERSION_V_API_VersionInfo.VIBVersionInfo.VIB_Board_Version, hwVersion, sizeof(hwVersion));
	memcpy(VERSION_V_API_VersionInfo.VIBVersionInfo.VIB_TSN_Boot_Version, "V1.0.1", 6U);
	memcpy(VERSION_V_API_VersionInfo.VIBVersionInfo.VIB_TSN_EP_Version, hwVersion, sizeof(hwVersion));
	memcpy(VERSION_V_API_VersionInfo.VIBVersionInfo.VIB_FPGA1_Version, "V1.0.1", 6U);
	memcpy(VERSION_V_API_VersionInfo.VIBVersionInfo.VIB_FPGA2_Version, "V1.0.1", 6U);
	memcpy(VERSION_V_API_VersionInfo.VIBVersionInfo.VIB_FPGA3_Version, "V1.0.1", 6U);
}

static void vobVersion_Init(void)
{
	memcpy(VERSION_V_API_VersionInfo.VOBVersionInfo.VOB_Board_Version, hwVersion, sizeof(hwVersion));
	memcpy(VERSION_V_API_VersionInfo.VOBVersionInfo.VOB_TSN_Boot_Version, "V1.0.1", 6U);
	memcpy(VERSION_V_API_VersionInfo.VOBVersionInfo.VOB_TSN_EP_Version, hwVersion, sizeof(hwVersion));
	memcpy(VERSION_V_API_VersionInfo.VOBVersionInfo.VOB_FPGA1_Version, fpgaVersion, sizeof(fpgaVersion));
	memcpy(VERSION_V_API_VersionInfo.VOBVersionInfo.VOB_FPGA2_Version, fpgaVersion, sizeof(fpgaVersion));
	memcpy(VERSION_V_API_VersionInfo.VOBVersionInfo.VOB_FPGA3_Version, fpgaVersion, sizeof(fpgaVersion));
}

static void btmVersion_Init(void)
{
	memcpy(VERSION_V_API_VersionInfo.SBTMVersionInfo.BTM_Version, "V1.1.2.2", 8U);
	VERSION_V_API_VersionInfo.SBTMVersionInfo.head.TypeID = (INT8U)VERSION_C_TYPE_BTM;
	VERSION_V_API_VersionInfo.SBTMVersionInfo.head.SwVersionNum = 0u;
	VERSION_V_API_VersionInfo.SBTMVersionInfo.head.HdVersionNum = 1u;

	memcpy(VERSION_V_API_VersionInfo.CBTMVersionInfo.BTM_Version, "V1.1.2.2", 8U);
	VERSION_V_API_VersionInfo.CBTMVersionInfo.head.TypeID = (INT8U)VERSION_C_TYPE_BTM;
	VERSION_V_API_VersionInfo.CBTMVersionInfo.head.SwVersionNum = 0u;
	VERSION_V_API_VersionInfo.CBTMVersionInfo.head.HdVersionNum = 1u;
}
#pragma endregion

static void VERSION_F_300C_VersionParse(void)
{
	//Bsw_readConfigFromFlash(VERSION_C_300C_VERSION_BIN_ADDR,"\\plug\\CVC300C_VersionList.bin", &VERSION_V_VersionConfig, sizeof(VERSION_T_300C_Version));
	VERSION_V_API_VersionInfo.BoardTypeNum = 6U;
	mpuVersion_Init();
	gwuVersion_Init();
	vvbVersion_Init();
	vibVersion_Init();
	vobVersion_Init();
	btmVersion_Init();
}

