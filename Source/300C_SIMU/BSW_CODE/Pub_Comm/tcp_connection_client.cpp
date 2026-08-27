#include "tcp_connection_client.h"

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

	long s_tcp_connection_client_transport_users = 0;

#ifdef _WIN32
	typedef SOCKET socket_t;
	#define INVALID_SOCKET_VAL INVALID_SOCKET
	#define SOCKET_ERR SOCKET_ERROR
	#define WOULD_BLOCK_ERR WSAEWOULDBLOCK
	#define CONN_RESET_ERR WSAECONNRESET
	#define NOT_CONN_ERR WSAENOTCONN
	#define INVAL_ERR WSAEINVAL
	#define IN_PROGRESS_ERR WSAEWOULDBLOCK
	#define CONN_REFUSED_ERR WSAECONNREFUSED

	int tcp_connection_client_get_last_error() noexcept
	{
		return WSAGetLastError();
	}

	connection_result tcp_connection_client_transport_acquire(int* last_socket_error) noexcept
	{
		WSADATA wsa_data;
		int result = 0;

		if (s_tcp_connection_client_transport_users == 0)
		{
			result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
			if (result != 0)
			{
				if (last_socket_error != nullptr)
				{
					*last_socket_error = tcp_connection_client_get_last_error();
				}
				return connection_result::startup_failed;
			}
		}

		++s_tcp_connection_client_transport_users;
		return connection_result::ok;
	}

	void tcp_connection_client_transport_release() noexcept
	{
		if (s_tcp_connection_client_transport_users <= 0)
		{
			return;
		}

		--s_tcp_connection_client_transport_users;
		if (s_tcp_connection_client_transport_users == 0)
		{
			(void)WSACleanup();
		}
	}

	int tcp_connection_client_set_nonblocking(socket_t socket_handle) noexcept
	{
		unsigned long nonblocking = 1UL;
		if (ioctlsocket(socket_handle, FIONBIO, &nonblocking) != 0)
		{
			return tcp_connection_client_get_last_error();
		}

		return 0;
	}

	int tcp_connection_client_socket_open(socket_t* sock_handle) noexcept
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
			return tcp_connection_client_get_last_error();
		}

		if (setsockopt(socket_handle, SOL_SOCKET, SO_REUSEADDR, (char*)&value, option_length) < 0)
		{
			const int error_code = tcp_connection_client_get_last_error();
			(void)closesocket(socket_handle);
			return error_code;
		}

		if (tcp_connection_client_set_nonblocking(socket_handle) != 0)
		{
			const int error_code = tcp_connection_client_get_last_error();
			(void)closesocket(socket_handle);
			return error_code;
		}

		*sock_handle = socket_handle;
		return 0;
	}

	int tcp_connection_client_socket_bind(socket_t sock_handle, const char* ip, uint16_t port) noexcept
	{
		struct sockaddr_in local_address;

		if (port == 0U)
		{
			/* Port 0 means OS picks — skip bind, return success. */
			return 0;
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
			return tcp_connection_client_get_last_error();
		}

		return 0;
	}

	int tcp_connection_client_socket_connect(socket_t sock_handle, const char* ip, uint16_t port) noexcept
	{
		struct sockaddr_in remote_address;

		if ((ip == nullptr) || ('\0' == ip[0]) || (port == 0U))
		{
			return WSAEINVAL;
		}

		memset(&remote_address, 0, sizeof(remote_address));
		remote_address.sin_family = AF_INET;
		remote_address.sin_port = htons(port);
		if (InetPtonA(AF_INET, ip, &remote_address.sin_addr) != 1)
		{
			return WSAEINVAL;
		}

		if (connect(sock_handle, (struct sockaddr*)&remote_address, sizeof(remote_address)) != 0)
		{
			const int error_code = tcp_connection_client_get_last_error();
			/* Non-blocking connect: EINPROGRESS / WSAEWOULDBLOCK is expected. */
			if (error_code == IN_PROGRESS_ERR)
			{
				return 0;
			}
			return error_code;
		}

		return 0;
	}

	int tcp_connection_client_socket_close(socket_t sock_handle) noexcept
	{
		if (sock_handle == INVALID_SOCKET_VAL)
		{
			return 0;
		}

		if (closesocket(sock_handle) != 0)
		{
			return tcp_connection_client_get_last_error();
		}

		return 0;
	}

	bool tcp_connection_client_is_readable(socket_t sock_handle) noexcept
	{
		fd_set read_set;
		timeval timeout;

		FD_ZERO(&read_set);
		FD_SET(sock_handle, &read_set);
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		return select(0, &read_set, nullptr, nullptr, &timeout) > 0;
	}

	bool tcp_connection_client_is_writable(socket_t sock_handle) noexcept
	{
		fd_set write_set;
		timeval timeout;

		FD_ZERO(&write_set);
		FD_SET(sock_handle, &write_set);
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		return select(0, nullptr, &write_set, nullptr, &timeout) > 0;
	}

