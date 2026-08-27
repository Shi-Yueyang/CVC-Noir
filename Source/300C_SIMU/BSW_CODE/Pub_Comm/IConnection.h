#ifndef ICONNECTION_INCLUDE
#define ICONNECTION_INCLUDE

#include <cstddef>
#include <cstdint>

enum class connection_result
{
	ok = 0,
	would_block,
	invalid_argument,
	not_open,
	already_open,
	no_default_peer,
	startup_failed,
	socket_failed,
	bind_failed,
	send_failed,
	receive_failed,
	internal_error
};

class IConnection
{
public:
	virtual ~IConnection() = default;

	virtual connection_result open() noexcept = 0;

	virtual connection_result close() noexcept = 0;
	virtual connection_result send(const void* data, std::size_t size) noexcept = 0;
	virtual connection_result receive(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept = 0;
};

#endif