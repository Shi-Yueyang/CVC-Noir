#include "ext_session.h"

#include <algorithm>
#include <array>
#include <limits>
#include <vector>
#include <iostream>
#include "../Pub_Comm/connection_factory.h"
#include "../../ASW_300C/interface_p2a.h"
#include "../../ASW_300C/Interface_Data.h"

namespace {
std::uint32_t parse_u32_value(const nlohmann::json& value, std::uint32_t default_value) noexcept
{
	if (value.is_number_unsigned())
	{
		return value.get<std::uint32_t>();
	}

	if (value.is_number_integer())
	{
		const int parsed = value.get<int>();
		return parsed >= 0 ? static_cast<std::uint32_t>(parsed) : default_value;
	}

	if (value.is_string())
	{
		const int parsed = session_parse_id(value);
		return parsed >= 0 ? static_cast<std::uint32_t>(parsed) : default_value;
	}

	return default_value;
}

std::vector<std::uint8_t> parse_byte_array(const nlohmann::json& config, const char* key)
{
	std::vector<std::uint8_t> result;
	if (!config.is_object() || key == nullptr || !config.contains(key) || !config[key].is_array())
	{
		return result;
	}

	for (const nlohmann::json& item : config[key])
	{
		const std::uint32_t value = parse_u32_value(item, 256U);
		if (value <= 0xFFU)
		{
			result.push_back(static_cast<std::uint8_t>(value));
		}
	}

	return result;
}

}

ext_session::ext_session(const nlohmann::json& config)
	: session(config)
	, prefix_zero_bytes_(config.is_object() && config.contains("prefix_zero_bytes") ? static_cast<std::size_t>(parse_u32_value(config["prefix_zero_bytes"], 0U)) : 0U)
	, idle_payload_(parse_byte_array(config, "idle_payload"))
{
	if (config.is_object() && config.contains("connection") && config["connection"].is_object())
	{
		connection_ = connection_factory::create(config["connection"]);
	}
}

ext_session::~ext_session()
{
}

session_result ext_session::do_input() noexcept
{
	if (!connection_)
	{
		return session_result::invalid_argument;
	}

	if (prefix_zero_bytes_ >= MAX_BSWMSG_SIZE)
	{
		return session_result::invalid_argument;
	}

	std::array<std::uint8_t, MAX_BSWMSG_SIZE> buffer = {};
	std::size_t received_size = 0U;
	bool received_any_packet = false;

	for (;;)
	{
		const connection_result res = connection_->receive(buffer.data(), buffer.size(), &received_size);
		if (res == connection_result::ok)
		{
			const session_result publish_result = publish_payload(buffer.data(), received_size);
			if (publish_result != session_result::ok)
			{
				return publish_result;
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
		if (!idle_payload_.empty())
		{
			return publish_payload(idle_payload_.data(), idle_payload_.size());
		}
		return session_result::would_block;
	}

	return session_result::ok;
}

session_result ext_session::do_output() noexcept
{
	return session_result::ok;
}

session_result ext_session::output(const std::uint8_t* buffer, std::size_t len) noexcept
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

std::uint32_t ext_session::peripheral_number() const noexcept
{
	return static_cast<std::uint32_t>(id_);
}

session_result ext_session::publish_payload(const std::uint8_t* payload, std::size_t len) noexcept
{
	const std::size_t max_payload_per_message = MAX_BSWMSG_SIZE - prefix_zero_bytes_;
	std::array<std::uint8_t, MAX_BSWMSG_SIZE> pf_buffer = {};
	std::size_t offset = 0U;

	if (payload == nullptr || len == 0U)
	{
		return session_result::invalid_argument;
	}

	if (max_payload_per_message == 0U)
	{
		return session_result::invalid_argument;
	}

	while (offset < len)
	{
		const std::size_t chunk_size = (std::min)(max_payload_per_message, len - offset);
		//std::cout << "len " << len << " chunk_size " << chunk_size << " offset " << offset << std::endl;
		if (prefix_zero_bytes_ > 0U)
		{
			std::fill_n(pf_buffer.data(), prefix_zero_bytes_, static_cast<std::uint8_t>(0));
		}
		std::copy(payload + offset, payload + offset + chunk_size, pf_buffer.data() + prefix_zero_bytes_);
		const std::size_t pf_size = prefix_zero_bytes_ + chunk_size;
		if (pf_size > static_cast<std::size_t>((std::numeric_limits<std::uint16_t>::max)()))
		{
			return session_result::invalid_argument;
		}
		if (CVC_TRUE != writePFMsg(
			static_cast<std::uint8_t>(RAW_MSG_C_TO_APP_DATA),
			static_cast<std::uint8_t>(APP_TYPE_C_EXT),
			static_cast<std::uint32_t>(id_),
			reinterpret_cast<INT8U*>(pf_buffer.data()),
			static_cast<std::uint16_t>(pf_size)))
		{
			return session_result::forward_failed;
		}
		offset += chunk_size;
	}

	return session_result::ok;
}