#else /* POSIX */

	typedef int socket_t;
	#define INVALID_SOCKET_VAL (-1)
	#define SOCKET_ERR (-1)
	#define WOULD_BLOCK_ERR EAGAIN
	#define CONN_RESET_ERR ECONNRESET
	#define NOT_CONN_ERR ENOTCONN
	#define INVAL_ERR EINVAL
	#define IN_PROGRESS_ERR EINPROGRESS
	#define CONN_REFUSED_ERR ECONNREFUSED

	int tcp_connection_client_get_last_error() noexcept
	{
		return errno;
	}

	connection_result tcp_connection_client_transport_acquire(int* /*last_socket_error*/) noexcept
	{
		/* POSIX: no WSAStartup needed. Reference count for symmetry. */
		++s_tcp_connection_client_transport_users;
		return connection_result::ok;
	}

	void tcp_connection_client_transport_release() noexcept
	{
		if (s_tcp_connection_client_transport_users <= 0)
		{
			return;
		}
		--s_tcp_connection_client_transport_users;
	}

	int tcp_connection_client_set_nonblocking(socket_t socket_handle) noexcept
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

	int tcp_connection_client_socket_open(socket_t* sock_handle) noexcept
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
			return tcp_connection_client_get_last_error();
		}

		if (setsockopt(socket_handle, SOL_SOCKET, SO_REUSEADDR, &value, option_length) < 0)
		{
			const int error_code = tcp_connection_client_get_last_error();
			(void)close(socket_handle);
			return error_code;
		}

		if (tcp_connection_client_set_nonblocking(socket_handle) != 0)
		{
			const int error_code = tcp_connection_client_get_last_error();
			(void)close(socket_handle);
			return error_code;
		}

		*sock_handle = socket_handle;
		return 0;
	}

	int tcp_connection_client_socket_bind(socket_t sock_handle, const char* ip, uint16_t port) noexcept
	{
		struct sockaddr_in local_address;

		if (port == 0U)
		{
			/* Port 0 means OS picks — skip bind, return success. */
			return 0;
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
			return tcp_connection_client_get_last_error();
		}

		return 0;
	}

	int tcp_connection_client_socket_connect(socket_t sock_handle, const char* ip, uint16_t port) noexcept
	{
		struct sockaddr_in remote_address;

		if ((ip == nullptr) || ('\0' == ip[0]) || (port == 0U))
		{
			return EINVAL;
		}

		memset(&remote_address, 0, sizeof(remote_address));
		remote_address.sin_family = AF_INET;
		remote_address.sin_port = htons(port);
		if (inet_pton(AF_INET, ip, &remote_address.sin_addr) != 1)
		{
			return EINVAL;
		}

		if (connect(sock_handle, (struct sockaddr*)&remote_address, sizeof(remote_address)) != 0)
		{
			const int error_code = tcp_connection_client_get_last_error();
			/* Non-blocking connect: EINPROGRESS is expected. */
			if (error_code == EINPROGRESS)
			{
				return 0;
			}
			return error_code;
		}

		return 0;
	}

	int tcp_connection_client_socket_close(socket_t sock_handle) noexcept
	{
		if (sock_handle == INVALID_SOCKET_VAL)
		{
			return 0;
		}

		if (close(sock_handle) != 0)
		{
			return tcp_connection_client_get_last_error();
		}

		return 0;
	}

	bool tcp_connection_client_is_readable(socket_t sock_handle) noexcept
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

	bool tcp_connection_client_is_writable(socket_t sock_handle) noexcept
	{
		fd_set write_set;
		timeval timeout;

		FD_ZERO(&write_set);
		FD_SET(sock_handle, &write_set);
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		/* On POSIX, nfds must be the highest fd + 1 */
		return select(sock_handle + 1, nullptr, &write_set, nullptr, &timeout) > 0;
	}

#endif /* _WIN32 / POSIX */

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

/* ── Class implementation (platform-independent) ── */

