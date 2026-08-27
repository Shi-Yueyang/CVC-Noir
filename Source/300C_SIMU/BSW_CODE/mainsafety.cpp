#include "mainsafety.h"

#include <cstddef>
#include <cstdint>

#include "AppContext.h"
#include "logging/logger.h"
#include "session/other_asw_session.h"
#include "session/raw_session.h"
#include "bswTime.h"
#include "ASW_300C/Interface_Data.h"
#include "ASW_300C/asw_runtime_api.h"

/*-------------disable some warning begin--------------*/
#ifdef _MSC_VER
#pragma warning(disable: 4057 4100 4127 4189 4244 4293 4389)
#endif
/*-------------disable some warning end----------------*/

static void ExternalDeviceDataOutput(AppContext* ctx);
static void MaintenanceDataOutput(AppContext* ctx);
static void OtherAswDataOutput(AppContext* ctx);
static void SyncTrainPosition(AppContext* ctx) noexcept;
static void SyncPxiSessionTrainPosition(AppContext* ctx) noexcept;
static bool has_ext_session(const AppContext* ctx, uint32_t peripheral_number);
static bool has_maint_session(const AppContext* ctx, uint32_t peripheral_number);
static bool has_other_asw_session(const AppContext* ctx, uint32_t peripheral_number);
static bool has_raw_session(const AppContext* ctx, uint32_t peripheral_number);
static bool has_rsspi_session(const AppContext* ctx, uint32_t peripheral_number);
static bool has_safety037_session(const AppContext* ctx, uint32_t matching_number, bool match_by_id_037);
static session_result output_to_ext_session(AppContext* ctx, uint32_t peripheral_number, const unsigned char* buffer, std::size_t len);
static session_result output_to_maint_session(AppContext* ctx, uint32_t peripheral_number, const unsigned char* buffer, std::size_t len);
static session_result output_to_other_asw_session(AppContext* ctx, uint32_t peripheral_number, const unsigned char* buffer, std::size_t len);
static session_result output_to_raw_session(AppContext* ctx, uint32_t peripheral_number, const unsigned char* buffer, std::size_t len);
static session_result output_to_rsspi_session(AppContext* ctx, uint32_t peripheral_number, const unsigned char* buffer, std::size_t len);
static session_result output_to_safety037_session(AppContext* ctx, uint32_t matching_number, bool match_by_id_037, const unsigned char* buffer, std::size_t len, unsigned char msg_id, uint32_t app_peripheral_number);

void MAIN_SAFETY_F_ProcessInData(void)
{
	std::uint32_t startTime = 0u;
	std::uint32_t endTime = 0u;
	std::uint32_t runTime = 0u;
	std::uint32_t endTime2 = 0u;

	static std::uint32_t loop_count = 0U;
	loop_count++;

	(void)CVC_BSW_ITF_getVSN0();
	startTime = timer_bswCurrTickGet();
	setcurrRunTime(startTime / 10u);

	CVC_BSW_ITF_Write(CVC_BSW_STA_RX_TYPE, reinterpret_cast<unsigned char*>(&statusData), sizeof(statusData));

	SRV_ActiveCycle();

	endTime = timer_bswCurrTickGet();
	runTime = timer_GetCostTime(startTime, endTime);
	INTERFACE_DATA_V_AppRunTime = runTime;
	endTime2 = timer_bswCurrTickGet();
	CVC_BSW_ITF_CalCurTime(endTime2);

	setlastRunTime(startTime / 10u);
}

void SyncInput(AppContext* ctx)
{
	if (ctx == NULL)
	{
		return;
	}

	for (const std::unique_ptr<session>& session_ptr : ctx->maint_sessions)
	{
		if (session_ptr)
		{
			(void)session_ptr->input();
		}
	}

	for (const std::unique_ptr<session>& session_ptr : ctx->other_asw_sessions)
	{
		if (session_ptr)
		{
			(void)session_ptr->input();
		}
	}

	for (const std::unique_ptr<session>& session_ptr : ctx->raw_sessions)
	{
		if (session_ptr)
		{
			(void)session_ptr->input();
		}
	}

	for (const std::unique_ptr<session>& session_ptr : ctx->ext_sessions)
	{
		if (session_ptr)
		{
			(void)session_ptr->input();
		}
	}

	for (const std::unique_ptr<session>& session_ptr : ctx->rsspi_sessions)
	{
		if (session_ptr)
		{
			(void)session_ptr->input();
		}
	}

	for (const std::unique_ptr<session>& session_ptr : ctx->safety037_sessions)
	{
		if (session_ptr)
		{
			(void)session_ptr->input();
		}
	}

	for (const std::unique_ptr<session>& session_ptr : ctx->snmp_sessions)
	{
		if (session_ptr)
		{
			(void)session_ptr->input();
		}
	}



	if (ctx->pxi_session_instance)
	{
		SyncPxiSessionTrainPosition(ctx);
		(void)ctx->pxi_session_instance->input();
	}

	if (ctx->pxi_motion_session_instance)
	{
		(void)ctx->pxi_motion_session_instance->input();
		SyncTrainPosition(ctx);
	}
}

