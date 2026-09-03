#include "a_train_session.h"

#include <array>
#include <cstddef>

#include "../../logging/logger.h"
#include "../Pub_Comm/connection_factory.h"

namespace {

Logger::Ptr get_a_train_log()
{
	return Logger::get("a_train");
}

}

a_train_session::a_train_session(const nlohmann::json& config)
	: session(config)
{
	if (config.is_object() && config.contains("connection") && config["connection"].is_object())
	{
		connection_ = connection_factory::create(config["connection"]);
	}
}

a_train_session::~a_train_session()
{
}

session_result a_train_session::do_input() noexcept
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
			get_a_train_log()->trace("%s in: len=%zu data=%.*s",
				name().c_str(),
				received_size,
				static_cast<int>(received_size),
				reinterpret_cast<const char*>(buffer.data()));
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

session_result a_train_session::do_output() noexcept
{
	if (!connection_)
	{
		return session_result::invalid_argument;
	}

	const char* const payload = a_train_dummy_payload_line();
	const std::size_t payload_size = std::char_traits<char>::length(payload);

	return session_map_send_result(connection_->send(reinterpret_cast<const unsigned char*>(payload), payload_size));
}
