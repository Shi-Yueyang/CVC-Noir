#ifndef SESSION_CRC_INCLUDE
#define SESSION_CRC_INCLUDE

#include <cstdint>

std::uint32_t session_crc32(const unsigned char* data, std::uint16_t length) noexcept;

#endif