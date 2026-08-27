#ifndef TCP_FRAMING_INCLUDE
#define TCP_FRAMING_INCLUDE

#include <cstddef>
#include <cstring>
#include <vector>

enum class tcp_framing_method
{
	none = 0,
	dmi,
	two_byte_len_big,
	two_byte_len_little
};

/* ── Framing constants ── */

constexpr unsigned char kDmiMagicFirstByte   = 0x55U;
constexpr unsigned char kDmiMagicSecondByte  = 0xAAU;
constexpr std::size_t   kDmiLengthFieldOffset      = 3U;
constexpr std::size_t   kDmiMinimumFrameLength     = 6U;
constexpr std::size_t   kTwoByteLenHeaderSize      = 2U;
constexpr std::size_t   kTcpFramingReceiveChunkSize = 4096U;

/* ── Shared buffer-only framing parsers ── */

inline bool tcp_framing_has_dmi_magic(const std::vector<unsigned char>& buffer, std::size_t index) noexcept
{
	return index + 1U < buffer.size()
		&& buffer[index] == kDmiMagicFirstByte
		&& buffer[index + 1U] == kDmiMagicSecondByte;
}

inline bool tcp_framing_try_extract_dmi_frame(
	std::vector<unsigned char>& receive_buffer,
	void* buffer,
	std::size_t capacity,
	std::size_t* received_size) noexcept
{
	for (;;)
	{
		std::size_t search_index = 0U;

		while (search_index < receive_buffer.size())
		{
			if (tcp_framing_has_dmi_magic(receive_buffer, search_index))
			{
				break;
			}
			++search_index;
		}

		if (search_index > 0U)
		{
			receive_buffer.erase(
				receive_buffer.begin(),
				receive_buffer.begin() + static_cast<std::ptrdiff_t>(search_index));
		}

		if (receive_buffer.size() < kDmiLengthFieldOffset + 1U)
		{
			return false;
		}

		const std::size_t frame_length = receive_buffer[kDmiLengthFieldOffset];
		if (frame_length < kDmiMinimumFrameLength)
		{
			receive_buffer.erase(receive_buffer.begin(), receive_buffer.begin() + static_cast<std::ptrdiff_t>(kTwoByteLenHeaderSize));
			continue;
		}

		if (receive_buffer.size() < frame_length)
		{
			return false;
		}

		if (frame_length > capacity)
		{
			*received_size = frame_length;
			return false;
		}

		std::memcpy(buffer, receive_buffer.data(), frame_length);
		*received_size = frame_length;
		receive_buffer.erase(
			receive_buffer.begin(),
			receive_buffer.begin() + static_cast<std::ptrdiff_t>(frame_length));
		return true;
	}
}

inline bool tcp_framing_try_extract_two_byte_len_frame(
	std::vector<unsigned char>& receive_buffer,
	void* buffer,
	std::size_t capacity,
	std::size_t* received_size,
	bool big_endian) noexcept
{
	for (;;)
	{
		if (receive_buffer.size() < kTwoByteLenHeaderSize)
		{
			return false;
		}

		const std::size_t length = big_endian
			? (static_cast<std::size_t>(receive_buffer[0]) << 8)
				| static_cast<std::size_t>(receive_buffer[1])
			: (static_cast<std::size_t>(receive_buffer[1]) << 8)
				| static_cast<std::size_t>(receive_buffer[0]);

		if (length == 0U)
		{
			receive_buffer.erase(receive_buffer.begin(), receive_buffer.begin() + static_cast<std::ptrdiff_t>(kTwoByteLenHeaderSize));
			continue;
		}
		if (receive_buffer.size() < kTwoByteLenHeaderSize + length)
		{
			return false;
		}

		if (length > capacity)
		{
			*received_size = length;
			return false;
		}

		std::memcpy(buffer, receive_buffer.data() + kTwoByteLenHeaderSize, length);
		*received_size = length;
		receive_buffer.erase(
			receive_buffer.begin(),
			receive_buffer.begin() + static_cast<std::ptrdiff_t>(kTwoByteLenHeaderSize + length));
		return true;
	}
}

/* ── Send-framing helpers ── */

constexpr std::size_t kDmiSendHeaderSize     = 6U;
constexpr std::size_t kDmiSendMaxPayloadSize = 249U;  /* max frame length is 255 (single byte) */
constexpr std::size_t kTcpFramingSendBufferSize = 65536U;

inline std::size_t tcp_framing_build_dmi_send_frame(
	const void* payload,
	std::size_t size,
	unsigned char* output,
	std::size_t output_capacity) noexcept
{
	if (size > kDmiSendMaxPayloadSize)
	{
		return 0U;
	}

	const std::size_t total_length = kDmiSendHeaderSize + size;
	if (total_length > output_capacity)
	{
		return 0U;
	}

	output[0] = kDmiMagicFirstByte;           /* 0x55 */
	output[1] = kDmiMagicSecondByte;          /* 0xAA */
	output[2] = 0x00U;                        /* reserved */
	output[3] = static_cast<unsigned char>(total_length);
	output[4] = 0x00U;                        /* reserved */
	output[5] = 0x00U;                        /* reserved */
	std::memcpy(output + kDmiSendHeaderSize, payload, size);

	return total_length;
}

inline std::size_t tcp_framing_build_two_byte_len_send_frame(
	const void* payload,
	std::size_t size,
	unsigned char* output,
	std::size_t output_capacity,
	bool big_endian) noexcept
{
	const std::size_t total_length = kTwoByteLenHeaderSize + size;
	if (total_length > output_capacity)
	{
		return 0U;
	}

	if (big_endian)
	{
		output[0] = static_cast<unsigned char>((size >> 8) & 0xFFU);
		output[1] = static_cast<unsigned char>(size & 0xFFU);
	}
	else
	{
		output[0] = static_cast<unsigned char>(size & 0xFFU);
		output[1] = static_cast<unsigned char>((size >> 8) & 0xFFU);
	}
	std::memcpy(output + kTwoByteLenHeaderSize, payload, size);

	return total_length;
}

#endif
