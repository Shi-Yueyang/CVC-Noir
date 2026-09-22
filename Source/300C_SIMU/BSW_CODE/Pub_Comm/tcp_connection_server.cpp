#include "tcp_connection_server.h"

#include "logging/logger.h"

#include <climits>
#include <cstring>

#include <array>
#include <utility>

#ifdef _WIN32
#include <WinSock2.h>
#include <Ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#endif

namespace
{
	constexpr int kListenBacklog = 1;

	long s_tcp_connection_server_transport_users = 0;

#ifdef _WIN32
	typedef SOCKET socket_t;
	#define INVALID_SOCKET_VAL INVALID_SOCKET
	#define SOCKET_ERR SOCKET_ERROR
	#define WOULD_BLOCK_ERR WSAEWOULDBLOCK
	#define CONN_RESET_ERR WSAECONNRESET
	#define NOT_CONN_ERR WSAENOTCONN
	#define INVAL_ERR WSAEINVAL

	typedef int tcp_connection_server_socklen;

	int tcp_connection_server_get_last_error() noexcept
	{
		return WSAGetLastError();
	}

	connection_result tcp_connection_server_transport_acquire(int* last_socket_error) noexcept
	{
		WSADATA wsa_data;
		int result = 0;

		if (s_tcp_connection_server_transport_users == 0)
		{
			result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
			if (result != 0)
			{
				if (last_socket_error != nullptr)
				{
					*last_socket_error = tcp_connection_server_get_last_error();
				}
				return connection_result::startup_failed;
			}
		}

		++s_tcp_connection_server_transport_users;
		return connection_result::ok;
	}

	void tcp_connection_server_transport_release() noexcept
	{
		if (s_tcp_connection_server_transport_users <= 0)
		{
			return;
		}

		--s_tcp_connection_server_transport_users;
		if (s_tcp_connection_server_transport_users == 0)
		{
			(void)WSACleanup();
		}
	}

	int tcp_connection_server_set_nonblocking(socket_t socket_handle) noexcept
	{
		unsigned long nonblocking = 1UL;
		if (ioctlsocket(socket_handle, FIONBIO, &nonblocking) != 0)
		{
			return tcp_connection_server_get_last_error();
		}

		return 0;
	}

	int tcp_connection_server_socket_open(socket_t* sock_handle) noexcept
	{
		socket_t socket_handle = INVALID_SOCKET_VAL;
		int value = 1;
		int option_length = sizeof(value);

		if (sock_handle == nullptr)
		{
			return WSAEINVAL;
		}

		socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (socket_handle == INVALID_SOCKET_VAL)
		{
			return tcp_connection_server_get_last_error();
		}

		if (setsockopt(socket_handle, SOL_SOCKET, SO_REUSEADDR, (char*)&value, option_length) < 0)
		{
			const int error_code = tcp_connection_server_get_last_error();
			(void)closesocket(socket_handle);
			return error_code;
		}

		if (tcp_connection_server_set_nonblocking(socket_handle) != 0)
		{
			const int error_code = tcp_connection_server_get_last_error();
			(void)closesocket(socket_handle);
			return error_code;
		}

		*sock_handle = socket_handle;
		return 0;
	}

	int tcp_connection_server_socket_bind(socket_t sock_handle, const char* ip, uint16_t port) noexcept
	{
		struct sockaddr_in local_address;

		if (port == 0U)
		{
			return WSAEINVAL;
		}

		memset(&local_address, 0, sizeof(local_address));
		local_address.sin_family = AF_INET;
		local_address.sin_port = htons(port);
		if ((ip == nullptr) || ('\0' == ip[0]))
		{
			local_address.sin_addr.s_addr = htonl(INADDR_ANY);
		}
		else if (InetPtonA(AF_INET, ip, &local_address.sin_addr) != 1)
		{
			return WSAEINVAL;
		}

		if (bind(sock_handle, (struct sockaddr*)&local_address, sizeof(local_address)) < 0)
		{
			return tcp_connection_server_get_last_error();
		}

		return 0;
	}