static void SyncTrainPosition(AppContext* ctx) noexcept
{
	if (ctx == nullptr || !ctx->pxi_motion_session_instance)
	{
		return;
	}

	std::int32_t train_pos_mm = 0;
	if (!ctx->pxi_motion_session_instance->try_get_train_pos_mm(&train_pos_mm))
	{
		return;
	}

	ctx->train_pos_cm = train_pos_mm / 10;
}

static void SyncPxiSessionTrainPosition(AppContext* ctx) noexcept
{
	if (ctx == nullptr || !ctx->pxi_session_instance)
	{
		return;
	}

	ctx->pxi_session_instance->set_train_pos_cm(ctx->train_pos_cm);
}

void SyncOutput(AppContext* ctx)
{
	if (ctx == nullptr)
	{
		return;
	}

	ExternalDeviceDataOutput(ctx);
	MaintenanceDataOutput(ctx);
	OtherAswDataOutput(ctx);

	for (const std::unique_ptr<session>& session_ptr : ctx->snmp_sessions)
	{
		if (session_ptr)
		{
			(void)session_ptr->output();
		}
	}
	 
	for (const std::unique_ptr<session>& session_ptr : ctx->safety037_sessions)
	{
		if (session_ptr)
		{
			(void)session_ptr->output();
		}
	}

	if (ctx->pxi_session_instance)
	{
		(void)ctx->pxi_session_instance->output();
	}

	if (ctx->pxi_motion_session_instance)
	{
		(void)ctx->pxi_motion_session_instance->output();
	}
}

static void ExternalDeviceDataOutput(AppContext* ctx)
{
	PFMSG_t app_msg = { 0 };
	const Logger::Ptr log = Logger::get("simulation");

	if (ctx == nullptr)
	{
		return;
	}

	while (CVC_BUFFER_OPER_SUCCESS == CVC_BSW_ITF_Read(CVC_BSW_APP_TX_TYPE, reinterpret_cast<unsigned char*>(&app_msg)))
	{
		bool is_session_type = true;
		bool session_found = false;

		switch (app_msg.AppType)
		{
		case APP_TYPE_C_EXT:
			session_found = has_ext_session(ctx, app_msg.PeripheralNumber);
			(void)output_to_ext_session(ctx, app_msg.PeripheralNumber, app_msg.Message, app_msg.MsgSize);
			break;

		case APP_TYPE_C_RSSP1:
			session_found = has_rsspi_session(ctx, app_msg.PeripheralNumber);
			(void)output_to_rsspi_session(ctx, app_msg.PeripheralNumber, app_msg.Message, app_msg.MsgSize);
			break;

		case APP_TYPE_C_RAW:
			if (has_raw_session(ctx, app_msg.PeripheralNumber))
			{
				session_found = true;
				(void)output_to_raw_session(ctx, app_msg.PeripheralNumber, app_msg.Message, app_msg.MsgSize);
			}
			else if (has_other_asw_session(ctx, app_msg.PeripheralNumber))
			{
				session_found = true;
				(void)output_to_other_asw_session(ctx, app_msg.PeripheralNumber, app_msg.Message, app_msg.MsgSize);
			}
			else
			{
				session_found = has_maint_session(ctx, app_msg.PeripheralNumber);
				(void)output_to_maint_session(ctx, app_msg.PeripheralNumber, app_msg.Message, app_msg.MsgSize);
			}
			break;

		case APP_TYPE_C_DY037:
			{
				const bool match_by_id_037 = app_msg.MsgID == DY037_MSG_C_CONNECT;
				const uint32_t matching_number = match_by_id_037
					? static_cast<uint32_t>(app_msg.Message[0])
					: app_msg.PeripheralNumber;
				session_found = has_safety037_session(ctx, matching_number, match_by_id_037);
				(void)output_to_safety037_session(ctx, matching_number, match_by_id_037, app_msg.Message, app_msg.MsgSize, app_msg.MsgID, app_msg.PeripheralNumber);
			}
			break;

		default:
			is_session_type = false;
			break;
		}

		if (is_session_type && !session_found)
		{
			log->warn("session not found for external device data: type=%u peripheral=0x%08X",
				static_cast<unsigned int>(app_msg.AppType),
				static_cast<unsigned int>(app_msg.PeripheralNumber));
		}
	}
}
 
