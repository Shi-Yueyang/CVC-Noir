#include "pxi_session.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdlib>
#include <string>
#include <unordered_map>

#include "../../ASW_300C/Interface_Data.h"
#include "../../logging/logger.h"
#include "../bswTime.h"
#include "../Pub_Comm/udp_connection.h"

namespace {

constexpr std::uint8_t kPxiHeaderFirstByte = 0x55U;
constexpr std::uint8_t kPxiHeaderSecondByte = 0xAAU;
constexpr std::uint8_t kBtmHeaderSecondByte = 0xBBU;
constexpr std::uint8_t kPxiLowValue = 0x22U;
constexpr std::uint8_t kInternalTrueValue = 1U;
constexpr std::size_t kAlwaysHighByteIndex = 11U;
constexpr std::uint8_t kUninitializedInternalValue = 0xFFU;
constexpr std::size_t kBtmPacketLengthThreshold = 100U;
constexpr std::size_t kBtmTelegramLength = 119U;
constexpr std::size_t kBtmTelegramPayloadOffset = 7U;
constexpr std::size_t kBtmTelegramPayloadLength = 104U;
constexpr uint32_t kBtmAccuracyValue = 1000U;
constexpr uint16_t kBtmTelegramBitLength = 830U;
constexpr uint64_t kInvalidBtmTimestamp2 = 0xFFFFFFFFFFFFFFFFULL;

Logger::Ptr get_pxi_log()
{
	return Logger::get("pxi");
}

bool try_read_byte_value(const nlohmann::json& node, std::uint8_t* value) noexcept
{
	if (value == nullptr)
	{
		return false;
	}

	if (node.is_number_integer())
	{
		const int int_value = node.get<int>();
		if (int_value < 0 || int_value > 0xFF)
		{
			return false;
		}

		*value = static_cast<std::uint8_t>(int_value);
		return true;
	}

	if (!node.is_string())
	{
		return false;
	}

	const std::string text = node.get<std::string>();
	char* end_ptr = nullptr;
	const unsigned long parsed_value = std::strtoul(text.c_str(), &end_ptr, 0);
	if (end_ptr == text.c_str() || *end_ptr != '\0' || parsed_value > 0xFFUL)
	{
		return false;
	}

	*value = static_cast<std::uint8_t>(parsed_value);
	return true;
}

std::vector<pxi_io_mapping_entry> parse_mapping_entries(const nlohmann::json& node)
{
	std::vector<pxi_io_mapping_entry> entries;
	if (!node.is_object())
	{
		return entries;
	}

	if (!node.contains("entries") || !node["entries"].is_array())
	{
		return entries;
	}

	for (const nlohmann::json& entry_node : node["entries"])
	{
		if (!entry_node.is_object())
		{
			continue;
		}

		if (!entry_node.contains("from") || !entry_node["from"].is_number_integer())
		{
			continue;
		}

		if (!entry_node.contains("to") || !entry_node["to"].is_number_integer())
		{
			continue;
		}

		pxi_io_mapping_entry entry;
		entry.from = entry_node["from"].get<int>();
		entry.to = entry_node["to"].get<int>();

		if (entry_node.contains("or") && entry_node["or"].is_number_integer())
		{
			entry.has_or = true;
			entry.or_from = entry_node["or"].get<int>();
		}

		entries.push_back(entry);
	}

	return entries;
}

pxi_packet_io_mapping parse_packet_mapping(const nlohmann::json& node)
{
	pxi_packet_io_mapping mapping;
	if (!node.is_object())
	{
		return mapping;
	}

	if (node.contains("total_length") && node["total_length"].is_number_integer())
	{
		const int total_length = node["total_length"].get<int>();
		if (total_length >= 0)
		{
			mapping.total_length = static_cast<std::size_t>(total_length);
		}
	}

	if (node.contains("high_value"))
	{
		std::uint8_t high_value = 0U;
		if (try_read_byte_value(node["high_value"], &high_value))
		{
			mapping.high_value = high_value;
		}
	}

	mapping.entries = parse_mapping_entries(node);
	return mapping;
}

pxi_internal_io_mapping parse_internal_mapping(const nlohmann::json& node)
{
	pxi_internal_io_mapping mapping;
	mapping.entries = parse_mapping_entries(node);
	return mapping;
}

std::unordered_map<int, std::string> parse_port_name_map(const nlohmann::json& node)
{
	std::unordered_map<int, std::string> names;
	if (!node.is_object())
	{
		return names;
	}

	for (auto it = node.begin(); it != node.end(); ++it)
	{
		const std::string& key = it.key();
		if (!it.value().is_string())
		{
			get_pxi_log()->warn("pxi_session port_names value for key \"%s\" ignored: expected string", key.c_str());
			continue;
		}

		char* end_ptr = nullptr;
		const long port_index = std::strtol(key.c_str(), &end_ptr, 10);
		if (end_ptr == key.c_str() || *end_ptr != '\0' || port_index < 0)
		{
			get_pxi_log()->warn("pxi_session port_names key \"%s\" ignored: not a valid port index", key.c_str());
			continue;
		}

		names[static_cast<int>(port_index)] = it.value().get<std::string>();
	}

	return names;
}

pxi_packet_io_mapping parse_to_pxi_rule(const nlohmann::json& node)
{
	pxi_packet_io_mapping mapping = parse_packet_mapping(node);
	if (!node.is_object())
	{
		return mapping;
	}

	if (node.contains("length") && node["length"].is_number_integer())
	{
		const int length = node["length"].get<int>();
		if (length >= 0)
		{
			mapping.length = static_cast<std::size_t>(length);
		}
	}
	else
	{
		mapping.length = mapping.total_length;
	}

	return mapping;
}

std::vector<pxi_packet_io_mapping> parse_to_pxi_rules(const nlohmann::json& node, Logger::Ptr log)
{
	std::vector<pxi_packet_io_mapping> mappings;
	if (node.is_object())
	{
		mappings.push_back(parse_to_pxi_rule(node));
	}
	else if (node.is_array())
	{
		for (const nlohmann::json& rule_node : node)
		{
			if (!rule_node.is_object())
			{
				if (log)
				{
					log->warn("pxi_session to_pxi entry ignored: expected object");
				}
				continue;
			}

			mappings.push_back(parse_to_pxi_rule(rule_node));
		}
	}

	return mappings;
}

session_result process_btm_input_packet(const unsigned char* packet, std::size_t packet_size, std::int32_t train_pos_cm);

bool is_internal_port_active(const std::array<std::uint8_t, MAX_IOPORT_NUM>& io_record, int port_index) noexcept
{
	if (port_index < 0 || port_index >= static_cast<int>(io_record.size()))
	{
		return false;
	}

	return io_record[static_cast<std::size_t>(port_index)] == kInternalTrueValue;
}

bool is_internal_port_active(const std::vector<std::uint8_t>& io_record, int port_index) noexcept
{
	if (port_index < 0 || static_cast<std::size_t>(port_index) >= io_record.size())
	{
		return false;
	}

	return io_record[static_cast<std::size_t>(port_index)] == kInternalTrueValue;
}

session_result receive_input_packets(
	IConnection* connection,
	std::array<unsigned char, 2048>* latest_buffer,
	std::size_t* latest_received_size,
	bool* received_normal_packet,
	bool* processed_btm_packet,
	std::int32_t train_pos_cm)
{
	if (connection == nullptr
		|| latest_buffer == nullptr
		|| latest_received_size == nullptr
		|| received_normal_packet == nullptr
		|| processed_btm_packet == nullptr)
	{
		return session_result::invalid_argument;
	}

	std::array<unsigned char, 2048> buffer = {};
	std::size_t received_size = 0U;
	*latest_received_size = 0U;
	*received_normal_packet = false;
	*processed_btm_packet = false;

	for (;;)
	{
		const connection_result result = connection->receive(buffer.data(), buffer.size(), &received_size);
		if (result == connection_result::ok)
		{
			if (received_size > kBtmPacketLengthThreshold)
			{
				const session_result btm_result = process_btm_input_packet(buffer.data(), received_size, train_pos_cm);
				if (btm_result != session_result::ok)
				{
					return btm_result;
				}

				*processed_btm_packet = true;
				continue;
			}

			*latest_buffer = buffer;
			*latest_received_size = received_size;
			*received_normal_packet = true;
			continue;
		}

		if (result == connection_result::would_block)
		{
			return session_result::ok;
		}

		return session_map_receive_result(result);
	}
}

session_result process_btm_input_packet(const unsigned char* packet, std::size_t packet_size, std::int32_t train_pos_cm)
{
	if (packet == nullptr || packet_size <= kBtmPacketLengthThreshold)
	{
		return session_result::invalid_argument;
	}

	for (std::size_t telegram_index = 0U; telegram_index < packet_size; telegram_index += kBtmTelegramLength)
	{

		if (packet[telegram_index] != kPxiHeaderFirstByte || packet[telegram_index + 1U] != kBtmHeaderSecondByte)
		{
			return session_result::receive_failed;
		}

		APP_BTMData_t btm_data = { 0 };
		btm_data.Timestamp1 = getcurrRunTimeFromAdapter(packet);
		btm_data.Timestamp2 = kInvalidBtmTimestamp2;
		btm_data.Location = static_cast<uint32_t>(train_pos_cm >= 0 ? train_pos_cm : 0);
		btm_data.Accuracy = kBtmAccuracyValue;
		btm_data.TelgDataLen = kBtmTelegramBitLength;
		std::memcpy(
			btm_data.TelgData,
			packet + telegram_index + kBtmTelegramPayloadOffset,
			kBtmTelegramPayloadLength);

		get_pxi_log()->info("btm loc=%u ts=%llu len=%zu",
			static_cast<unsigned int>(btm_data.Location),
			static_cast<unsigned long long>(btm_data.Timestamp1),
			packet_size);

		if (CVC_FALSE == CVC_BSW_ITF_Write(CVC_BSW_BTM_RX_TYPE, reinterpret_cast<uint8_t*>(&btm_data), sizeof(btm_data)))
		{
			return session_result::forward_failed;
		}
	}

	return session_result::ok;
}

void append_pxi_packet_input(
	IOData_t* data_to_asw,
	std::size_t* outdata_index,
	const unsigned char* packet,
	std::size_t packet_size,
	const pxi_packet_io_mapping& mapping) noexcept
{
	if (data_to_asw == nullptr || outdata_index == nullptr || packet == nullptr)
	{
		return;
	}

	for (const pxi_io_mapping_entry& entry : mapping.entries)
	{
		if (entry.from < 0 || static_cast<std::size_t>(entry.from) >= packet_size)
		{
			continue;
		}

		if (entry.to < 0 || outdata_index[0] >= MAX_IOPORT_NUM)
		{
			continue;
		}

		data_to_asw->IOPortData[*outdata_index].PortIndex = static_cast<uint16_t>(entry.to);
		data_to_asw->IOPortData[*outdata_index].PortValue =
			packet[static_cast<std::size_t>(entry.from)] == mapping.high_value ? 1U : 0U;
		++(*outdata_index);
	}
}

void append_internal_input(
	IOData_t* data_to_asw,
	std::size_t* outdata_index,
	const std::vector<std::uint8_t>& internal_io_record,
	const pxi_internal_io_mapping& mapping) noexcept
{
	if (data_to_asw == nullptr || outdata_index == nullptr)
	{
		return;
	}

	for (const pxi_io_mapping_entry& entry : mapping.entries)
	{
		if (entry.to < 0 || *outdata_index >= MAX_IOPORT_NUM)
		{
			continue;
		}

		const bool is_active = is_internal_port_active(internal_io_record, entry.from)
			|| (entry.has_or && is_internal_port_active(internal_io_record, entry.or_from));
		data_to_asw->IOPortData[*outdata_index].PortIndex = static_cast<uint16_t>(entry.to);
		data_to_asw->IOPortData[*outdata_index].PortValue = is_active ? 1U : 0U;
		++(*outdata_index);
	}
}

bool has_any_from_pxi_entries(const std::vector<pxi_packet_io_mapping>& mappings) noexcept
{
	for (const pxi_packet_io_mapping& mapping : mappings)
	{
		if (!mapping.entries.empty())
		{
			return true;
		}
	}

	return false;
}

bool should_publish_vib_input(
	bool received_any_packet,
	const std::vector<pxi_packet_io_mapping>& from_pxi_mappings,
	const pxi_internal_io_mapping& from_internal_mapping) noexcept
{
	if (received_any_packet)
	{
		return true;
	}

	return !from_internal_mapping.entries.empty() || has_any_from_pxi_entries(from_pxi_mappings);
}

const pxi_packet_io_mapping* find_from_pxi_mapping(
	const std::vector<pxi_packet_io_mapping>& mappings,
	std::size_t packet_size) noexcept
{
	for (const pxi_packet_io_mapping& mapping : mappings)
	{
		if (mapping.total_length == packet_size)
		{
			return &mapping;
		}
	}

	return nullptr;
}

session_result validate_pxi_input_packet(const unsigned char* packet, std::size_t packet_size, const pxi_packet_io_mapping& mapping) noexcept
{
	if (packet == nullptr)
	{
		get_pxi_log()->warn("pxi_session validate_pxi_input_packet failed: null packet");
		return session_result::invalid_argument;
	}

	if (mapping.total_length == 0U)
	{
		get_pxi_log()->warn("pxi_session validate_pxi_input_packet failed: total_length is not configured");
		return session_result::invalid_argument;
	}

	if (packet_size != mapping.total_length)
	{
		get_pxi_log()->warn("pxi_session validate_pxi_input_packet failed: packet_size=%zu expected=%zu",
			packet_size,
			mapping.total_length);
		return session_result::invalid_argument;
	}

	if (packet_size < 2U || packet[0] != kPxiHeaderFirstByte || packet[1] != kPxiHeaderSecondByte)
	{
		get_pxi_log()->warn("pxi_session validate_pxi_input_packet failed: invalid header size=%zu first=0x%02X second=0x%02X",
			packet_size,
			packet_size > 0U ? static_cast<unsigned int>(packet[0]) : 0U,
			packet_size > 1U ? static_cast<unsigned int>(packet[1]) : 0U);
		return session_result::receive_failed;
	}

	return session_result::ok;
}

session_result resolve_input_receive_state(
	bool received_normal_packet,
	bool processed_btm_packet,
	const std::vector<pxi_packet_io_mapping>& from_pxi_mappings,
	const pxi_internal_io_mapping& from_internal_mapping,
	bool* should_publish_vib) noexcept
{
	if (should_publish_vib == nullptr)
	{
		return session_result::invalid_argument;
	}

	*should_publish_vib = true;
	if (received_normal_packet)
	{
		return session_result::ok;
	}

	if (should_publish_vib_input(false, from_pxi_mappings, from_internal_mapping))
	{
		return processed_btm_packet ? session_result::ok : session_result::ok;
	}

	*should_publish_vib = false;
	return processed_btm_packet ? session_result::ok : session_result::would_block;
}

std::string format_port_group(
	const std::vector<int>& ports,
	const std::unordered_map<int, std::string>& port_names,
	bool trace_unnamed) noexcept
{
	std::string result;
	bool first = true;
	for (std::size_t i = 0U; i < ports.size(); ++i)
	{
		const int port_index = ports[i];
		const auto name_it = port_names.find(port_index);
		const bool has_name = (name_it != port_names.end());
		if (!has_name && !trace_unnamed)
		{
			continue;
		}

		if (!first)
		{
			result += ',';
		}
		first = false;

		if (has_name)
		{
			result += name_it->second;
		}
		else
		{
			result += std::to_string(port_index);
		}
	}
	return result;
}

std::string format_io_ports(
	const IOData_t& io_data,
	const std::unordered_map<int, std::string>& port_names,
	bool trace_unnamed) noexcept
{
	std::vector<int> high_ports;
	std::vector<int> low_ports;

	const std::size_t count =
		(static_cast<std::size_t>(io_data.Length) <= MAX_IOPORT_NUM)
			? static_cast<std::size_t>(io_data.Length)
			: MAX_IOPORT_NUM;

	for (std::size_t i = 0U; i < count; ++i)
	{
		const int port_index = static_cast<int>(io_data.IOPortData[i].PortIndex);
		if (io_data.IOPortData[i].PortValue == 1U)
		{
			high_ports.push_back(port_index);
		}
		else
		{
			low_ports.push_back(port_index);
		}
	}

	std::sort(high_ports.begin(), high_ports.end());
	std::sort(low_ports.begin(), low_ports.end());

	std::string result = "HIGH[";
	result += format_port_group(high_ports, port_names, trace_unnamed);
	result += "] LOW[";
	result += format_port_group(low_ports, port_names, trace_unnamed);
	result += ']';
	return result;
}

session_result publish_vib_input(
	bool received_normal_packet,
	const std::array<unsigned char, 2048>& latest_buffer,
	std::size_t latest_received_size,
	const std::vector<std::uint8_t>& internal_io_record,
	const pxi_packet_io_mapping& from_pxi_mapping,
	const pxi_internal_io_mapping& from_internal_mapping,
	const std::unordered_map<int, std::string>& port_names_in,
	bool trace_unnamed) noexcept
{
	IOData_t data_to_asw = { 0 };
	std::size_t outdata_index = 0U;

	if (received_normal_packet)
	{
		append_pxi_packet_input(
			&data_to_asw,
			&outdata_index,
			latest_buffer.data(),
			latest_received_size,
			from_pxi_mapping);
	}

	append_internal_input(
		&data_to_asw,
		&outdata_index,
		internal_io_record,
		from_internal_mapping);

	data_to_asw.Length = static_cast<uint16_t>(outdata_index);
	get_pxi_log()->trace("in: %s", format_io_ports(data_to_asw, port_names_in, trace_unnamed).c_str());
	if (CVC_FALSE == CVC_BSW_ITF_Write(CVC_BSW_VIB_RX_TYPE, reinterpret_cast<uint8_t*>(&data_to_asw), sizeof(data_to_asw)))
	{
		return session_result::forward_failed;
	}

	return session_result::ok;
}

const pxi_packet_io_mapping* find_to_pxi_mapping(
	const std::vector<pxi_packet_io_mapping>& mappings,
	std::size_t vob_length) noexcept
{
	for (const pxi_packet_io_mapping& mapping : mappings)
	{
		if (mapping.length == vob_length)
		{
			return &mapping;
		}
	}

	return nullptr;
}

} // namespace