	int tcp_connection_server_socket_listen(socket_t sock_handle) noexcept
	{
		if (listen(sock_handle, kListenBacklog) != 0)
		{
			return tcp_connection_server_get_last_error();
		}

		return 0;
	}

	int tcp_connection_server_socket_close(socket_t sock_handle) noexcept
	{
		if (sock_handle == INVALID_SOCKET_VAL)
		{
			return 0;
		}

		if (closesocket(sock_handle) != 0)
		{
			return tcp_connection_server_get_last_error();
		}

		return 0;
	}

	bool tcp_connection_server_is_readable(socket_t sock_handle) noexcept
	{
		fd_set read_set;
		timeval timeout;

		FD_ZERO(&read_set);
		FD_SET(sock_handle, &read_set);
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		return select(0, &read_set, nullptr, nullptr, &timeout) > 0;
	}

#else /* POSIX */

	typedef int socket_t;
	#define INVALID_SOCKET_VAL (-1)
	#define SOCKET_ERR (-1)
	#define WOULD_BLOCK_ERR EAGAIN
	#define CONN_RESET_ERR ECONNRESET
	#define NOT_CONN_ERR ENOTCONN
	#define INVAL_ERR EINVAL

	typedef socklen_t tcp_connection_server_socklen;

	int tcp_connection_server_get_last_error() noexcept
	{
		return errno;
	}

	connection_result tcp_connection_server_transport_acquire(int* /*last_socket_error*/) noexcept
	{
		/* POSIX: no WSAStartup needed. Reference count for symmetry. */
		++s_tcp_connection_server_transport_users;
		return connection_result::ok;
	}

	void tcp_connection_server_transport_release() noexcept
	{
		if (s_tcp_connection_server_transport_users <= 0)
		{
			return;
		}
		--s_tcp_connection_server_transport_users;
	}

	int tcp_connection_server_set_nonblocking(socket_t socket_handle) noexcept
	{
		int flags = fcntl(socket_handle, F_GETFL, 0);
		if (flags < 0)
		{
			return errno;
		}
		if (fcntl(socket_handle, F_SETFL, flags | O_NONBLOCK) < 0)
		{
			return errno;
		}
		return 0;
	}

	int tcp_connection_server_socket_open(socket_t* sock_handle) noexcept
	{
		socket_t socket_handle = INVALID_SOCKET_VAL;
		int value = 1;
		socklen_t option_length = sizeof(value);

		if (sock_handle == nullptr)
		{
			return EINVAL;
		}

		socket_handle = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_handle == INVALID_SOCKET_VAL)
		{
			return tcp_connection_server_get_last_error();
		}

		if (setsockopt(socket_handle, SOL_SOCKET, SO_REUSEADDR, &value, option_length) < 0)
		{
			const int error_code = tcp_connection_server_get_last_error();
			(void)close(socket_handle);
			return error_code;
		}

		if (tcp_connection_server_set_nonblocking(socket_handle) != 0)
		{
			const int error_code = tcp_connection_server_get_last_error();
			(void)close(socket_handle);
			return error_code;
		}

		*sock_handle = socket_handle;
		return 0;
	}

	int tcp_connection_server_socket_bind(socket_t sock_handle, const char* ip, uint16_t port) noexcept
	{
		struct sockaddr_in local_address;

		if (port == 0U)
		{
			return EINVAL;
		}

		memset(&local_address, 0, sizeof(local_address));
		local_address.sin_family = AF_INET;
		local_address.sin_port = htons(port);
		if ((ip == nullptr) || ('\0' == ip[0]))
		{
			local_address.sin_addr.s_addr = htonl(INADDR_ANY);
		}
		else if (inet_pton(AF_INET, ip, &local_address.sin_addr) != 1)
		{
			return EINVAL;
		}

		if (bind(sock_handle, (struct sockaddr*)&local_address, sizeof(local_address)) < 0)
		{
			return tcp_connection_server_get_last_error();
		}

		return 0;
	}

	int tcp_connection_server_socket_listen(socket_t sock_handle) noexcept
	{
		if (listen(sock_handle, kListenBacklog) != 0)
		{
			return tcp_connection_server_get_last_error();
		}

		return 0;
	}

	int tcp_connection_server_socket_close(socket_t sock_handle) noexcept
	{
		if (sock_handle == INVALID_SOCKET_VAL)
		{
			return 0;
		}

		if (close(sock_handle) != 0)
		{
			return tcp_connection_server_get_last_error();
		}

		return 0;
	}

	bool tcp_connection_server_is_readable(socket_t sock_handle) noexcept
	{
		fd_set read_set;
		timeval timeout;

		FD_ZERO(&read_set);
		FD_SET(sock_handle, &read_set);
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		/* On POSIX, nfds must be the highest fd + 1 */
		return select(sock_handle + 1, &read_set, nullptr, nullptr, &timeout) > 0;
	}

