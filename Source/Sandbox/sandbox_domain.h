#ifndef SANDBOX_DOMAIN_H
#define SANDBOX_DOMAIN_H

typedef unsigned char SandboxU8;
typedef unsigned short SandboxU16;
typedef unsigned int SandboxU32;
typedef unsigned long long SandboxU64;

enum
{
	SANDBOX_STATUS_OK = 0U,
	SANDBOX_STATUS_NOT_IMPLEMENTED = 205U,
	SANDBOX_STATUS_BAD_PARAMETER = 204U
};

#define SANDBOX_DOMAIN_VERSION 0x00010000U

void API_ReadCurrentRunTime(SandboxU32* const runtime);
SandboxU8 SRV_Initialize(
	SandboxU8* const configuration,
	const SandboxU32 configuration_size,
	SandboxU32* app_version,
	SandboxU64 raw_address);
void SRV_ActiveCycle(void);
SandboxU8 SRV_Teach(void);
SandboxU8 SRV_Learn(void);
void SRV_ShutdownActiveCycle(void);
SandboxU8 SRV_ReadMaintMsg(SandboxU8* message, SandboxU16 message_size);

#endif