pxi_session::pxi_session(const nlohmann::json& config)
	: session(config)
{
	internal_io_record_.assign(MAX_IOPORT_NUM, kUninitializedInternalValue);

	if (!config.is_object())
	{
		return;
	}

	const nlohmann::json& io_mapping_node =
		(config.contains("io_mapping") && config["io_mapping"].is_object()) ? config["io_mapping"] : nlohmann::json();

	if (io_mapping_node.is_object())
	{
		to_pxi_mappings_ = parse_to_pxi_rules(
			io_mapping_node.contains("to_pxi") ? io_mapping_node["to_pxi"] : nlohmann::json(),
			get_pxi_log());
		from_internal_mapping_ = parse_internal_mapping(
			io_mapping_node.contains("from_internal") ? io_mapping_node["from_internal"] : nlohmann::json());

		if (io_mapping_node.contains("from_pxi"))
		{
			const nlohmann::json& from_pxi_node = io_mapping_node["from_pxi"];
			if (from_pxi_node.is_array())
			{
				for (const nlohmann::json& rule_node : from_pxi_node)
				{
					if (!rule_node.is_object())
					{
						get_pxi_log()->warn("pxi_session from_pxi entry ignored: expected object");
						continue;
					}

					from_pxi_mappings_.push_back(parse_packet_mapping(rule_node));
				}
			}
			else
			{
				get_pxi_log()->error("pxi_session from_pxi must be an array of mapping rules");
			}
		}
	}

	if (config.contains("port_names") && config["port_names"].is_object())
	{
		const nlohmann::json& port_names_node = config["port_names"];
		if (port_names_node.contains("input"))
		{
			port_names_in_ = parse_port_name_map(port_names_node["input"]);
		}
		if (port_names_node.contains("output"))
		{
			port_names_out_ = parse_port_name_map(port_names_node["output"]);
		}
	}

	if (config.contains("trace_unnamed_ports") && config["trace_unnamed_ports"].is_boolean())
	{
		trace_unnamed_ports_ = config["trace_unnamed_ports"].get<bool>();
	}

	const nlohmann::json& local_ip_node = config.contains("local_ip") ? config["local_ip"] : nlohmann::json();
	const nlohmann::json& local_port_node = config.contains("local_port") ? config["local_port"] : nlohmann::json();
	const nlohmann::json& peer_ip_node = config.contains("peer_ip") ? config["peer_ip"] : nlohmann::json();
	const nlohmann::json& peer_port_node = config.contains("peer_port") ? config["peer_port"] : nlohmann::json();

	if (!local_port_node.is_number_integer())
	{
		return;
	}

	const std::string local_ip = local_ip_node.is_string() ? local_ip_node.get<std::string>() : std::string();
	const uint16_t local_port = static_cast<uint16_t>(local_port_node.get<int>());
	const std::string peer_ip = peer_ip_node.is_string() ? peer_ip_node.get<std::string>() : std::string();
	const uint16_t peer_port = peer_port_node.is_number_integer() ? static_cast<uint16_t>(peer_port_node.get<int>()) : 0U;

	connection_.reset(new udp_connection(
		local_ip.empty() ? nullptr : local_ip.c_str(),
		local_port,
		peer_ip.empty() ? nullptr : peer_ip.c_str(),
		peer_port));
}