#endif /* _WIN32 / POSIX */

	void tcp_connection_server_format_endpoint(
		const struct sockaddr* addr,
		char* ip_text,
		std::size_t ip_text_size,
		unsigned* port) noexcept
	{
		const char* const k_unknown = "unknown";
		std::size_t copy_len = 0U;

		*port = 0U;

		if ((addr != nullptr) && (addr->sa_family == AF_INET))
		{
			const struct sockaddr_in* ipv4 = reinterpret_cast<const struct sockaddr_in*>(addr);
			if (inet_ntop(AF_INET, &ipv4->sin_addr, ip_text, static_cast<tcp_connection_server_socklen>(ip_text_size)) != nullptr)
			{
				*port = static_cast<unsigned>(ntohs(ipv4->sin_port));
				return;
			}
		}

		copy_len = (ip_text_size - 1U < strlen(k_unknown)) ? (ip_text_size - 1U) : strlen(k_unknown);
		memcpy(ip_text, k_unknown, copy_len);
		ip_text[copy_len] = '\0';
	}

	connection_result map_send_error(int error_code) noexcept
	{
		if (error_code == WOULD_BLOCK_ERR)
		{
			return connection_result::would_block;
		}
		return connection_result::send_failed;
	}

	connection_result map_receive_error(int error_code) noexcept
	{
		if (error_code == WOULD_BLOCK_ERR)
		{
			return connection_result::would_block;
		}
		return connection_result::receive_failed;
	}

}

/* ── Rest of the file (class methods) is platform-independent ── */

connection_result tcp_connection_server::receive_two_byte_len_frame(
	void* buffer,
	std::size_t capacity,
	std::size_t* received_size,
	bool big_endian) noexcept
{
	std::array<unsigned char, kTcpFramingReceiveChunkSize> chunk = {};
	connection_result client_result;

	if (tcp_framing_try_extract_two_byte_len_frame(receive_buffer_, buffer, capacity, received_size, big_endian))
	{
		return connection_result::ok;
	}
	if (*received_size > capacity)
	{
		return connection_result::invalid_argument;
	}

	client_result = ensure_client_connected();
	if (client_result != connection_result::ok)
	{
		return client_result;
	}

	if (!tcp_connection_server_is_readable(static_cast<socket_t>(client_socket_handle_)))
	{
		return connection_result::would_block;
	}

	for (;;)
	{
#ifdef _WIN32
		const int received = recv(
			static_cast<socket_t>(client_socket_handle_),
			reinterpret_cast<char*>(chunk.data()),
			static_cast<int>(chunk.size()),
			0);
#else
		const ssize_t received_raw = recv(
			static_cast<socket_t>(client_socket_handle_),
			chunk.data(),
			chunk.size(),
			0);
		const int received = (received_raw < 0) ? (int)received_raw : (int)received_raw;
#endif
		if (received == SOCKET_ERR)
		{
			last_socket_error_ = tcp_connection_server_get_last_error();
			if (last_socket_error_ == CONN_RESET_ERR || last_socket_error_ == NOT_CONN_ERR)
			{
				close_client();
			}
			return map_receive_error(last_socket_error_);
		}

		if (received == 0)
		{
			close_client();
			return connection_result::would_block;
		}

		receive_buffer_.insert(receive_buffer_.end(), chunk.begin(), chunk.begin() + received);
		if (tcp_framing_try_extract_two_byte_len_frame(receive_buffer_, buffer, capacity, received_size, big_endian))
		{
			return connection_result::ok;
		}
		if (*received_size > capacity)
		{
			return connection_result::invalid_argument;
		}

		if (!tcp_connection_server_is_readable(static_cast<socket_t>(client_socket_handle_)))
		{
			return connection_result::would_block;
		}
	}
}

