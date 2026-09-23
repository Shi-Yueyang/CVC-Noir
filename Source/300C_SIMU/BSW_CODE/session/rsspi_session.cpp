#include "rsspi_session.h"

#include <array>

#include "../Pub_Comm/connection_factory.h"
#include "../../ASW_300C/interface_p2a.h"
#include "../../ASW_300C/Interface_Data.h"
#include "../../logging/logger.h"

namespace {

Logger::Ptr get_rssp1_log()
{
	return Logger::get("rssp1");
}

}

rsspi_session::rsspi_session(const nlohmann::json& config)
	: session(config)
{
	if (config.is_object() && config.contains("connection") && config["connection"].is_object())
	{
		connection_ = connection_factory::create(config["connection"]);
	}
}

rsspi_session::~rsspi_session()
{
}

session_result rsspi_session::do_input() noexcept
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
			get_rssp1_log()->trace("%s-0x%08X in: len=%zu",
				name().c_str(),
				static_cast<unsigned int>(id_),
				received_size);

			if (CVC_TRUE != writePFMsg(RSSPI_MSG_C_TO_APP_DATA, APP_TYPE_C_RSSP1, id_, buffer.data(), static_cast<INT16U>(received_size)))
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

session_result rsspi_session::do_output() noexcept
{
	return session_result::ok;
}

session_result rsspi_session::output(const unsigned char* buffer, std::size_t len) noexcept
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

	const session_result send_result = session_map_send_result(connection_->send(buffer, len));
	get_rssp1_log()->trace("%s-0x%08X out: len=%zu result=%d",
		name().c_str(),
		static_cast<unsigned int>(id_),
		len,
		static_cast<int>(send_result));
	return send_result;
}