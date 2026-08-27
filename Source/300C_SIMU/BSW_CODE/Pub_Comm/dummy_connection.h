#ifndef DUMMY_CONNECTION_INCLUDE
#define DUMMY_CONNECTION_INCLUDE

#include <cstddef>
#include <cstdint>
#include <vector>

#include "IConnection.h"

class dummy_connection final : public IConnection
{
public:
	dummy_connection() noexcept;
	explicit dummy_connection(std::vector<std::uint8_t> payload) noexcept;
	~dummy_connection() noexcept override = default;

	dummy_connection(const dummy_connection&) = delete;
	dummy_connection& operator=(const dummy_connection&) = delete;
	dummy_connection(dummy_connection&& other) noexcept = default;
	dummy_connection& operator=(dummy_connection&& other) noexcept = default;

	connection_result open() noexcept override;
	connection_result close() noexcept override;
	connection_result send(const void* data, std::size_t size) noexcept override;
	connection_result receive(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept override;

private:
	bool is_open_;
	bool payload_pending_;
	std::vector<std::uint8_t> payload_;
};

#endif