static void MaintenanceDataOutput(AppContext* ctx)
{
	const Logger::Ptr log = Logger::get("simulation");

	if (ctx == nullptr)
	{ 
		return;
	} 

	std::uint16_t msg_count = 0U;
	if (CVC_TRUE != CVC_BSW_ITF_MantInfo_GetMsgNum(&msg_count))
	{
		return;
	}
	 
	unsigned char maint_info_buffer[sizeof(MANTINFO_HEAD_t) + MAX_MANT_MSG_SIZE] = { 0 };
	for (std::uint16_t index = 0U; index < msg_count; ++index)
	{
		if (CVC_TRUE != CVC_BSW_ITF_MantInfo_Read(maint_info_buffer))
		{
			break;
		}

		const MANTINFO_HEAD_t* const maint_info = reinterpret_cast<const MANTINFO_HEAD_t*>(maint_info_buffer);
		const unsigned char* const payload = maint_info_buffer + sizeof(MANTINFO_HEAD_t);
		const session_result output_result = output_to_maint_session(ctx, maint_info->PeripheralNumber,
			payload, maint_info->MsgSize);
		if (output_result != session_result::ok)
		{
			log->warn("maint session output failed: peripheral=0x%08X result=%d",
				static_cast<unsigned int>(maint_info->PeripheralNumber),
				static_cast<int>(output_result));
		}
	}
}

static void OtherAswDataOutput(AppContext* ctx)
{
	const Logger::Ptr log = Logger::get("simulation");

	if (ctx == nullptr)
	{
		return;
	}

	std::uint16_t msg_count = 0U;
	if (CVC_TRUE != CVC_BSW_ITF_GetMsgNum(CVC_BSW_ASW_COM_TX_TYPE, &msg_count))
	{
		return;
	}

	ASWTxData_t tx_data = { 0 };
	for (std::uint16_t msg_index = 0U; msg_index < msg_count; ++msg_index)
	{
		if (CVC_BUFFER_OPER_SUCCESS != CVC_BSW_ITF_Read(CVC_BSW_ASW_COM_TX_TYPE, reinterpret_cast<unsigned char*>(&tx_data)))
		{
			break;
		}

		if (tx_data.Size > ASW_COM_DATA_SIZE)
		{
			log->warn("asw tx invalid size=%u", static_cast<unsigned int>(tx_data.Size));
			continue;
		}

		const std::uint16_t id_count = static_cast<std::uint16_t>(tx_data.IDNum);
		for (std::uint16_t id_index = 0U; id_index < id_count; ++id_index)
		{
			const std::uint32_t peripheral_number = static_cast<std::uint32_t>(tx_data.DistIDS[id_index]);
			const session_result output_result = output_to_other_asw_session(ctx, peripheral_number, tx_data.Data, tx_data.Size);
			if (output_result != session_result::ok)
			{ 
				log->warn("other_asw output failed: peripheral=0x%08X result=%d",
					static_cast<unsigned int>(peripheral_number),
					static_cast<int>(output_result));
			}
		} 
	} 
}
 
static bool has_ext_session(const AppContext* ctx, uint32_t peripheral_number)
{
	for (const std::unique_ptr<session>& session_ptr : ctx->ext_sessions)
	{
		if (session_ptr == nullptr)
		{
			continue;
		}

		ext_session* const ext = static_cast<ext_session*>(session_ptr.get());
		if (ext->peripheral_number() == peripheral_number)
		{
			return true;
		}
	}

	return false;
}

static bool has_raw_session(const AppContext* ctx, uint32_t peripheral_number)
{
	for (const std::unique_ptr<session>& session_ptr : ctx->raw_sessions)
	{
		if (session_ptr && session_ptr->id() == static_cast<int>(peripheral_number))
		{
			return true;
		}
	}

	return false;
}

static bool has_other_asw_session(const AppContext* ctx, uint32_t peripheral_number)
{
	for (const std::unique_ptr<session>& session_ptr : ctx->other_asw_sessions)
	{
		if (session_ptr && session_ptr->id() == static_cast<int>(peripheral_number))
		{
			return true;
		}
	}

	return false;
}