connection_result tcp_connection_client::receive_two_byte_len_frame(
	void* buffer,
	std::size_t capacity,
	std::size_t* received_size,
	bool big_endian) noexcept
{
	std::array<unsigned char, kTcpFramingReceiveChunkSize> chunk = {};
	connection_result conn_result;

	if (tcp_framing_try_extract_two_byte_len_frame(receive_buffer_, buffer, capacity, received_size, big_endian))
	{
		return connection_result::ok;
	}
	if (*received_size > capacity)
	{
		return connection_result::invalid_argument;
	}

	conn_result = ensure_connected();
	if (conn_result != connection_result::ok)
	{
		return conn_result;
	}

	if (!tcp_connection_client_is_readable(static_cast<socket_t>(socket_handle_)))
	{
		return connection_result::would_block;
	}

	for (;;)
	{
#ifdef _WIN32
		const int received = recv(
			static_cast<socket_t>(socket_handle_),
			reinterpret_cast<char*>(chunk.data()),
			static_cast<int>(chunk.size()),
			0);
#else
		const ssize_t received_raw = recv(
			static_cast<socket_t>(socket_handle_),
			chunk.data(),
			chunk.size(),
			0);
		const int received = (received_raw < 0) ? (int)received_raw : (int)received_raw;
#endif
		if (received == SOCKET_ERR)
		{
			last_socket_error_ = tcp_connection_client_get_last_error();
			if (last_socket_error_ == CONN_RESET_ERR || last_socket_error_ == NOT_CONN_ERR)
			{
				is_connected_ = false;
			}
			return map_receive_error(last_socket_error_);
		}

		if (received == 0)
		{
			is_connected_ = false;
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

		if (!tcp_connection_client_is_readable(static_cast<socket_t>(socket_handle_)))
		{
			return connection_result::would_block;
		}
	}
}

tcp_connection_client::tcp_connection_client() noexcept
	: socket_handle_(invalid_socket_handle())
	, has_configuration_(false)
	, is_open_(false)
	, is_connected_(false)
	, last_socket_error_(0)
	, local_port_(0U)
	, remote_port_(0U)
	, receive_framing_method_(tcp_framing_method::none)
	, send_framing_method_(tcp_framing_method::none)
{
}

tcp_connection_client::tcp_connection_client(
	const char* remote_ip,
	uint16_t remote_port,
	const char* local_ip,
	uint16_t local_port,
	tcp_framing_method receive_framing, tcp_framing_method send_framing) noexcept
	: tcp_connection_client()
{
	(void)init(remote_ip, remote_port, local_ip, local_port, receive_framing, send_framing);
}

tcp_connection_client::~tcp_connection_client() noexcept
{
	(void)close();
}

tcp_connection_client::tcp_connection_client(tcp_connection_client&& other) noexcept
	: socket_handle_(other.socket_handle_)
	, has_configuration_(other.has_configuration_)
	, is_open_(other.is_open_)
	, is_connected_(other.is_connected_)
	, last_socket_error_(other.last_socket_error_)
	, local_ip_(std::move(other.local_ip_))
	, local_port_(other.local_port_)
	, remote_ip_(std::move(other.remote_ip_))
	, remote_port_(other.remote_port_)
	, receive_framing_method_(other.receive_framing_method_)
	, send_framing_method_(other.send_framing_method_)
	, receive_buffer_(std::move(other.receive_buffer_))
{
	other.reset_state();
}

tcp_connection_client& tcp_connection_client::operator=(tcp_connection_client&& other) noexcept
{
	if (this != &other)
	{
		(void)close();
		socket_handle_ = other.socket_handle_;
		has_configuration_ = other.has_configuration_;
		is_open_ = other.is_open_;
		is_connected_ = other.is_connected_;
		last_socket_error_ = other.last_socket_error_;
		local_ip_ = std::move(other.local_ip_);
		local_port_ = other.local_port_;
		remote_ip_ = std::move(other.remote_ip_);
		remote_port_ = other.remote_port_;
		receive_framing_method_ = other.receive_framing_method_;
			send_framing_method_ = other.send_framing_method_;
		receive_buffer_ = std::move(other.receive_buffer_);
		other.reset_state();
	}

	return *this;
}

void tcp_connection_client::reset_state() noexcept
{
	socket_handle_ = invalid_socket_handle();
	has_configuration_ = false;
	is_open_ = false;
	is_connected_ = false;
	last_socket_error_ = 0;
	local_ip_.clear();
	local_port_ = 0U;
	remote_ip_.clear();
	remote_port_ = 0U;
	receive_framing_method_ = tcp_framing_method::none;
		send_framing_method_ = tcp_framing_method::none;
	receive_buffer_.clear();
}

std::uintptr_t tcp_connection_client::invalid_socket_handle() noexcept
{
	return static_cast<std::uintptr_t>(INVALID_SOCKET_VAL);
}

connection_result tcp_connection_client::init(
	const char* remote_ip,
	uint16_t remote_port,
	const char* local_ip,
	uint16_t local_port,
	tcp_framing_method receive_framing, tcp_framing_method send_framing) noexcept
{
	if (is_open_)
	{
		return connection_result::already_open;
	}

	if ((remote_ip == nullptr) || (remote_ip[0] == '\0') || (remote_port == 0U))
	{
		return connection_result::invalid_argument;
	}

	remote_ip_ = remote_ip;
	remote_port_ = remote_port;
	local_ip_ = ((local_ip != nullptr) && (local_ip[0] != '\0')) ? local_ip : std::string();
	local_port_ = local_port;
	receive_framing_method_ = receive_framing;
	send_framing_method_ = send_framing;
	has_configuration_ = true;
	receive_buffer_.clear();

	return connection_result::ok;
}

connection_result tcp_connection_client::open() noexcept
{
	socket_t sock = INVALID_SOCKET_VAL;
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

	transport_result = tcp_connection_client_transport_acquire(&last_socket_error_);
	if (transport_result != connection_result::ok)
	{
		return transport_result;
	}

	result = tcp_connection_client_socket_open(&sock);
	if (result != 0)
	{
		last_socket_error_ = result;
		tcp_connection_client_transport_release();
		return connection_result::socket_failed;
	}

	/* Optional: bind to a specific local address/port. */
	result = tcp_connection_client_socket_bind(sock, local_ip_.empty() ? nullptr : local_ip_.c_str(), local_port_);
	if (result != 0)
	{
		last_socket_error_ = result;
		(void)tcp_connection_client_socket_close(sock);
		tcp_connection_client_transport_release();
		return connection_result::bind_failed;
	}

	/* Initiate non-blocking connect. EINPROGRESS is expected and handled in ensure_connected(). */
	result = tcp_connection_client_socket_connect(sock, remote_ip_.c_str(), remote_port_);
	if (result != 0)
	{
		last_socket_error_ = result;
		(void)tcp_connection_client_socket_close(sock);
		tcp_connection_client_transport_release();
		if (result == CONN_REFUSED_ERR || result == static_cast<int>(INVAL_ERR))
		{
			return connection_result::socket_failed;
		}
		return connection_result::socket_failed;
	}

	socket_handle_ = static_cast<std::uintptr_t>(sock);
	is_open_ = true;
	is_connected_ = false;
	receive_buffer_.clear();
	return connection_result::ok;
}

connection_result tcp_connection_client::close() noexcept
{
	if (!is_open_)
	{
		return connection_result::not_open;
	}

	last_socket_error_ = tcp_connection_client_socket_close(static_cast<socket_t>(socket_handle_));
	socket_handle_ = invalid_socket_handle();
	is_open_ = false;
	is_connected_ = false;
	tcp_connection_client_transport_release();

	return (last_socket_error_ == 0) ? connection_result::ok : connection_result::socket_failed;
}

connection_result tcp_connection_client::ensure_connected() noexcept
{
	int so_error = 0;
	socklen_t so_error_len = sizeof(so_error);

	if (!is_open_)
	{
		return connection_result::not_open;
	}

	if (is_connected_)
	{
		return connection_result::ok;
	}

	/* Check if the non-blocking connect has completed. */
	if (!tcp_connection_client_is_writable(static_cast<socket_t>(socket_handle_)))
	{
		return connection_result::would_block;
	}

	/* Socket is writable — check SO_ERROR to see whether connect succeeded. */
	if (getsockopt(static_cast<socket_t>(socket_handle_), SOL_SOCKET, SO_ERROR,
#ifdef _WIN32
		reinterpret_cast<char*>(&so_error), &so_error_len) < 0)
#else
		&so_error, &so_error_len) < 0)
