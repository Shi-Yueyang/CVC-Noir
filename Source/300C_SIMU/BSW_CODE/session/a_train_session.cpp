#include "a_train_session.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

#include "../../ASW_300C/Interface_Data.h"
#include "../../logging/logger.h"
#include "../Pub_Comm/connection_factory.h"

namespace {

Logger::Ptr get_a_train_log()
{
	return Logger::get("a_train");
}

}

std::string a_train_atp_payload_line(const IOData_t& vob_data)
{
	std::size_t signal_size = 0U;
	for (std::size_t i = 0U; i < vob_data.Length; ++i)
	{
		signal_size = (std::max)(signal_size,
			static_cast<std::size_t>(vob_data.IOPortData[i].PortIndex) + 1U);
	}

	std::string atp_signal(signal_size, '0');
	for (std::size_t i = 0U; i < vob_data.Length; ++i)
	{
		const std::size_t port_index = vob_data.IOPortData[i].PortIndex;
		atp_signal[port_index] = vob_data.IOPortData[i].PortValue == 1U ? '1' : '0';
	}
	for (std::size_t position = 5U; position < atp_signal.size(); position += 6U)
	{
		atp_signal.insert(position, 1U, '_');
	}

	const nlohmann::ordered_json payload = {
		{"type", "atp_command"},
		{"train_id", a_train_hardcoded_train_id()},
		{"cab_id", a_train_hardcoded_cab_id()},
		{"atp_signal", atp_signal}
	};
	return payload.dump() + "\n";
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

	bool sent_any_packet = false;
	IOData_t vob_data = {};
	while (CVC_BUFFER_OPER_SUCCESS == CVC_BSW_ITF_Read(CVC_BSW_VOB_TX_TYPE, reinterpret_cast<INT8U*>(&vob_data)))
	{
		if (vob_data.Length > MAX_IOPORT_NUM)
		{
			return session_result::invalid_argument;
		}

		const std::string payload = a_train_atp_payload_line(vob_data);
		get_a_train_log()->trace("%s out: %s", name().c_str(), payload.c_str());
		const connection_result send_result = connection_->send(
			reinterpret_cast<const unsigned char*>(payload.data()), payload.size());
		if (send_result != connection_result::ok)
		{
			return session_map_send_result(send_result);
		}

		sent_any_packet = true;
		vob_data = {};
	}

	return sent_any_packet ? session_result::ok : session_result::would_block;
}