tcp_connection_server::tcp_connection_server() noexcept
	: listen_socket_handle_(invalid_socket_handle())
	, client_socket_handle_(invalid_socket_handle())
	, has_configuration_(false)
	, is_open_(false)
	, last_socket_error_(0)
	, local_port_(0U)
	, receive_framing_method_(tcp_framing_method::none)
	, send_framing_method_(tcp_framing_method::none)
{
}

tcp_connection_server::tcp_connection_server(
	const char* local_ip,
	uint16_t local_port,
	tcp_framing_method receive_framing,
	tcp_framing_method send_framing) noexcept
	: tcp_connection_server()
{
	(void)init(local_ip, local_port, receive_framing, send_framing);
}

tcp_connection_server::~tcp_connection_server() noexcept
{
	(void)close();
}

tcp_connection_server::tcp_connection_server(tcp_connection_server&& other) noexcept
	: listen_socket_handle_(other.listen_socket_handle_)
	, client_socket_handle_(other.client_socket_handle_)
	, has_configuration_(other.has_configuration_)
	, is_open_(other.is_open_)
	, last_socket_error_(other.last_socket_error_)
	, local_ip_(std::move(other.local_ip_))
	, local_port_(other.local_port_)
	, receive_framing_method_(other.receive_framing_method_)
	, send_framing_method_(other.send_framing_method_)
	, receive_buffer_(std::move(other.receive_buffer_))
{
	other.reset_state();
}

tcp_connection_server& tcp_connection_server::operator=(tcp_connection_server&& other) noexcept
{
	if (this != &other)
	{
		(void)close();
		listen_socket_handle_ = other.listen_socket_handle_;
		client_socket_handle_ = other.client_socket_handle_;
		has_configuration_ = other.has_configuration_;
		is_open_ = other.is_open_;
		last_socket_error_ = other.last_socket_error_;
		local_ip_ = std::move(other.local_ip_);
		local_port_ = other.local_port_;
		receive_framing_method_ = other.receive_framing_method_;
		send_framing_method_ = other.send_framing_method_;
		receive_buffer_ = std::move(other.receive_buffer_);
		other.reset_state();
	}

	return *this;
}

void tcp_connection_server::reset_state() noexcept
{
	listen_socket_handle_ = invalid_socket_handle();
	client_socket_handle_ = invalid_socket_handle();
	has_configuration_ = false;
	is_open_ = false;
	last_socket_error_ = 0;
	local_ip_.clear();
	local_port_ = 0U;
	receive_framing_method_ = tcp_framing_method::none;
	send_framing_method_ = tcp_framing_method::none;
	receive_buffer_.clear();
}

std::uintptr_t tcp_connection_server::invalid_socket_handle() noexcept
{
	return static_cast<std::uintptr_t>(INVALID_SOCKET_VAL);
}

connection_result tcp_connection_server::init(
	const char* local_ip,
	uint16_t local_port,
	tcp_framing_method receive_framing,
	tcp_framing_method send_framing) noexcept
{
	if (is_open_)
	{
		return connection_result::already_open;
	}

	local_ip_ = ((local_ip != nullptr) && (local_ip[0] != '\0')) ? local_ip : std::string();
	local_port_ = local_port;
	receive_framing_method_ = receive_framing;
	send_framing_method_ = send_framing;
	has_configuration_ = (local_port_ != 0U);
	receive_buffer_.clear();

	return has_configuration_ ? connection_result::ok : connection_result::invalid_argument;
}