#endif
	{
		last_socket_error_ = tcp_connection_client_get_last_error();
		return connection_result::socket_failed;
	}

	if (so_error != 0)
	{
		last_socket_error_ = so_error;
		if (so_error == CONN_REFUSED_ERR)
		{
			return connection_result::socket_failed;
		}
		return connection_result::socket_failed;
	}

	is_connected_ = true;
	return connection_result::ok;
}

connection_result tcp_connection_client::send(const void* data, std::size_t size) noexcept
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

	const unsigned char* send_buffer = raw_data;
	std::size_t send_size = raw_size;
	std::size_t sent_total = 0U;
	connection_result conn_result;

	conn_result = ensure_connected();
	if (conn_result != connection_result::ok)
	{
		return conn_result;
	}

	while (sent_total < send_size)
	{
#ifdef _WIN32
		const int sent_now = ::send(
			static_cast<socket_t>(socket_handle_),
			reinterpret_cast<const char*>(send_buffer + sent_total),
			static_cast<int>(send_size - sent_total),
			0);
#else
		const ssize_t sent_now_raw = ::send(
			static_cast<socket_t>(socket_handle_),
			send_buffer + sent_total,
			send_size - sent_total,
			0);
		const int sent_now = (sent_now_raw < 0) ? -1 : (int)sent_now_raw;
#endif
		if (sent_now == SOCKET_ERR)
		{
			last_socket_error_ = tcp_connection_client_get_last_error();
			if (last_socket_error_ == CONN_RESET_ERR || last_socket_error_ == NOT_CONN_ERR)
			{
				is_connected_ = false;
			}
			return map_send_error(last_socket_error_);
		}

		if (sent_now == 0)
		{
			is_connected_ = false;
			return connection_result::send_failed;
		}

		sent_total += static_cast<std::size_t>(sent_now);
	}

	return connection_result::ok;
}