pxi_session::~pxi_session()
{
}

session_result pxi_session::do_input() noexcept
{
	if (!connection_)
	{
		return session_result::invalid_argument;
	}

	// Drain the UDP socket: normal PXI packets are kept as the latest frame,
	// BTM telegrams are parsed and forwarded immediately.
	std::array<unsigned char, 2048> latest_buffer = {};
	std::size_t latest_received_size = 0U;
	bool received_normal_packet = false;
	bool processed_btm_packet = false;

	const session_result receive_result = receive_input_packets(
		connection_.get(),
		&latest_buffer,
		&latest_received_size,
		&received_normal_packet,
		&processed_btm_packet,
		train_pos_cm_);
	if (receive_result != session_result::ok)
	{
		return receive_result;
	}

	// Decide whether a VIB input should be published this cycle. When no
	// packet arrived and no internal/from_pxi mapping exists, there is nothing
	// to publish and the cycle is treated as idle.
	bool should_publish = true;
	const session_result receive_state_result = resolve_input_receive_state(
		received_normal_packet,
		processed_btm_packet,
		from_pxi_mappings_,
		from_internal_mapping_,
		&should_publish);
	if (receive_state_result != session_result::ok)
	{
		return receive_state_result;
	}

	if (!should_publish)
	{
		return session_result::ok;
	}

	// Select the from_pxi mapping rule whose total_length matches the received
	// packet size, then validate the header and size of that packet.
	const pxi_packet_io_mapping* selected_from_pxi_mapping = nullptr;
	pxi_packet_io_mapping empty_from_pxi_mapping;
	if (received_normal_packet)
	{
		selected_from_pxi_mapping = find_from_pxi_mapping(from_pxi_mappings_, latest_received_size);
		if (selected_from_pxi_mapping == nullptr)
		{
			get_pxi_log()->warn("pxi_session no from_pxi mapping for packet size %zu", latest_received_size);
			selected_from_pxi_mapping = &empty_from_pxi_mapping;
		}
		else
		{
			const session_result validation_result = validate_pxi_input_packet(
				latest_buffer.data(),
				latest_received_size,
				*selected_from_pxi_mapping);
			if (validation_result != session_result::ok)
			{
				return validation_result;
			}
		}
	}

	// Map the packet bits and internal signals into IOData_t port pairs and
	// forward them to ASW as a VIB input (with a trace of the resulting state).
	return publish_vib_input(
		received_normal_packet,
		latest_buffer,
		latest_received_size,
		internal_io_record_,
		selected_from_pxi_mapping != nullptr ? *selected_from_pxi_mapping : empty_from_pxi_mapping,
		from_internal_mapping_,
		port_names_in_,
		trace_unnamed_ports_);
}