connection_result tcp_connection_server::open() noexcept
{
	socket_t listen_socket = INVALID_SOCKET_VAL;
	int result = 0;
	connection_result transport_result;

	if (is_open_)
	{
		return connection_result::already_open;
	}

	last_socket_error_ = 0;
	if (!has_configuration_)
	{
		return connection_result::invalid_argument;
	}

	transport_result = tcp_connection_server_transport_acquire(&last_socket_error_);
	if (transport_result != connection_result::ok)
	{
		return transport_result;
	}

	result = tcp_connection_server_socket_open(&listen_socket);
	if (result != 0)
	{
		last_socket_error_ = result;
		tcp_connection_server_transport_release();
		return connection_result::socket_failed;
	}

	result = tcp_connection_server_socket_bind(listen_socket, local_ip_.empty() ? nullptr : local_ip_.c_str(), local_port_);
	if (result != 0)
	{
		last_socket_error_ = result;
		(void)tcp_connection_server_socket_close(listen_socket);
		tcp_connection_server_transport_release();
		return connection_result::bind_failed;
	}

	result = tcp_connection_server_socket_listen(listen_socket);
	if (result != 0)
	{
		last_socket_error_ = result;
		(void)tcp_connection_server_socket_close(listen_socket);
		tcp_connection_server_transport_release();
		return connection_result::socket_failed;
	}

	listen_socket_handle_ = static_cast<std::uintptr_t>(listen_socket);
	client_socket_handle_ = invalid_socket_handle();
	is_open_ = true;
	receive_buffer_.clear();
	return connection_result::ok;
}

void tcp_connection_server::close_client() noexcept
{
	if (client_socket_handle_ != invalid_socket_handle())
	{
		(void)tcp_connection_server_socket_close(static_cast<socket_t>(client_socket_handle_));
		client_socket_handle_ = invalid_socket_handle();
	}
	receive_buffer_.clear();
}

connection_result tcp_connection_server::close() noexcept
{
	if (!is_open_)
	{
		return connection_result::not_open;
	}

	close_client();
	last_socket_error_ = tcp_connection_server_socket_close(static_cast<socket_t>(listen_socket_handle_));
	listen_socket_handle_ = invalid_socket_handle();
	is_open_ = false;
	tcp_connection_server_transport_release();

	return (last_socket_error_ == 0) ? connection_result::ok : connection_result::socket_failed;
}

connection_result tcp_connection_server::ensure_client_connected() noexcept
{
	socket_t client_socket = INVALID_SOCKET_VAL;
	int accept_error = 0;

	if (!is_open_)
	{
		return connection_result::not_open;
	}

	if (client_socket_handle_ != invalid_socket_handle())
	{
		return connection_result::ok;
	}

	if (!tcp_connection_server_is_readable(static_cast<socket_t>(listen_socket_handle_)))
	{
		return connection_result::would_block;
	}

#ifdef _WIN32
	client_socket = accept(static_cast<socket_t>(listen_socket_handle_), nullptr, nullptr);
	if (client_socket == INVALID_SOCKET_VAL)
	{
		accept_error = tcp_connection_server_get_last_error();
		last_socket_error_ = accept_error;
		if (accept_error == WOULD_BLOCK_ERR)
		{
			return connection_result::would_block;
		}
		return connection_result::socket_failed;
	}
#else
	client_socket = accept(static_cast<socket_t>(listen_socket_handle_), nullptr, nullptr);
	if (client_socket == INVALID_SOCKET_VAL)
	{
		accept_error = tcp_connection_server_get_last_error();
		last_socket_error_ = accept_error;
		if (accept_error == EAGAIN || accept_error == EWOULDBLOCK)
		{
			return connection_result::would_block;
		}
		return connection_result::socket_failed;
	}
#endif

	accept_error = tcp_connection_server_set_nonblocking(client_socket);
	if (accept_error != 0)
	{
		last_socket_error_ = accept_error;
		(void)tcp_connection_server_socket_close(client_socket);
		return connection_result::socket_failed;
	}

	client_socket_handle_ = static_cast<std::uintptr_t>(client_socket);
	receive_buffer_.clear();
	{
		struct sockaddr_storage local_addr;
		struct sockaddr_storage peer_addr;
		tcp_connection_server_socklen local_len = static_cast<tcp_connection_server_socklen>(sizeof(local_addr));
		tcp_connection_server_socklen peer_len = static_cast<tcp_connection_server_socklen>(sizeof(peer_addr));
		const struct sockaddr* local_addr_ptr = nullptr;
		const struct sockaddr* peer_addr_ptr = nullptr;
		char server_ip_text[46];
		char client_ip_text[46];
		unsigned server_port = 0U;
		unsigned client_port = 0U;

		if (getsockname(client_socket, reinterpret_cast<struct sockaddr*>(&local_addr), &local_len) == 0)
		{
			local_addr_ptr = reinterpret_cast<const struct sockaddr*>(&local_addr);
		}
		if (getpeername(client_socket, reinterpret_cast<struct sockaddr*>(&peer_addr), &peer_len) == 0)
		{
			peer_addr_ptr = reinterpret_cast<const struct sockaddr*>(&peer_addr);
		}

		tcp_connection_server_format_endpoint(local_addr_ptr, server_ip_text, sizeof(server_ip_text), &server_port);
		tcp_connection_server_format_endpoint(peer_addr_ptr, client_ip_text, sizeof(client_ip_text), &client_port);
		::log_info("simulation", "client connected to tcp server %s:%u from %s:%u", server_ip_text, server_port, client_ip_text, client_port);
	}
	return connection_result::ok;
}

