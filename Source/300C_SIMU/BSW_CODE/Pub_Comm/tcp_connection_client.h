#ifndef TCP_CONNECTION_CLIENT_INCLUDE
#define TCP_CONNECTION_CLIENT_INCLUDE

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "IConnection.h"
#include "tcp_framing.h"

class tcp_connection_client final : public IConnection
{
public:
	tcp_connection_client() noexcept;
	tcp_connection_client(
		const char* remote_ip,
		uint16_t remote_port,
		const char* local_ip = nullptr,
		uint16_t local_port = 0U,
		tcp_framing_method receive_framing = tcp_framing_method::none,
		tcp_framing_method send_framing = tcp_framing_method::none) noexcept;
	~tcp_connection_client() noexcept;

	tcp_connection_client(const tcp_connection_client&) = delete;
	tcp_connection_client& operator=(const tcp_connection_client&) = delete;

	tcp_connection_client(tcp_connection_client&& other) noexcept;
	tcp_connection_client& operator=(tcp_connection_client&& other) noexcept;

	connection_result open() noexcept override;
	connection_result close() noexcept override;
	connection_result send(const void* data, std::size_t size) noexcept override;
	connection_result receive(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept override;

	connection_result init(
		const char* remote_ip,
		uint16_t remote_port,
		const char* local_ip = nullptr,
		uint16_t local_port = 0U,
		tcp_framing_method receive_framing = tcp_framing_method::none,
		tcp_framing_method send_framing = tcp_framing_method::none) noexcept;

	int last_socket_error() const noexcept;
	bool is_open() const noexcept;
	bool is_connected() const noexcept;
	tcp_framing_method receive_framing() const noexcept;
	tcp_framing_method send_framing() const noexcept;

	static const char* result_string(connection_result result) noexcept;

private:
	void reset_state() noexcept;
	connection_result ensure_connected() noexcept;
	connection_result receive_raw(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept;
	connection_result receive_dmi_frame(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept;
	connection_result receive_two_byte_len_frame(
		void* buffer,
		std::size_t capacity,
		std::size_t* received_size,
		bool big_endian) noexcept;
	connection_result receive_ndjson_line(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept;
	static std::uintptr_t invalid_socket_handle() noexcept;

	std::uintptr_t socket_handle_;
	bool has_configuration_;
	bool is_open_;
	bool is_connected_;
	int last_socket_error_;
	std::string local_ip_;
	uint16_t local_port_;
	std::string remote_ip_;
	uint16_t remote_port_;
	tcp_framing_method receive_framing_method_;
	tcp_framing_method send_framing_method_;
	std::vector<unsigned char> receive_buffer_;
};

#endif
