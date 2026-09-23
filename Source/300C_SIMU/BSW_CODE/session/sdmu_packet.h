#ifndef SDMU_PACKET_INCLUDE
#define SDMU_PACKET_INCLUDE

#include <cstdint>

struct asw_motion_sample
{
	std::uint32_t time_ms = 0U;
	std::int32_t position_mm = 0;
	std::int32_t velocity_mm_s = 0;
	std::int32_t acceleration_mm_s2 = 0;
	unsigned char pulse_state_1 = 0U;
	unsigned char pulse_state_2 = 0U;
	unsigned char pulse_state_3 = 0U;
	std::int32_t pulse_count_1 = 0;
	std::int32_t pulse_count_2 = 0;
	std::int32_t pulse_count_3 = 0;
	unsigned char motion = 0U;
	unsigned char direction = 0U;
};

std::uint32_t sdmu_current_time_ms() noexcept;

void assemble_secure_sdmu_packet(const asw_motion_sample& sample, unsigned char* dest, std::uint16_t* len) noexcept;

void assemble_accurate_sdmu_packet(const asw_motion_sample& sample, unsigned char* dest, std::uint16_t* len) noexcept;

bool write_asw_message(const unsigned char* payload, std::uint16_t payload_size) noexcept;

#endif