connection_result tcp_connection_server::send(const void* data, std::size_t size) noexcept
{
	const unsigned char* raw_data = static_cast<const unsigned char*>(data);
	std::size_t raw_size = size;
	unsigned char framed_buf[kTcpFramingSendBufferSize];

	if ((data == nullptr) || (size == 0U) || (size > static_cast<std::size_t>(INT_MAX)))
	{
		return connection_result::invalid_argument;
	}

	/* Apply send-framing if configured. */
	if (send_framing_method_ == tcp_framing_method::dmi)
	{
		raw_size = tcp_framing_build_dmi_send_frame(data, size, framed_buf, sizeof(framed_buf));
		if (raw_size == 0U)
		{
			return connection_result::invalid_argument;
		}
		raw_data = framed_buf;
	}
	else if (send_framing_method_ == tcp_framing_method::two_byte_len_big)
	{
		raw_size = tcp_framing_build_two_byte_len_send_frame(data, size, framed_buf, sizeof(framed_buf), true);
		if (raw_size == 0U)
		{
			return connection_result::invalid_argument;
		}
		raw_data = framed_buf;
	}
	else if (send_framing_method_ == tcp_framing_method::two_byte_len_little)
	{
		raw_size = tcp_framing_build_two_byte_len_send_frame(data, size, framed_buf, sizeof(framed_buf), false);
		if (raw_size == 0U)
		{
			return connection_result::invalid_argument;
		}
		raw_data = framed_buf;
	}
	else if (send_framing_method_ == tcp_framing_method::ndjson)
	{
		raw_size = tcp_framing_build_ndjson_send_frame(data, size, framed_buf, sizeof(framed_buf));
		if (raw_size == 0U)
		{
			return connection_result::invalid_argument;
		}
		raw_data = framed_buf;
	}

	const unsigned char* send_buffer = raw_data;
	std::size_t send_size = raw_size;
	std::size_t sent_total = 0U;
	connection_result client_result;

	client_result = ensure_client_connected();
	if (client_result != connection_result::ok)
	{
		return client_result;
	}

	while (sent_total < send_size)
	{
#ifdef _WIN32
		const int sent_now = ::send(
			static_cast<socket_t>(client_socket_handle_),
			reinterpret_cast<const char*>(send_buffer + sent_total),
			static_cast<int>(send_size - sent_total),
			0);
#else
		const ssize_t sent_now_raw = ::send(
			static_cast<socket_t>(client_socket_handle_),
			send_buffer + sent_total,
			send_size - sent_total,
			0);
		const int sent_now = (sent_now_raw < 0) ? -1 : (int)sent_now_raw;
#endif
		if (sent_now == SOCKET_ERR)
		{
			last_socket_error_ = tcp_connection_server_get_last_error();
			if (last_socket_error_ == CONN_RESET_ERR || last_socket_error_ == NOT_CONN_ERR)
			{
				close_client();
			}
			return map_send_error(last_socket_error_);
		}

		if (sent_now == 0)
		{
			close_client();
			return connection_result::send_failed;
		}

		sent_total += static_cast<std::size_t>(sent_now);
	}

	return connection_result::ok;
}

