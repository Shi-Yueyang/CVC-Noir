#include "safety037_session.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <string>

#include "../Pub_Comm/connection_factory.h"
#include "../Pub_Comm/udp_connection.h"
#include "../../logging/logger.h"
#include "../../ASW_300C/interface_p2a.h"
#include "../../ASW_300C/Interface_Data.h"

namespace {

constexpr int kDefaultConnectSuccessDelayMs = 1000;
constexpr int kDefaultDisconnectFailureDelayMs = 200;
constexpr int kDefaultRecvTimeoutMs = -1;

Logger::Ptr get_037_log(const std::string& session_name)
{
	return Logger::get("037_" + session_name);
}

const char* dy037_status_name(unsigned char msg_id) noexcept
{
	switch (msg_id)
	{
	case DY037_MSG_C_CONNECTION_SUCCESS:
		return "CONNECTION_SUCCESS";
	case DY037_MSG_C_CONNECTION_LOST:
		return "CONNECTION_LOST";
	case DY037_MSG_C_CONNECTION_FAILURE:
		return "CONNECTION_FAILURE";
	default:
		return "UNKNOWN_STATUS";
	}
}

unsigned int parse_u32_value(const nlohmann::json& value, unsigned int default_value = 0U) noexcept
{
	if (value.is_number_unsigned())
	{
		return value.get<unsigned int>();
	}

	if (value.is_number_integer())
	{
		const int parsed = value.get<int>();
		return parsed >= 0 ? static_cast<unsigned int>(parsed) : default_value;
	}

	if (value.is_string())
	{
		const int parsed = session_parse_id(value);
		return parsed >= 0 ? static_cast<unsigned int>(parsed) : default_value;
	}

	return default_value;
}

int parse_timeout_value(const nlohmann::json& config, const char* key, int default_value) noexcept
{
	if (!config.is_object() || key == nullptr || !config.contains(key))
	{
		return default_value;
	}

	const nlohmann::json& value = config[key];
	if (value.is_number_integer())
	{
		return value.get<int>();
	}

	if (value.is_string())
	{
		return session_parse_id(value);
	}

	return default_value;
}

bool parse_bool_value(const nlohmann::json& config, const char* key, bool default_value) noexcept
{
	if (!config.is_object() || key == nullptr || !config.contains(key))
	{
		return default_value;
	}

	const nlohmann::json& value = config[key];
	if (value.is_boolean())
	{
		return value.get<bool>();
	}

	if (value.is_number_integer())
	{
		return value.get<int>() != 0;
	}

	if (value.is_string())
	{
		const std::string text = value.get<std::string>();
		return text == "true" || text == "1";
	}

	return default_value;
}

bool parse_bool_node(const nlohmann::json& value, bool default_value) noexcept
{
	if (value.is_boolean())
	{
		return value.get<bool>();
	}

	if (value.is_number_integer())
	{
		return value.get<int>() != 0;
	}

	if (value.is_string())
	{
		const std::string text = value.get<std::string>();
		return text == "true" || text == "1";
	}

	return default_value;
}

safety037_session::peer_mapping parse_peer_mapping(const nlohmann::json& mapping_config)
{
	safety037_session::peer_mapping mapping = {};
	if (!mapping_config.is_object())
	{
		return mapping;
	}

	const nlohmann::json& from = mapping_config.contains("from") ? mapping_config["from"] : nlohmann::json();
	const nlohmann::json& to = mapping_config.contains("to") ? mapping_config["to"] : nlohmann::json();
	if (from.is_object())
	{
		if (from.contains("ip") && from["ip"].is_string())
		{
			mapping.from_ip = from["ip"].get<std::string>();
		}
		if (from.contains("port"))
		{
			mapping.from_port = parse_u32_value(from["port"]);
		}
	}
	if (to.is_object())
	{
		if (to.contains("ip") && to["ip"].is_string())
		{
			mapping.to_ip = to["ip"].get<std::string>();
		}
		if (to.contains("port"))
		{
			mapping.to_port = parse_u32_value(to["port"]);
		}
	}

	return mapping;
}

std::vector<safety037_session::peer_mapping> parse_peer_mappings(const nlohmann::json& config)
{
	std::vector<safety037_session::peer_mapping> mappings;
	if (!config.is_object() || !config.contains("dynamic_peer") || !config["dynamic_peer"].is_object())
	{
		return mappings;
	}

	const nlohmann::json& dynamic_peer = config["dynamic_peer"];
	if (!dynamic_peer.contains("peer_mappings") || !dynamic_peer["peer_mappings"].is_array())
	{
		return mappings;
	}

	for (const nlohmann::json& mapping_config : dynamic_peer["peer_mappings"])
	{
		const safety037_session::peer_mapping mapping = parse_peer_mapping(mapping_config);
		if (!mapping.from_ip.empty() && mapping.from_port != 0U && !mapping.to_ip.empty() && mapping.to_port != 0U)
		{
			mappings.push_back(mapping);
		}
	}

	return mappings;
}

bool parse_dynamic_peer_enabled(const nlohmann::json& config, const std::vector<safety037_session::peer_mapping>& peer_mappings) noexcept
{
	if (!config.is_object() || !config.contains("dynamic_peer"))
	{
		return false;
	}

	const nlohmann::json& dynamic_peer = config["dynamic_peer"];
	if (dynamic_peer.is_object())
	{
		if (dynamic_peer.contains("enabled"))
		{
			return parse_bool_node(dynamic_peer["enabled"], false) || !peer_mappings.empty();
		}

		return !peer_mappings.empty();
	}

	return parse_bool_node(dynamic_peer, false);
}

}