connection_result tcp_connection_client::receive(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept
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

	return receive_raw(buffer, capacity, received_size);
}

connection_result tcp_connection_client::receive_raw(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept
{
	connection_result conn_result = ensure_connected();
	if (conn_result != connection_result::ok)
	{
		return conn_result;
	}

	if (!tcp_connection_client_is_readable(static_cast<socket_t>(socket_handle_)))
	{
		return connection_result::would_block;
	}

#ifdef _WIN32
	const int received = recv(static_cast<socket_t>(socket_handle_), static_cast<char*>(buffer), static_cast<int>(capacity), 0);
#else
	const ssize_t received_raw = recv(static_cast<socket_t>(socket_handle_), buffer, capacity, 0);
	const int received = (received_raw < 0) ? (int)received_raw : (int)received_raw;
#endif
	if (received == SOCKET_ERR)
	{
		last_socket_error_ = tcp_connection_client_get_last_error();
		if (last_socket_error_ == CONN_RESET_ERR || last_socket_error_ == NOT_CONN_ERR)
		{
			is_connected_ = false;
		}
		return map_receive_error(last_socket_error_);
	}

	if (received == 0)
	{
		is_connected_ = false;
		return connection_result::would_block;
	}

	*received_size = static_cast<std::size_t>(received);
	return connection_result::ok;
}

connection_result tcp_connection_client::receive_dmi_frame(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept
{
	std::array<unsigned char, kTcpFramingReceiveChunkSize> chunk = {};
	connection_result conn_result;

	if (tcp_framing_try_extract_dmi_frame(receive_buffer_, buffer, capacity, received_size))
	{
		return connection_result::ok;
	}
	if (*received_size > capacity)
	{
		return connection_result::invalid_argument;
	}

	conn_result = ensure_connected();
	if (conn_result != connection_result::ok)
	{
		return conn_result;
	}

	if (!tcp_connection_client_is_readable(static_cast<socket_t>(socket_handle_)))
	{
		return connection_result::would_block;
	}

	for (;;)
	{
#ifdef _WIN32
		const int received = recv(
			static_cast<socket_t>(socket_handle_),
			reinterpret_cast<char*>(chunk.data()),
			static_cast<int>(chunk.size()),
			0);
#else
		const ssize_t received_raw = recv(
			static_cast<socket_t>(socket_handle_),
			chunk.data(),
			chunk.size(),
			0);
		const int received = (received_raw < 0) ? (int)received_raw : (int)received_raw;
#endif
		if (received == SOCKET_ERR)
		{
			last_socket_error_ = tcp_connection_client_get_last_error();
			if (last_socket_error_ == CONN_RESET_ERR || last_socket_error_ == NOT_CONN_ERR)
			{
				is_connected_ = false;
			}
			return map_receive_error(last_socket_error_);
		}

		if (received == 0)
		{
			is_connected_ = false;
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

		if (!tcp_connection_client_is_readable(static_cast<socket_t>(socket_handle_)))
		{
			return connection_result::would_block;
		}
	}
}

int tcp_connection_client::last_socket_error() const noexcept
{
	return last_socket_error_;
}

bool tcp_connection_client::is_open() const noexcept
{
	return is_open_;
}

bool tcp_connection_client::is_connected() const noexcept
{
	return is_connected_;
}

tcp_framing_method tcp_connection_client::receive_framing() const noexcept
{
	return receive_framing_method_;
}

tcp_framing_method tcp_connection_client::send_framing() const noexcept
{
	return send_framing_method_;
}

const char* tcp_connection_client::result_string(connection_result result) noexcept
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
