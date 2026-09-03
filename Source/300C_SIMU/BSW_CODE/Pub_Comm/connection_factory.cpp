#include "connection_factory.h"

#include <cstdint>
#include <string>
#include <vector>

#include "dummy_connection.h"
#include "serial_connection.h"
#include "tcp_connection_client.h"
#include "tcp_connection_server.h"
#include "udp_connection.h"

namespace {

uint32_t parse_u32_value(const nlohmann::json& value, uint32_t default_value = 0U)
{
	if (value.is_number_unsigned())
	{
		return value.get<uint32_t>();
	}

	if (value.is_number_integer())
	{
		const int parsed = value.get<int>();
		return parsed >= 0 ? static_cast<uint32_t>(parsed) : default_value;
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
		const uint32_t value = parse_u32_value(item, 256U);
		if (value <= 0xFFU)
		{
			result.push_back(static_cast<std::uint8_t>(value));
		}
	}

	return result;
}

tcp_framing_method parse_tcp_framing_method(const nlohmann::json& config, const char* key)
{
	if (!config.contains(key) || !config[key].is_string())
	{
		return tcp_framing_method::none;
	}

	const std::string framing = config[key].get<std::string>();
	if (framing == "dmi")
	{
		return tcp_framing_method::dmi;
	}
	if (framing == "2-byte-len-big" || framing == "two_byte_len_big")
	{
		return tcp_framing_method::two_byte_len_big;
	}
	if (framing == "2-byte-len-little" || framing == "two_byte_len_little")
	{
		return tcp_framing_method::two_byte_len_little;
	}
	if (framing == "ndjson")
	{
		return tcp_framing_method::ndjson;
	}

	return tcp_framing_method::none;
}

}

std::unique_ptr<IConnection> connection_factory::create(const nlohmann::json& config)
{
	if (!config.is_object() || !config.contains("type") || !config["type"].is_string())
	{
		return std::unique_ptr<IConnection>();
	}

	const std::string type = config["type"].get<std::string>();
	if (type == "udp")
	{
		const nlohmann::json& local_ip_node = config.contains("local_ip") ? config["local_ip"] : nlohmann::json();
		const nlohmann::json& local_port_node = config.contains("local_port") ? config["local_port"] : nlohmann::json();
		const nlohmann::json& default_peer_ip_node = config.contains("default_peer_ip") ? config["default_peer_ip"] : nlohmann::json();
		const nlohmann::json& default_peer_port_node = config.contains("default_peer_port") ? config["default_peer_port"] : nlohmann::json();

		if (local_port_node.is_null() || !local_port_node.is_number_integer())
		{
			return std::unique_ptr<IConnection>();
		}

		const std::string local_ip = local_ip_node.is_string()
			? local_ip_node.get<std::string>()
			: std::string();
		const uint16_t local_port = static_cast<uint16_t>(local_port_node.get<int>());
		const std::string default_peer_ip = default_peer_ip_node.is_string()
			? default_peer_ip_node.get<std::string>()
			: std::string();
		const uint16_t default_peer_port = default_peer_port_node.is_number_integer()
			? static_cast<uint16_t>(default_peer_port_node.get<int>())
			: 0U;

		return std::unique_ptr<IConnection>(
			new udp_connection(
				local_ip.empty() ? nullptr : local_ip.c_str(),
				local_port,
				default_peer_ip.empty() ? nullptr : default_peer_ip.c_str(),
				default_peer_port));
	}
	if (type == "tcp" || type == "tcp_server")
	{
		const nlohmann::json& local_ip_node = config.contains("local_ip") ? config["local_ip"] : nlohmann::json();
		const nlohmann::json& local_port_node = config.contains("local_port") ? config["local_port"] : nlohmann::json();

		if (!local_port_node.is_number_integer())
		{
			return std::unique_ptr<IConnection>();
		}

		const std::string local_ip = local_ip_node.is_string()
			? local_ip_node.get<std::string>()
			: std::string();
		const uint16_t local_port = static_cast<uint16_t>(local_port_node.get<int>());
		const tcp_framing_method recv_framing = parse_tcp_framing_method(config, "receive_framing");
			const tcp_framing_method send_framing = parse_tcp_framing_method(config, "send_framing");

		return std::unique_ptr<IConnection>(
			new tcp_connection_server(
				local_ip.empty() ? nullptr : local_ip.c_str(),
				local_port,
				recv_framing, send_framing));
	}
	if (type == "tcp_client")
	{
		const nlohmann::json& remote_ip_node = config.contains("remote_ip") ? config["remote_ip"] : nlohmann::json();
		const nlohmann::json& remote_port_node = config.contains("remote_port") ? config["remote_port"] : nlohmann::json();
		const nlohmann::json& local_ip_node = config.contains("local_ip") ? config["local_ip"] : nlohmann::json();
		const nlohmann::json& local_port_node = config.contains("local_port") ? config["local_port"] : nlohmann::json();

		if (!remote_ip_node.is_string() || !remote_port_node.is_number_integer())
		{
			return std::unique_ptr<IConnection>();
		}

		const std::string remote_ip = remote_ip_node.get<std::string>();
		const uint16_t remote_port = static_cast<uint16_t>(remote_port_node.get<int>());
		const std::string local_ip = local_ip_node.is_string()
			? local_ip_node.get<std::string>()
			: std::string();
		const uint16_t local_port = local_port_node.is_number_integer()
			? static_cast<uint16_t>(local_port_node.get<int>())
			: 0U;
		const tcp_framing_method recv_framing = parse_tcp_framing_method(config, "receive_framing");
			const tcp_framing_method send_framing = parse_tcp_framing_method(config, "send_framing");

		return std::unique_ptr<IConnection>(
			new tcp_connection_client(
				remote_ip.empty() ? nullptr : remote_ip.c_str(),
				remote_port,
				local_ip.empty() ? nullptr : local_ip.c_str(),
				local_port,
				recv_framing, send_framing));
	}
	if (type == "serial")
	{
		const nlohmann::json& port_node = config.contains("port") ? config["port"] : nlohmann::json();
		if (!port_node.is_string())
		{
			return std::unique_ptr<IConnection>();
		}

		const std::string port = port_node.get<std::string>();
		const uint32_t baud_rate = parse_u32_value(config.contains("baud_rate") ? config["baud_rate"] : nlohmann::json(), 115200U);
		const uint8_t data_bits = static_cast<uint8_t>(parse_u32_value(config.contains("data_bits") ? config["data_bits"] : nlohmann::json(), 8U));
		const uint8_t stop_bits = static_cast<uint8_t>(parse_u32_value(config.contains("stop_bits") ? config["stop_bits"] : nlohmann::json(), 1U));
		const uint8_t parity = static_cast<uint8_t>(parse_u32_value(config.contains("parity") ? config["parity"] : nlohmann::json(), 0U));
		const uint8_t rts_cts = static_cast<uint8_t>(parse_u32_value(config.contains("rts_cts") ? config["rts_cts"] : nlohmann::json(), 0U));
		const uint32_t read_total_timeout_ms = parse_u32_value(config.contains("read_total_timeout_ms") ? config["read_total_timeout_ms"] : nlohmann::json(), 10U);

		return std::unique_ptr<IConnection>(
			new serial_connection(
				port.c_str(),
				baud_rate,
				data_bits,
				stop_bits,
				parity,
				rts_cts,
				read_total_timeout_ms));
	}
	if (type == "dummy")
	{
		return std::unique_ptr<IConnection>(
			new dummy_connection(parse_byte_array(config, "received_data")));
	}

	// Reserve for other connection types.
	return std::unique_ptr<IConnection>();
}