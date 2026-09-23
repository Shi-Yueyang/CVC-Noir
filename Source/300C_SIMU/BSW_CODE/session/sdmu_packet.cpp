#include "sdmu_packet.h"

#include <array>
#include <cstddef>
#include <cstring>

extern "C"
{
#include "../../ASW_300C/asw_runtime_api.h"
#include "../../ASW_300C/Interface_Data.h"
}

#include "session_crc.h"

namespace {

constexpr unsigned char kSecurePacketHead = 0x02U;
constexpr unsigned char kSecurePacketType = 0x33U;
constexpr unsigned char kAccuratePacketType = 0x39U;
constexpr unsigned char kPacketTail = 0x03U;
constexpr std::uint16_t kWheelDiameterMm = 1050U;
constexpr std::uint16_t kTeethCount = 200U;
constexpr unsigned char kSlipByte = 0U;
constexpr unsigned char kSensorUsed = 0U;
constexpr unsigned char kSensorPushState = 0U;

void append_u8(unsigned char*& dest, std::uint8_t value) noexcept
{
	*dest++ = value;
}

void append_u16_be(unsigned char*& dest, std::uint16_t value) noexcept
{
	*dest++ = static_cast<unsigned char>((value >> 8) & 0xFFU);
	*dest++ = static_cast<unsigned char>(value & 0xFFU);
}

void append_i16_be(unsigned char*& dest, std::int16_t value) noexcept
{
	append_u16_be(dest, static_cast<std::uint16_t>(value));
}

void append_u32_be(unsigned char*& dest, std::uint32_t value) noexcept
{
	*dest++ = static_cast<unsigned char>((value >> 24) & 0xFFU);
	*dest++ = static_cast<unsigned char>((value >> 16) & 0xFFU);
	*dest++ = static_cast<unsigned char>((value >> 8) & 0xFFU);
	*dest++ = static_cast<unsigned char>(value & 0xFFU);
}

void append_i32_be(unsigned char*& dest, std::int32_t value) noexcept
{
	append_u32_be(dest, static_cast<std::uint32_t>(value));
}

void append_u64_be(unsigned char*& dest, std::uint64_t value) noexcept
{
	append_u32_be(dest, static_cast<std::uint32_t>(value >> 32));
	append_u32_be(dest, static_cast<std::uint32_t>(value & 0xFFFFFFFFULL));
}

void append_i64_be(unsigned char*& dest, std::int64_t value) noexcept
{
	append_u64_be(dest, static_cast<std::uint64_t>(value));
}

void store_u16_be(unsigned char* dest, std::uint16_t value) noexcept
{
	dest[0] = static_cast<unsigned char>((value >> 8) & 0xFFU);
	dest[1] = static_cast<unsigned char>(value & 0xFFU);
}

void store_u32_be(unsigned char* dest, std::uint32_t value) noexcept
{
	dest[0] = static_cast<unsigned char>((value >> 24) & 0xFFU);
	dest[1] = static_cast<unsigned char>((value >> 16) & 0xFFU);
	dest[2] = static_cast<unsigned char>((value >> 8) & 0xFFU);
	dest[3] = static_cast<unsigned char>(value & 0xFFU);
}

}

std::uint32_t sdmu_current_time_ms() noexcept
{
	UINT_32 run_time_10ms = 0U;
	API_ReadCurrentRunTime(&run_time_10ms);
	return static_cast<std::uint32_t>(run_time_10ms) * 10U;
}

void assemble_secure_sdmu_packet(const asw_motion_sample& sample, unsigned char* dest, std::uint16_t* len) noexcept
{
	std::int64_t estimated_pos_mm = sample.position_mm;
	std::uint32_t estimated_vel_mm_s = sample.velocity_mm_s < 0 ? 0U : static_cast<std::uint32_t>(sample.velocity_mm_s);
	std::int16_t estimated_acc_mm_s2 = static_cast<std::int16_t>(sample.acceleration_mm_s2);
	std::int64_t max_pos_mm = estimated_pos_mm > 1000 ? estimated_pos_mm + 1000 : estimated_pos_mm;
	std::int64_t min_pos_mm = estimated_pos_mm > 1000 ? estimated_pos_mm - 1000 : estimated_pos_mm;
	std::uint32_t max_vel_mm_s = estimated_vel_mm_s > 100 ? estimated_vel_mm_s + 100 : estimated_vel_mm_s;
	std::uint32_t min_vel_mm_s = estimated_vel_mm_s > 100 ? estimated_vel_mm_s - 100 : estimated_vel_mm_s;
	std::uint32_t crc = 0U;
	std::uint16_t data_len = 0U;
	unsigned char output_packet[200] = { 0U };
	unsigned char* temp_ptr = output_packet;

	append_u8(temp_ptr, kSecurePacketHead);
	append_u8(temp_ptr, kSecurePacketType);
	append_u16_be(temp_ptr, data_len);

	const std::uint32_t system_time = sdmu_current_time_ms();
	append_u32_be(temp_ptr, system_time);
	append_i64_be(temp_ptr, estimated_pos_mm);
	append_i64_be(temp_ptr, max_pos_mm);
	append_i64_be(temp_ptr, min_pos_mm);
	append_u32_be(temp_ptr, estimated_vel_mm_s);
	append_u32_be(temp_ptr, max_vel_mm_s);
	append_u32_be(temp_ptr, min_vel_mm_s);
	append_i16_be(temp_ptr, estimated_acc_mm_s2);
	append_u8(temp_ptr, sample.direction);
	append_u8(temp_ptr, sample.motion);

	unsigned char* crc_addr = temp_ptr;
	temp_ptr += 4;
	append_u8(temp_ptr, kPacketTail);

	data_len = static_cast<std::uint16_t>(crc_addr - output_packet - 2);
	store_u16_be(output_packet + 2, data_len);

	crc = session_crc32(output_packet, static_cast<std::uint16_t>(data_len + 2));
	store_u32_be(crc_addr, crc);

	const std::uint16_t packet_len = static_cast<std::uint16_t>(data_len + 7);
	std::memcpy(dest, output_packet, packet_len);
	*len = packet_len;
}

