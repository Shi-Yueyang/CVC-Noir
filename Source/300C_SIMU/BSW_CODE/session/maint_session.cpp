#include "maint_session.h"

#include <array>
#include <cstdint>

#include "../Pub_Comm/connection_factory.h"
#include "../../ASW_300C/interface_p2a.h"
#include "../../ASW_300C/Interface_Data.h"

namespace {

constexpr unsigned char kMaintMessageStart = 0x7EU;
constexpr unsigned char kMaintMessageEnd = 0x7FU;
constexpr unsigned char kMaintEscape = 0x7DU;
constexpr unsigned char kMaintEscapedStart = 0x5EU;
constexpr unsigned char kMaintEscapedEscape = 0x5DU;
constexpr std::size_t kMaintLengthFieldHighOffset = 2U;
constexpr std::size_t kMaintLengthFieldLowOffset = 3U;
constexpr std::size_t kMaintDataOffset = 4U;
constexpr std::size_t kMaintCrcSize = 2U;
constexpr std::size_t kMaintMinimumFrameSize = 1U + 1U + 2U + 2U + 1U;

session_result send_post_processed_message(IConnection* connection, const unsigned char* buffer, std::size_t len) noexcept
{
	if (connection == nullptr || buffer == nullptr || len == 0U)
	{
		return session_result::invalid_argument;
	}

	std::array<unsigned char, (MAX_MANT_MSG_SIZE * 2U) + 2U> framed_buffer = {};
	std::size_t framed_size = 1U;
	framed_buffer[0] = kMaintMessageStart;

	for (std::size_t index = 0; index < len; ++index)
	{
		const unsigned char value = buffer[index];
		if (value == kMaintMessageStart)
		{
			framed_buffer[framed_size++] = kMaintEscape;
			framed_buffer[framed_size++] = kMaintEscapedStart;
		}
		else if (value == kMaintEscape)
		{
			framed_buffer[framed_size++] = kMaintEscape;
			framed_buffer[framed_size++] = kMaintEscapedEscape;
		}
		else
		{
			framed_buffer[framed_size++] = value;
		}
	}

	framed_buffer[framed_size++] = kMaintMessageStart;
	return session_map_send_result(connection->send(framed_buffer.data(), framed_size));
}

}

maint_session::maint_session(const nlohmann::json& config)
	: session(config)
{
	if (config.is_object() && config.contains("connection") && config["connection"].is_object())
	{
		connection_ = connection_factory::create(config["connection"]);
	}
}

maint_session::~maint_session()
{
}

session_result maint_session::do_input() noexcept
{
	return session_result::ok;
}

session_result maint_session::do_output() noexcept
{
	return session_result::ok;
}

session_result maint_session::output(const unsigned char* buffer, std::size_t len) noexcept
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

	if (len > static_cast<std::size_t>(MAX_MANT_MSG_SIZE))
	{
		return session_result::invalid_argument;
	}

	for (std::size_t index = 0U; index < len; )
	{
		if (buffer[index] == kMaintMessageStart)
		{
			if ((len - index) < kMaintMinimumFrameSize)
			{
				return session_result::invalid_argument;
			}

			const std::size_t length_high_index = index + kMaintLengthFieldHighOffset;
			const std::size_t length_low_index = index + kMaintLengthFieldLowOffset;
			const std::uint16_t data_length =
				(static_cast<std::uint16_t>(buffer[length_high_index]) << 8)
				| static_cast<std::uint16_t>(buffer[length_low_index])-2;

			const std::size_t data_start = index + kMaintDataOffset;
			const std::size_t frame_end_index = data_start + static_cast<std::size_t>(data_length) + kMaintCrcSize;
			if (frame_end_index >= len)
			{
				return session_result::invalid_argument;
			}

			if (buffer[frame_end_index] != kMaintMessageEnd)
			{
				return session_result::invalid_argument;
			}

			if (data_length > 0U)
			{
				const session_result send_result = send_post_processed_message(connection_.get(), buffer + data_start, static_cast<std::size_t>(data_length));
				if (send_result != session_result::ok)
				{
					return send_result;
				}
			}

			index = frame_end_index + 1U;
			continue;
		}

		++index;
	}

	return session_result::ok;
}