#include "sandbox_domain.h"

SandboxU8 SRV_Initialize(
	SandboxU8* const configuration,
	const SandboxU32 configuration_size,
	SandboxU32* app_version,
	SandboxU64 raw_address)
{
	(void)raw_address;
	if ((app_version == 0) || ((configuration == 0) && (configuration_size != 0U)))
	{
		return SANDBOX_STATUS_BAD_PARAMETER;
	}

	*app_version = SANDBOX_DOMAIN_VERSION;
	return SANDBOX_STATUS_OK;
}

void SRV_ActiveCycle(void)
{
	SandboxU32 current_runtime = 0U;
	API_ReadCurrentRunTime(&current_runtime);
	(void)current_runtime;
}

SandboxU8 SRV_Teach(void)
{
	return SANDBOX_STATUS_NOT_IMPLEMENTED;
}

SandboxU8 SRV_Learn(void)
{
	return SANDBOX_STATUS_NOT_IMPLEMENTED;
}

void SRV_ShutdownActiveCycle(void)
{
}

SandboxU8 SRV_ReadMaintMsg(SandboxU8* message, SandboxU16 message_size)
{
	(void)message;
	(void)message_size;
	return SANDBOX_STATUS_NOT_IMPLEMENTED;
}