connection_result tcp_connection_server::receive(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept
{
	if (received_size != nullptr)
	{
		*received_size = 0U;
	}

	if ((buffer == nullptr) || (received_size == nullptr) || (capacity == 0U) || (capacity > static_cast<std::size_t>(INT_MAX)))
	{
		return connection_result::invalid_argument;
	}

	if (!is_open_)
	{
		return connection_result::not_open;
	}

	if (receive_framing_method_ == tcp_framing_method::dmi)
	{
		return receive_dmi_frame(buffer, capacity, received_size);
	}
	if (receive_framing_method_ == tcp_framing_method::two_byte_len_big)
	{
		return receive_two_byte_len_frame(buffer, capacity, received_size, true);
	}
	if (receive_framing_method_ == tcp_framing_method::two_byte_len_little)
	{
		return receive_two_byte_len_frame(buffer, capacity, received_size, false);
	}
	if (receive_framing_method_ == tcp_framing_method::ndjson)
	{
		return receive_ndjson_line(buffer, capacity, received_size);
	}

	return receive_raw(buffer, capacity, received_size);
}

connection_result tcp_connection_server::receive_raw(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept
{
	connection_result client_result = ensure_client_connected();
	if (client_result != connection_result::ok)
	{
		return client_result;
	}

	if (!tcp_connection_server_is_readable(static_cast<socket_t>(client_socket_handle_)))
	{
		return connection_result::would_block;
	}

#ifdef _WIN32
	const int received = recv(static_cast<socket_t>(client_socket_handle_), static_cast<char*>(buffer), static_cast<int>(capacity), 0);
#else
	const ssize_t received_raw = recv(static_cast<socket_t>(client_socket_handle_), buffer, capacity, 0);
	const int received = (received_raw < 0) ? (int)received_raw : (int)received_raw;
#endif
	if (received == SOCKET_ERR)
	{
		last_socket_error_ = tcp_connection_server_get_last_error();
		if (last_socket_error_ == CONN_RESET_ERR || last_socket_error_ == NOT_CONN_ERR)
		{
			close_client();
		}
		return map_receive_error(last_socket_error_);
	}

	if (received == 0)
	{
		close_client();
		return connection_result::would_block;
	}

	*received_size = static_cast<std::size_t>(received);
	return connection_result::ok;
}

connection_result tcp_connection_server::receive_dmi_frame(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept
{
	std::array<unsigned char, kTcpFramingReceiveChunkSize> chunk = {};
	connection_result client_result;

	if (tcp_framing_try_extract_dmi_frame(receive_buffer_, buffer, capacity, received_size))
	{
		return connection_result::ok;
	}
	if (*received_size > capacity)
	{
		return connection_result::invalid_argument;
	}

	client_result = ensure_client_connected();
	if (client_result != connection_result::ok)
	{
		return client_result;
	}

	if (!tcp_connection_server_is_readable(static_cast<socket_t>(client_socket_handle_)))
	{
		return connection_result::would_block;
	}

	for (;;)
	{
#ifdef _WIN32
		const int received = recv(
			static_cast<socket_t>(client_socket_handle_),
			reinterpret_cast<char*>(chunk.data()),
			static_cast<int>(chunk.size()),
			0);
#else
		const ssize_t received_raw = recv(
			static_cast<socket_t>(client_socket_handle_),
			chunk.data(),
			chunk.size(),
			0);
		const int received = (received_raw < 0) ? (int)received_raw : (int)received_raw;
#endif
		if (received == SOCKET_ERR)
		{
			last_socket_error_ = tcp_connection_server_get_last_error();
			if (last_socket_error_ == CONN_RESET_ERR || last_socket_error_ == NOT_CONN_ERR)
			{
				close_client();
			}
			return map_receive_error(last_socket_error_);
		}

		if (received == 0)
		{
			close_client();
			return connection_result::would_block;
		}

		receive_buffer_.insert(receive_buffer_.end(), chunk.begin(), chunk.begin() + received);
		if (tcp_framing_try_extract_dmi_frame(receive_buffer_, buffer, capacity, received_size))
		{
			return connection_result::ok;
		}
		if (*received_size > capacity)
		{
			return connection_result::invalid_argument;
		}

		if (!tcp_connection_server_is_readable(static_cast<socket_t>(client_socket_handle_)))
		{
			return connection_result::would_block;
		}
	}
}

connection_result tcp_connection_server::receive_ndjson_line(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept
{
	std::array<unsigned char, kTcpFramingReceiveChunkSize> chunk = {};
	connection_result client_result;

	if (tcp_framing_try_extract_ndjson_line(receive_buffer_, buffer, capacity, received_size))
	{
		return connection_result::ok;
	}

	client_result = ensure_client_connected();
	if (client_result != connection_result::ok)
	{
		return client_result;
	}

	if (!tcp_connection_server_is_readable(static_cast<socket_t>(client_socket_handle_)))
	{
		return connection_result::would_block;
	}

	for (;;)
	{
#ifdef _WIN32
		const int received = recv(
			static_cast<socket_t>(client_socket_handle_),
			reinterpret_cast<char*>(chunk.data()),
			static_cast<int>(chunk.size()),
			0);
#else
		const ssize_t received_raw = recv(
			static_cast<socket_t>(client_socket_handle_),
			chunk.data(),
			chunk.size(),
			0);
		const int received = (received_raw < 0) ? (int)received_raw : (int)received_raw;
#endif
		if (received == SOCKET_ERR)
		{
			last_socket_error_ = tcp_connection_server_get_last_error();
			if (last_socket_error_ == CONN_RESET_ERR || last_socket_error_ == NOT_CONN_ERR)
			{
				close_client();
			}
			return map_receive_error(last_socket_error_);
		}

		if (received == 0)
		{
			close_client();
			return connection_result::would_block;
		}

		receive_buffer_.insert(receive_buffer_.end(), chunk.begin(), chunk.begin() + received);
		if (tcp_framing_try_extract_ndjson_line(receive_buffer_, buffer, capacity, received_size))
		{
			return connection_result::ok;
		}

		if (!tcp_connection_server_is_readable(static_cast<socket_t>(client_socket_handle_)))
		{
			return connection_result::would_block;
		}
	}
}

int tcp_connection_server::last_socket_error() const noexcept
{
	return last_socket_error_;
}

bool tcp_connection_server::is_open() const noexcept
{
	return is_open_;
}

bool tcp_connection_server::has_client() const noexcept
{
	return client_socket_handle_ != invalid_socket_handle();
}

tcp_framing_method tcp_connection_server::receive_framing() const noexcept
{
	return receive_framing_method_;
}

tcp_framing_method tcp_connection_server::send_framing() const noexcept
{
	return send_framing_method_;
}

const char* tcp_connection_server::result_string(connection_result result) noexcept
{
	switch (result)
	{
	case connection_result::ok:
		return "ok";
	case connection_result::would_block:
		return "would_block";
	case connection_result::invalid_argument:
		return "invalid_argument";
	case connection_result::not_open:
		return "not_open";
	case connection_result::already_open:
		return "already_open";
	case connection_result::no_default_peer:
		return "no_default_peer";
	case connection_result::startup_failed:
		return "startup_failed";
	case connection_result::socket_failed:
		return "socket_failed";
	case connection_result::bind_failed:
		return "bind_failed";
	case connection_result::send_failed:
		return "send_failed";
	case connection_result::receive_failed:
		return "receive_failed";
	case connection_result::internal_error:
	default:
		return "internal_error";
	}
}