session_result pxi_session::do_output() noexcept
{
	if (!connection_)
	{
		return session_result::invalid_argument;
	}

	if (to_pxi_mappings_.empty())
	{
		return session_result::invalid_argument;
	}

	bool sent_any_packet = false;
	IOData_t vob_data = { 0 };
	while (CVC_BUFFER_OPER_SUCCESS == CVC_BSW_ITF_Read(CVC_BSW_VOB_TX_TYPE, reinterpret_cast<uint8_t*>(&vob_data)))
	{
		if (vob_data.Length > MAX_IOPORT_NUM)
		{
			return session_result::invalid_argument;
		}

		const pxi_packet_io_mapping* selected_to_pxi_mapping = find_to_pxi_mapping(to_pxi_mappings_, vob_data.Length);
		if (selected_to_pxi_mapping == nullptr)
		{
			get_pxi_log()->warn("pxi_session no to_pxi mapping for vob_data.Length %hu", vob_data.Length);
			vob_data = { 0 };
			continue;
		}

		if (selected_to_pxi_mapping->total_length == 0U)
		{
			get_pxi_log()->warn("pxi_session to_pxi mapping for vob_data.Length %hu has no total_length", vob_data.Length);
			vob_data = { 0 };
			continue;
		}

		get_pxi_log()->trace("out: %s", format_io_ports(vob_data, port_names_out_, trace_unnamed_ports_).c_str());

		std::array<std::uint8_t, MAX_IOPORT_NUM> io_record;
		io_record.fill(0xFFU);

		for (std::size_t i = 0; i < vob_data.Length; ++i)
		{
			const std::size_t port_index = vob_data.IOPortData[i].PortIndex;
			if (port_index >= io_record.size())
			{
				continue;
			}

			io_record[port_index] = static_cast<std::uint8_t>(vob_data.IOPortData[i].PortValue);
		}

		internal_io_record_.assign(io_record.begin(), io_record.end());

		std::vector<std::uint8_t> packet(selected_to_pxi_mapping->total_length, 0U);
		if (packet.size() > 0U)
		{
			packet[0] = kPxiHeaderFirstByte;
		}
		if (packet.size() > 1U)
		{
			packet[1] = kPxiHeaderSecondByte;
		}
		if (packet.size() > kAlwaysHighByteIndex)
		{
			packet[kAlwaysHighByteIndex] = selected_to_pxi_mapping->high_value;
		}

		for (const pxi_io_mapping_entry& entry : selected_to_pxi_mapping->entries)
		{
			if (entry.to < 0 || static_cast<std::size_t>(entry.to) >= packet.size())
			{
				continue;
			}

			const bool is_active = is_internal_port_active(io_record, entry.from)
				|| (entry.has_or && is_internal_port_active(io_record, entry.or_from));
			packet[static_cast<std::size_t>(entry.to)] = is_active ? selected_to_pxi_mapping->high_value : kPxiLowValue;
		}

		const connection_result send_result = connection_->send(packet.data(), packet.size());
		if (send_result != connection_result::ok)
		{
			return session_map_send_result(send_result);
		}

		sent_any_packet = true;
		vob_data = { 0 };
	}

	if (!sent_any_packet)
	{
		return session_result::would_block;
	}

	return session_result::ok;
}

