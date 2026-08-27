#ifndef SERIAL_CONNECTION_INCLUDE
#define SERIAL_CONNECTION_INCLUDE

#include <cstddef>
#include <cstdint>
#include <string>

#include "IConnection.h"

class serial_connection final : public IConnection
{
public:
	serial_connection() noexcept;
	serial_connection(
		const char* port_name,
		uint32_t baud_rate,
		uint8_t data_bits,
		uint8_t stop_bits,
		uint8_t parity,
		uint8_t rts_cts,
		uint32_t read_total_timeout_ms) noexcept;
	~serial_connection() noexcept;

	serial_connection(const serial_connection&) = delete;
	serial_connection& operator=(const serial_connection&) = delete;

	serial_connection(serial_connection&& other) noexcept;
	serial_connection& operator=(serial_connection&& other) noexcept;

	connection_result open() noexcept override;
	connection_result close() noexcept override;
	connection_result send(const void* data, std::size_t size) noexcept override;
	connection_result receive(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept override;

	connection_result init(
		const char* port_name,
		uint32_t baud_rate,
		uint8_t data_bits,
		uint8_t stop_bits,
		uint8_t parity,
		uint8_t rts_cts,
		uint32_t read_total_timeout_ms) noexcept;

	uint32_t last_error() const noexcept;
	bool is_open() const noexcept;

private:
	void reset_state() noexcept;
	static std::uintptr_t invalid_handle() noexcept;

	std::uintptr_t handle_;
	bool has_configuration_;
	bool is_open_;
	uint32_t last_error_;
	std::string port_name_;
	uint32_t baud_rate_;
	uint8_t data_bits_;
	uint8_t stop_bits_;
	uint8_t parity_;
	uint8_t rts_cts_;
	uint32_t read_total_timeout_ms_;
};

#endif