void assemble_accurate_sdmu_packet(const asw_motion_sample& sample, unsigned char* dest, std::uint16_t* len) noexcept
{
	unsigned char output_packet[200] = { 0U };
	std::uint16_t data_len = 0U;
	std::uint32_t crc = 0U;
	std::uint64_t time_us = static_cast<std::uint64_t>(sample.time_ms) * 1000U;
	unsigned char cumulative_ws_state = 0U;
	unsigned char* temp_ptr = output_packet;

	append_u8(temp_ptr, kSecurePacketHead);
	append_u8(temp_ptr, kAccuratePacketType);
	append_u16_be(temp_ptr, data_len);

	const std::uint32_t system_time = sdmu_current_time_ms();
	const std::uint32_t velocity_mm_s = sample.velocity_mm_s < 0 ? 0U : static_cast<std::uint32_t>(sample.velocity_mm_s);
	const std::int16_t acceleration_mm_s2 = static_cast<std::int16_t>(sample.acceleration_mm_s2);

	append_u32_be(temp_ptr, system_time);
	append_i32_be(temp_ptr, sample.position_mm);
	append_u32_be(temp_ptr, velocity_mm_s);
	append_i16_be(temp_ptr, acceleration_mm_s2);

	const unsigned char pulse_states[3] = { sample.pulse_state_1, sample.pulse_state_2, sample.pulse_state_3 };
	const std::int32_t pulse_counts[3] = { sample.pulse_count_1, sample.pulse_count_2, sample.pulse_count_3 };
	for (int index = 0; index < 3; ++index)
	{
		append_u64_be(temp_ptr, time_us);
		append_u16_be(temp_ptr, kWheelDiameterMm);
		append_u16_be(temp_ptr, kTeethCount);
		append_i32_be(temp_ptr, pulse_counts[index]);
		if (pulse_states[index] == 0U)
		{
			cumulative_ws_state = 1U;
		}
		append_u8(temp_ptr, cumulative_ws_state);
		append_u8(temp_ptr, kSlipByte);
	}

	append_u8(temp_ptr, kSensorUsed);
	append_u8(temp_ptr, kSensorPushState);

	unsigned char* crc_addr = temp_ptr;
	temp_ptr += 4;
	append_u8(temp_ptr, kPacketTail);

	data_len = static_cast<std::uint16_t>(crc_addr - output_packet - 2);
	store_u16_be(output_packet + 2, data_len);

	crc = session_crc32(output_packet, static_cast<std::uint16_t>(data_len + 2));
	store_u32_be(crc_addr, crc);

	const std::uint16_t packet_len = static_cast<std::uint16_t>(data_len + 7);
	std::memcpy(dest, output_packet, packet_len);
	*len = packet_len;
}

bool write_asw_message(const unsigned char* payload, std::uint16_t payload_size) noexcept
{
	ASWRxData_t asw_rx_data = { 0 };
	asw_rx_data.SrcID = ATP_SDLU_IN_ATP_SESSION_ID;
	asw_rx_data.Size = payload_size;
	std::memcpy(asw_rx_data.Data, payload, payload_size);
	const std::int16_t data_size = static_cast<std::int16_t>(sizeof(ASWRxData_t) - ASW_COM_DATA_SIZE + asw_rx_data.Size);
	return CVC_FALSE != CVC_BSW_ITF_Write(CVC_BSW_ASW_COM_RX_TYPE, reinterpret_cast<unsigned char*>(&asw_rx_data), data_size);
}
