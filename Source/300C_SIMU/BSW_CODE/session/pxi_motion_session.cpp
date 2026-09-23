#include "pxi_motion_session.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "sdmu_packet.h"
#include "../Pub_Comm/udp_connection.h"

namespace {

constexpr std::size_t kMotionPacketBufferSize = 1024U;
constexpr std::size_t kMinimumMotionPacketSize = 50U;

std::uint32_t load_u32_le(const unsigned char* src) noexcept
{
	return static_cast<std::uint32_t>(src[0])
		| (static_cast<std::uint32_t>(src[1]) << 8)
		| (static_cast<std::uint32_t>(src[2]) << 16)
		| (static_cast<std::uint32_t>(src[3]) << 24);
}

std::int32_t load_i32_le(const unsigned char* src) noexcept
{
	return static_cast<std::int32_t>(load_u32_le(src));
}

bool parse_motion_sample(const unsigned char* buffer, std::size_t size, asw_motion_sample* sample) noexcept
{
	if (buffer == nullptr || sample == nullptr || size < kMinimumMotionPacketSize)
	{
		return false;
	}

	sample->time_ms = load_u32_le(buffer + 4);

	sample->pulse_state_1 = buffer[11];
	sample->pulse_state_2 = buffer[12];
	sample->pulse_state_3 = buffer[13];
	sample->pulse_count_1 = load_i32_le(buffer + 14);
	sample->pulse_count_2 = load_i32_le(buffer + 18);
	sample->pulse_count_3 = load_i32_le(buffer + 22);

	sample->position_mm = load_i32_le(buffer + 36);
	sample->velocity_mm_s = load_i32_le(buffer + 40);
	sample->acceleration_mm_s2 = load_i32_le(buffer + 44);
	sample->motion = buffer[48];
	sample->direction = buffer[49];
	sample->motion = sample->velocity_mm_s > 0 ? 1U : 0U;

	return true;
}

}

pxi_motion_session::pxi_motion_session(const nlohmann::json& config)
	: session(config)
{
	if (!config.is_object())
	{
		return;
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

pxi_motion_session::~pxi_motion_session()
{
}

bool pxi_motion_session::try_get_train_pos_mm(std::int32_t* position_mm) const noexcept
{
	if (position_mm == nullptr || !has_last_position_)
	{
		return false;
	}

	*position_mm = last_position_;
	return true;
}

session_result pxi_motion_session::do_input() noexcept
{
	if (!connection_)
	{
		return session_result::invalid_argument;
	}

	std::array<unsigned char, kMotionPacketBufferSize> buffer = {};
	std::array<unsigned char, kMotionPacketBufferSize> latest_buffer = {};
	std::size_t received_size = 0U;
	std::size_t latest_received_size = 0U;
	bool received_any_packet = false;

	for (;;)
	{
		const connection_result result = connection_->receive(buffer.data(), buffer.size(), &received_size);
		if (result == connection_result::ok)
		{
			latest_buffer = buffer;
			latest_received_size = received_size;
			received_any_packet = true;
			continue;
		}

		if (result == connection_result::would_block)
		{
			break;
		}

		return session_map_receive_result(result);
	}

	if (!received_any_packet)
	{
		return session_result::would_block;
	}

	asw_motion_sample sample;
	if (!parse_motion_sample(latest_buffer.data(), latest_received_size, &sample))
	{
		return session_result::invalid_argument;
	}

	if (has_last_position_ && sample.position_mm == 0 && last_position_ != 0)
	{
		last_position_ = sample.position_mm;
		return session_result::ok;
	}

	last_position_ = sample.position_mm;
	has_last_position_ = true;

	unsigned char packet_buffer[256] = { 0U };
	std::uint16_t packet_length = 0U;

	assemble_secure_sdmu_packet(sample, packet_buffer, &packet_length);
	if (!write_asw_message(packet_buffer, packet_length))
	{
		return session_result::forward_failed;
	}

	assemble_accurate_sdmu_packet(sample, packet_buffer, &packet_length);
	if (!write_asw_message(packet_buffer, packet_length))
	{
		return session_result::forward_failed;
	}

	return session_result::ok;
}

session_result pxi_motion_session::do_output() noexcept
{
	return session_result::ok;
}