safety037_session::safety037_session(const nlohmann::json& config)
	: session(config)
	, recv_no_data_send_lost_ms_(parse_timeout_value(config, "recv_no_data_send_lost_ms", kDefaultRecvTimeoutMs))
	, connect_success_delay_ms_(parse_timeout_value(config, "connect_success_delay_ms", kDefaultConnectSuccessDelayMs))
	, disconnect_failure_delay_ms_(parse_timeout_value(config, "disconnect_failure_delay_ms", kDefaultDisconnectFailureDelayMs))
	, peripheral_number_(0U)
	, dynamic_peer_(false)
	, peer_mappings_(parse_peer_mappings(config))
	, connected_(false)
	, connect_pending_(false)
	, disconnect_pending_(false)
	, recv_no_data_send_lost_armed_(false)
	, connect_deadline_()
	, disconnect_deadline_()
	, last_receive_time_()
{
	dynamic_peer_ = parse_dynamic_peer_enabled(config, peer_mappings_);
	if (config.is_object() && config.contains("connection") && config["connection"].is_object())
	{
		connection_ = connection_factory::create(config["connection"]);
	}
}

safety037_session::~safety037_session()
{
}

session_result safety037_session::do_input() noexcept
{
	if (!connection_)
	{
		return session_result::invalid_argument;
	}

	std::array<unsigned char, 2048> buffer = {};
	std::size_t received_size = 0U;
	bool received_any_packet = false;
	const clock_type::time_point now = clock_type::now();

	for (;;)
	{
		const connection_result res = connection_->receive(buffer.data(), buffer.size(), &received_size);
		if (res == connection_result::ok)
		{
			get_037_log(name())->trace("%s-0x%08X in: (%u) len=%zu",
				name().c_str(),
				peripheral_number(),
				received_size > 0U ? static_cast<unsigned int>(buffer[0]) : 0U,
				received_size);

			if (CVC_TRUE != writePFMsg(DY037_MSG_C_TO_APP_DATA, APP_TYPE_C_DY037, peripheral_number(), buffer.data(), static_cast<INT16U>(received_size)))
			{
				return session_result::forward_failed;
			}

			received_any_packet = true;
			handle_receive_activity(now);
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

session_result safety037_session::do_output() noexcept
{
	if (!connection_)
	{
		return session_result::invalid_argument;
	}

	const clock_type::time_point now = clock_type::now();

	if (connect_pending_ && now >= connect_deadline_)
	{
		connect_pending_ = false;
		connected_ = true;
		if (recv_no_data_send_lost_ms_ > 0)
		{
			recv_no_data_send_lost_armed_ = true;
			last_receive_time_ = now;
		}

		return emit_status(DY037_MSG_C_CONNECTION_SUCCESS);
	}

	if (disconnect_pending_ && now >= disconnect_deadline_)
	{
		disconnect_pending_ = false;
		connected_ = false;
		recv_no_data_send_lost_armed_ = false;

		return emit_status(DY037_MSG_C_CONNECTION_FAILURE);
	}

	if (connected_ && recv_no_data_send_lost_armed_ && recv_no_data_send_lost_ms_ > 0)
	{
		const std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_receive_time_);
		if (elapsed.count() >= recv_no_data_send_lost_ms_)
		{
			connected_ = false;
			recv_no_data_send_lost_armed_ = false;
			return emit_status(DY037_MSG_C_CONNECTION_LOST);
		}
	}

	return session_result::ok;
}

session_result safety037_session::output(const unsigned char* buffer, std::size_t len, unsigned char msg_id, unsigned int peripheral_number) noexcept
{
	if (is_skip())
	{
		return session_result::ok;
	}

	if (!connection_)
	{
		return session_result::invalid_argument;
	}

	switch (msg_id)
	{
	case DY037_MSG_C_CONNECT:
		peripheral_number_ = peripheral_number;
		get_037_log(name())->debug("%s-0x%08X [REQ] connect_request",
			name().c_str(),
			this->peripheral_number());

		if (dynamic_peer_ && !try_apply_dynamic_peer(buffer, len))
		{
			get_037_log(name())->debug("%s-0x%08X dynamic_peer_update_failed",
				name().c_str(),
				this->peripheral_number());
			return session_result::invalid_argument;
		}

		schedule_connect_success();
		return session_result::ok;

	case DY037_MSG_C_DISCONNECT:
		get_037_log(name())->debug("%s-0x%08X [REQ] disconnect_request",
			name().c_str(),
			this->peripheral_number());
		schedule_disconnect_failure();
		return session_result::ok;

	case DY037_MSG_C_FROM_APP_DATA:
		if (buffer == nullptr || len == 0U)
		{
			return session_result::invalid_argument;
		}

		get_037_log(name())->trace("%s-0x%08X out: (%u) len=%zu",
			name().c_str(),
			peripheral_number,
			static_cast<unsigned int>(buffer[0]),
			len);

		return session_map_send_result(connection_->send(buffer, len));

	default:
		return session_result::invalid_argument;
	}
}

unsigned int safety037_session::peripheral_number() const noexcept
{
	return peripheral_number_;
}

void safety037_session::schedule_connect_success() noexcept
{
	const clock_type::time_point now = clock_type::now();
	connect_pending_ = true;
	disconnect_pending_ = false;
	connected_ = false;
	recv_no_data_send_lost_armed_ = false;
	connect_deadline_ = now + std::chrono::milliseconds(connect_success_delay_ms_);
}

void safety037_session::schedule_disconnect_failure() noexcept
{
	const clock_type::time_point now = clock_type::now();
	disconnect_pending_ = true;
	connect_pending_ = false;
	connected_ = false;
	recv_no_data_send_lost_armed_ = false;
	disconnect_deadline_ = now + std::chrono::milliseconds(disconnect_failure_delay_ms_);
}

void safety037_session::handle_receive_activity(clock_type::time_point now) noexcept
{
	if (recv_no_data_send_lost_ms_ <= 0)
	{
		return;
	}

	if (connected_)
	{
		recv_no_data_send_lost_armed_ = true;
		last_receive_time_ = now;
	}
}

session_result safety037_session::emit_status(unsigned char msg_id) noexcept
{
	unsigned char buffer[1] = { 0U };
	get_037_log(name())->debug("%s-0x%08X [RSP] emit_status=%s",
		name().c_str(),
		peripheral_number(),
		dy037_status_name(msg_id));
	if (CVC_TRUE != writePFMsg(msg_id, APP_TYPE_C_DY037, peripheral_number(), buffer, 0U))
	{
		get_037_log(name())->debug("%s-0x%08X emit_status_forward_failed",
			name().c_str(),
			peripheral_number());
		return session_result::forward_failed;
	}

	return session_result::ok;
}

bool safety037_session::try_apply_dynamic_peer(const unsigned char* buffer, std::size_t len) noexcept
{
	constexpr std::size_t kConnectPayloadSize = 7U;
	constexpr std::size_t kSessionIndexOffset = 0U;
	constexpr std::size_t kIpOffset = 1U;
	constexpr std::size_t kPortOffset = 5U;

	if (buffer == nullptr || len < kConnectPayloadSize)
	{
		return false;
	}

	udp_connection* const udp = dynamic_cast<udp_connection*>(connection_.get());
	if (udp == nullptr)
	{
		return false;
	}

	const unsigned int connect_session_index = static_cast<unsigned int>(buffer[kSessionIndexOffset]);
	char ip_text[16] = { 0 };
	const int ip_written = std::snprintf(
		ip_text,
		sizeof(ip_text),
		"%u.%u.%u.%u",
		static_cast<unsigned int>(buffer[kIpOffset + 3U]),
		static_cast<unsigned int>(buffer[kIpOffset + 2U]),
		static_cast<unsigned int>(buffer[kIpOffset + 1U]),
		static_cast<unsigned int>(buffer[kIpOffset + 0U]));
	if (ip_written <= 0 || ip_written >= static_cast<int>(sizeof(ip_text)))
	{
		return false;
	}

	const uint16_t port = static_cast<uint16_t>(buffer[kPortOffset])
		| static_cast<uint16_t>(static_cast<uint16_t>(buffer[kPortOffset + 1U]) << 8U);
	if (port == 0U)
	{
		return false;
	}

	std::string peer_ip(ip_text);
	unsigned int peer_port = static_cast<unsigned int>(port);
	const std::string original_peer_ip = peer_ip;
	const unsigned int original_peer_port = peer_port;
	const bool is_mapped = remap_dynamic_peer(&peer_ip, &peer_port);

	(void)connect_session_index;
	if (udp->set_default_peer(peer_ip.c_str(), static_cast<uint16_t>(peer_port)) != connection_result::ok)
	{
		return false;
	}

	if (is_mapped)
	{
		get_037_log(name())->debug("%s-0x%08X peer %s:%u (mapped from %s:%u)",
			name().c_str(),
			peripheral_number(),
			peer_ip.c_str(),
			peer_port,
			original_peer_ip.c_str(),
			original_peer_port);
	}
	else
	{
		get_037_log(name())->debug("%s-0x%08X peer %s:%u",
			name().c_str(),
			peripheral_number(),
			peer_ip.c_str(),
			peer_port);
	}
	return true;
}

bool safety037_session::remap_dynamic_peer(std::string* ip, unsigned int* port) const noexcept
{
	if (ip == nullptr || port == nullptr)
	{
		return false;
	}

	for (const peer_mapping& mapping : peer_mappings_)
	{
		if (mapping.from_ip == *ip && mapping.from_port == *port)
		{
			*ip = mapping.to_ip;
			*port = mapping.to_port;
			return true;
		}
	}

	return false;
}