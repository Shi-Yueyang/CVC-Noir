#include "sandbox_domain.h"

static unsigned int runtime_api_call_count = 0U;

void API_ReadCurrentRunTime(SandboxU32* const runtime)
{
	if (runtime != 0)
	{
		*runtime = 1234U;
		++runtime_api_call_count;
	}
}

int main(void)
{
	SandboxU8 configuration[] = { 0x12U, 0x34U };
	SandboxU8 message[] = { 0x01U };
	SandboxU32 app_version = 0U;

	if (SRV_Initialize(configuration, sizeof(configuration), &app_version, 0U) != SANDBOX_STATUS_OK)
	{
		return 1;
	}
	if (app_version != SANDBOX_DOMAIN_VERSION)
	{
		return 2;
	}
	if (SRV_Initialize(0, 0U, &app_version, 0U) != SANDBOX_STATUS_OK)
	{
		return 3;
	}
	if (SRV_Initialize(0, 1U, &app_version, 0U) != SANDBOX_STATUS_BAD_PARAMETER)
	{
		return 4;
	}
	if (SRV_Initialize(configuration, sizeof(configuration), 0, 0U) != SANDBOX_STATUS_BAD_PARAMETER)
	{
		return 5;
	}

	SRV_ActiveCycle();
	if (runtime_api_call_count != 1U)
	{
		return 6;
	}
	SRV_ShutdownActiveCycle();
	if (SRV_Teach() != SANDBOX_STATUS_NOT_IMPLEMENTED)
	{
		return 7;
	}
	if (SRV_Learn() != SANDBOX_STATUS_NOT_IMPLEMENTED)
	{
		return 8;
	}
	if (SRV_ReadMaintMsg(message, sizeof(message)) != SANDBOX_STATUS_NOT_IMPLEMENTED)
	{
		return 9;
	}

	return 0;
}