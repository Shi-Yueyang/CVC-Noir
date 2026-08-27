#include "snmp_session.h"

#include <array>
#include <cstdint>
#include <cstring>

#include "../Pub_Comm/connection_factory.h"
#include "../../ASW_300C/interface_p2a.h"
#include "../../ASW_300C/Interface_Data.h"

namespace {

constexpr unsigned char kSnmpHeaderByte = 0xBFU;
constexpr std::size_t kSnmpMobileStateFieldCount = 4U;
constexpr std::size_t kSnmpMobileStateSize = kSnmpMobileStateFieldCount * sizeof(std::int32_t);
constexpr std::size_t kSnmpMobileStateOffset = 1U;
constexpr std::size_t kSnmpMinimumStatePacketSize = kSnmpMobileStateOffset + kSnmpMobileStateSize;

bool try_update_mobile_state(const unsigned char* buffer, std::size_t size, std::array<std::int32_t, 4>* mobile_state) noexcept
{
	if (buffer == nullptr || mobile_state == nullptr)
	{
		return false;
	}

	if (size < kSnmpMinimumStatePacketSize || buffer[0] != kSnmpHeaderByte)
	{
		return false;
	}

	std::memcpy(mobile_state->data(), buffer + kSnmpMobileStateOffset, kSnmpMobileStateSize);
	return true;
}

}

snmp_session::snmp_session(const nlohmann::json& config)
	: session(config)
	, mobile_state_({ 0, 0, 2, 0 })
{
	if (config.is_object() && config.contains("connection") && config["connection"].is_object())
	{
		connection_ = connection_factory::create(config["connection"]);
	}
}

snmp_session::~snmp_session()
{
}

session_result snmp_session::do_input() noexcept
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
			(void)try_update_mobile_state(buffer.data(), received_size, &mobile_state_);
			if (CVC_TRUE != writePFMsg(1, APP_TYPE_C_SNMP, id_, buffer.data(), static_cast<INT16U>(received_size)))
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

session_result snmp_session::do_output() noexcept
{
	if (!connection_)
	{
		return session_result::invalid_argument;
	}

	const unsigned char mobile_state = mobile_state_[3] == 0 ? 1U : 3U;
	const unsigned char packet[2] = { kSnmpHeaderByte, mobile_state };
	return session_map_send_result(connection_->send(packet, sizeof(packet)));
}