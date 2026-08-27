#include "other_asw_session.h"

#include <array>
#include <cstdint>
#include <cstring>

#include "../Pub_Comm/connection_factory.h"
#include "../../ASW_300C/interface_p2a.h"
#include "../../ASW_300C/Interface_Data.h"

namespace {
}

other_asw_session::other_asw_session(const nlohmann::json& config)
	: session(config)
{
	if (config.is_object() && config.contains("connection") && config["connection"].is_object())
	{
		connection_ = connection_factory::create(config["connection"]);
	}
}

other_asw_session::~other_asw_session()
{
}

session_result other_asw_session::do_input() noexcept
{
	if (!connection_)
	{
		return session_result::invalid_argument;
	}

	std::array<unsigned char, 2048> buffer = {};
	std::size_t received_size = 0U;
	bool received_any_packet = false;

	for (;;)
	{
		const connection_result res = connection_->receive(buffer.data(), buffer.size(), &received_size);
		if (res == connection_result::ok)
		{
			if (received_size > ASW_COM_DATA_SIZE)
			{
				return session_result::invalid_argument;
			}

			ASWRxData_t asw_rx_data = { 0 };
			asw_rx_data.SrcID = static_cast<std::uint16_t>(id_);
			asw_rx_data.Size = static_cast<std::uint16_t>(received_size);
			std::memcpy(asw_rx_data.Data, buffer.data(), received_size);
			const std::uint16_t data_size = static_cast<std::uint16_t>(sizeof(ASWRxData_t) - ASW_COM_DATA_SIZE + asw_rx_data.Size);

			if (CVC_TRUE != CVC_BSW_ITF_Write(CVC_BSW_ASW_COM_RX_TYPE, reinterpret_cast<unsigned char*>(&asw_rx_data), data_size))
			{
				return session_result::forward_failed;
			}
			received_any_packet = true;
			continue;
		}

		if (res == connection_result::would_block)
		{
			break;
		}

		return session_map_receive_result(res);
	}

	if (!received_any_packet)
	{
		return session_result::would_block;
	}

	return session_result::ok;
}

session_result other_asw_session::do_output() noexcept
{
	return session_result::ok;
}

session_result other_asw_session::output(const unsigned char* buffer, std::size_t len) noexcept
{
	if (is_skip())
	{
		return session_result::ok;
	}

	if (!connection_)
	{
		return session_result::invalid_argument;
	}

	if (buffer == nullptr || len == 0U)
	{
		return session_result::invalid_argument;
	}

	return session_map_send_result(connection_->send(buffer, len));
}