#ifndef TCP_CONNECTION_SERVER_INCLUDE
#define TCP_CONNECTION_SERVER_INCLUDE

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "IConnection.h"
#include "tcp_framing.h"

class tcp_connection_server final : public IConnection
{
public:
	tcp_connection_server() noexcept;
	tcp_connection_server(
		const char* local_ip,
		uint16_t local_port,
		tcp_framing_method receive_framing = tcp_framing_method::none,
		tcp_framing_method send_framing = tcp_framing_method::none) noexcept;
	~tcp_connection_server() noexcept;

	tcp_connection_server(const tcp_connection_server&) = delete;
	tcp_connection_server& operator=(const tcp_connection_server&) = delete;

	tcp_connection_server(tcp_connection_server&& other) noexcept;
	tcp_connection_server& operator=(tcp_connection_server&& other) noexcept;

	connection_result open() noexcept override;
	connection_result close() noexcept override;
	connection_result send(const void* data, std::size_t size) noexcept override;
	connection_result receive(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept override;

	connection_result init(
		const char* local_ip,
		uint16_t local_port,
		tcp_framing_method receive_framing = tcp_framing_method::none,
		tcp_framing_method send_framing = tcp_framing_method::none) noexcept;

	int last_socket_error() const noexcept;
	bool is_open() const noexcept;
	bool has_client() const noexcept;
	tcp_framing_method receive_framing() const noexcept;
	tcp_framing_method send_framing() const noexcept;

	static const char* result_string(connection_result result) noexcept;

private:
	void reset_state() noexcept;
	void close_client() noexcept;
	connection_result ensure_client_connected() noexcept;
	connection_result receive_raw(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept;
	connection_result receive_dmi_frame(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept;
	connection_result receive_two_byte_len_frame(
		void* buffer,
		std::size_t capacity,
		std::size_t* received_size,
		bool big_endian) noexcept;
	connection_result receive_ndjson_line(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept;
	static std::uintptr_t invalid_socket_handle() noexcept;

	std::uintptr_t listen_socket_handle_;
	std::uintptr_t client_socket_handle_;
	bool has_configuration_;
	bool is_open_;
	int last_socket_error_;
	std::string local_ip_;
	uint16_t local_port_;
	tcp_framing_method receive_framing_method_;
	tcp_framing_method send_framing_method_;
	std::vector<unsigned char> receive_buffer_;
};

#endif