static bool has_maint_session(const AppContext* ctx, uint32_t peripheral_number)
{
	for (const std::unique_ptr<session>& session_ptr : ctx->maint_sessions)
	{
		if (session_ptr && session_ptr->id() == static_cast<int>(peripheral_number))
		{
			return true;
		}
	}

	return false;
}

static bool has_rsspi_session(const AppContext* ctx, uint32_t peripheral_number)
{
	for (const std::unique_ptr<session>& session_ptr : ctx->rsspi_sessions)
	{
		if (session_ptr && session_ptr->id() == static_cast<int>(peripheral_number))
		{
			return true;
		}
	}

	return false;
}

static bool has_safety037_session(const AppContext* ctx, uint32_t matching_number, bool match_by_id_037)
{
	for (const std::unique_ptr<session>& session_ptr : ctx->safety037_sessions)
	{
		if (session_ptr == nullptr)
		{
			continue;
		}

		safety037_session* const safety_session = static_cast<safety037_session*>(session_ptr.get());
		const bool is_match = match_by_id_037
			? session_ptr->id() == static_cast<int>(matching_number)
			: safety_session->peripheral_number() == matching_number;
		if (is_match)
		{
			return true;
		}
	}

	return false;
}

static session_result output_to_ext_session(AppContext* ctx, uint32_t peripheral_number, const unsigned char* buffer, std::size_t len)
{
	bool matched = false;
	session_result last_result = session_result::internal_error;

	for (const std::unique_ptr<session>& session_ptr : ctx->ext_sessions)
	{
		if (session_ptr == nullptr)
		{
			continue;
		}

		ext_session* const ext = static_cast<ext_session*>(session_ptr.get());
		if (ext->peripheral_number() == peripheral_number)
		{
			matched = true;
			last_result = ext->output(buffer, len);
			if (last_result != session_result::ok)
			{
				return last_result;
			}
		}
	}

	return matched ? session_result::ok : session_result::internal_error;
}

static session_result output_to_maint_session(AppContext* ctx, uint32_t peripheral_number, const unsigned char* buffer, std::size_t len)
{
	for (const std::unique_ptr<session>& session_ptr : ctx->maint_sessions)
	{
		if (session_ptr && session_ptr->id() == static_cast<int>(peripheral_number))
		{
			return static_cast<maint_session*>(session_ptr.get())->output(buffer, len);
		}
	}

	return session_result::invalid_argument;
}

static session_result output_to_raw_session(AppContext* ctx, uint32_t peripheral_number, const unsigned char* buffer, std::size_t len)
{
	for (const std::unique_ptr<session>& session_ptr : ctx->raw_sessions)
	{
		if (session_ptr && session_ptr->id() == static_cast<int>(peripheral_number))
		{
			return static_cast<raw_session*>(session_ptr.get())->output(buffer, len);
		}
	}

	return session_result::invalid_argument;
}

static session_result output_to_other_asw_session(AppContext* ctx, uint32_t peripheral_number, const unsigned char* buffer, std::size_t len)
{
	for (const std::unique_ptr<session>& session_ptr : ctx->other_asw_sessions)
	{
		if (session_ptr && session_ptr->id() == static_cast<int>(peripheral_number))
		{
			return static_cast<other_asw_session*>(session_ptr.get())->output(buffer, len);
		}
	}

	return session_result::invalid_argument;
}

static session_result output_to_rsspi_session(AppContext* ctx, uint32_t peripheral_number, const unsigned char* buffer, std::size_t len)
{
	for (const std::unique_ptr<session>& session_ptr : ctx->rsspi_sessions)
	{
		if (session_ptr && session_ptr->id() == static_cast<int>(peripheral_number))
		{
			return static_cast<rsspi_session*>(session_ptr.get())->output(buffer, len);
		}
	}

	return session_result::invalid_argument;
}

static session_result output_to_safety037_session(AppContext* ctx, uint32_t matching_number, bool match_by_id_037, const unsigned char* buffer, std::size_t len, unsigned char msg_id, uint32_t peripheral_number)
{
	for (const std::unique_ptr<session>& session_ptr : ctx->safety037_sessions)
	{
		if (session_ptr == nullptr)
		{
			continue;
		}

		safety037_session* const safety_session = static_cast<safety037_session*>(session_ptr.get());
		const bool is_match = match_by_id_037
			? session_ptr->id() == static_cast<int>(matching_number)
			: safety_session->peripheral_number() == matching_number;
		if (is_match)
		{
			return safety_session->output(buffer, len, msg_id, peripheral_number);
		}
	}

	return session_result::invalid_argument;
}