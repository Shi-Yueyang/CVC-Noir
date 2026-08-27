#ifndef UDP_CONNECTION_INCLUDE
#define UDP_CONNECTION_INCLUDE

#include <cstddef>
#include <cstdint>
#include <string>

#include "IConnection.h"

class udp_connection final : public IConnection
{
public:
	udp_connection() noexcept;
	udp_connection(const char* local_ip,
		uint16_t local_port,
		const char* default_peer_ip = nullptr,
		uint16_t default_peer_port = 0U) noexcept;
	~udp_connection() noexcept;

	udp_connection(const udp_connection&) = delete;
	udp_connection& operator=(const udp_connection&) = delete;

	udp_connection(udp_connection&& other) noexcept;
	udp_connection& operator=(udp_connection&& other) noexcept;

	connection_result open() noexcept override;

	connection_result init(
		const char* local_ip,
		uint16_t local_port,
		const char* default_peer_ip = nullptr,
		uint16_t default_peer_port = 0U) noexcept;

	connection_result set_default_peer(const char* ip, uint16_t port) noexcept;
	connection_result send(const void* data, std::size_t size) noexcept override;
	connection_result send_to(const char* ip, uint16_t port, const void* data, std::size_t size) noexcept;
	connection_result receive(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept override;
	int last_socket_error() const noexcept;
	connection_result close() noexcept override;
	bool is_open() const noexcept;

	static const char* result_string(connection_result result) noexcept;

private:
	void reset_state() noexcept;
	static std::uintptr_t invalid_socket_handle() noexcept;

	std::uintptr_t socket_handle_;
	bool has_configuration_;
	bool is_open_;
	bool has_default_peer_;
	int last_socket_error_;
	std::string local_ip_;
	uint16_t local_port_;
	std::string default_peer_ip_;
	uint16_t default_peer_port_;
};

#endif