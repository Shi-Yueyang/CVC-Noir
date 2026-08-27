#include "udp_connection.h"

#include <limits.h>
#include <string.h>

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
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

#include "../../logging/logger.h"

namespace
{
	long s_udp_connection_transport_users = 0;

	Logger::Ptr get_udp_log()
	{
		return Logger::get("udp_connection");
	}

#ifdef _WIN32
	typedef SOCKET socket_t;
	#define INVALID_SOCKET_VAL INVALID_SOCKET
	#define WOULD_BLOCK_ERR WSAEWOULDBLOCK
	#define INVAL_ERR WSAEINVAL

	int udp_connection_get_last_error() noexcept
	{
		return WSAGetLastError();
	}

	connection_result udp_connection_transport_acquire(int* last_socket_error) noexcept
	{
		WSADATA wsa_data;
		int result = 0;

		if (s_udp_connection_transport_users == 0)
		{
			result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
			if (result != 0)
			{
				if (last_socket_error != nullptr)
				{
					*last_socket_error = udp_connection_get_last_error();
				}
				return connection_result::startup_failed;
			}
		}

		++s_udp_connection_transport_users;
		return connection_result::ok;
	}

	void udp_connection_transport_release() noexcept
	{
		if (s_udp_connection_transport_users <= 0)
		{
			return;
		}

		--s_udp_connection_transport_users;
		if (s_udp_connection_transport_users == 0)
		{
			(void)WSACleanup();
		}
	}

	int udp_connection_socket_open(socket_t* sock_handle) noexcept
	{
		socket_t socket_handle = INVALID_SOCKET_VAL;
		int value = 1;
		int option_length = sizeof(value);
		unsigned long nonblocking = 1UL;

		if (sock_handle == nullptr)
		{
			return WSAEINVAL;
		}

		socket_handle = socket(AF_INET, SOCK_DGRAM, 0);
		if (socket_handle == INVALID_SOCKET_VAL)
		{
			return udp_connection_get_last_error();
		}

		if (setsockopt(socket_handle, SOL_SOCKET, SO_REUSEADDR, (char*)&value, option_length) < 0)
		{
			const int error_code = udp_connection_get_last_error();
			(void)closesocket(socket_handle);
			return error_code;
		}

		if (ioctlsocket(socket_handle, FIONBIO, &nonblocking) != 0)
		{
			const int error_code = udp_connection_get_last_error();
			(void)closesocket(socket_handle);
			return error_code;
		}

		*sock_handle = socket_handle;
		return 0;
	}

	int udp_connection_socket_bind(socket_t sock_handle, const char* ip, uint16_t port) noexcept
	{
		struct sockaddr_in local_address;

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
			return udp_connection_get_last_error();
		}

		return 0;
	}

	int udp_connection_socket_close(socket_t sock_handle) noexcept
	{
		if (sock_handle == INVALID_SOCKET_VAL)
		{
			return 0;
		}

		if (closesocket(sock_handle) != 0)
		{
			return udp_connection_get_last_error();
		}

		return 0;
	}

	int udp_connection_socket_send_to(
		socket_t sock_handle,
		const char* ip,
		uint16_t port,
		const unsigned char* data,
		int size) noexcept
	{
		struct sockaddr_in remote_address;

		memset(&remote_address, 0, sizeof(remote_address));
		remote_address.sin_family = AF_INET;
		remote_address.sin_port = htons(port);
		if (InetPtonA(AF_INET, ip, &remote_address.sin_addr) != 1)
		{
			return SOCKET_ERROR;
		}

		return sendto(sock_handle, (const char*)data, size, 0, (struct sockaddr*)&remote_address, sizeof(remote_address));
	}

	int udp_connection_socket_receive(socket_t sock_handle, unsigned char* buffer, int capacity) noexcept
	{
		struct sockaddr_in sender_address;
		int sender_length = sizeof(sender_address);

		memset(&sender_address, 0, sizeof(sender_address));
		return recvfrom(sock_handle, (char*)buffer, capacity, 0, (struct sockaddr*)&sender_address, &sender_length);
	}

#else /* POSIX */

	typedef int socket_t;
	#define INVALID_SOCKET_VAL (-1)
	#define WOULD_BLOCK_ERR EAGAIN
	#define INVAL_ERR EINVAL

	int udp_connection_get_last_error() noexcept
	{
		return errno;
	}

	connection_result udp_connection_transport_acquire(int* /*last_socket_error*/) noexcept
	{
		++s_udp_connection_transport_users;
		return connection_result::ok;
	}

	void udp_connection_transport_release() noexcept
	{
		if (s_udp_connection_transport_users <= 0)
		{
			return;
		}
		--s_udp_connection_transport_users;
	}

	int udp_connection_socket_open(socket_t* sock_handle) noexcept
	{
		socket_t socket_handle = INVALID_SOCKET_VAL;
		int value = 1;
		socklen_t option_length = sizeof(value);

		if (sock_handle == nullptr)
		{
			return EINVAL;
		}

		socket_handle = socket(AF_INET, SOCK_DGRAM, 0);
		if (socket_handle == INVALID_SOCKET_VAL)
		{
			return udp_connection_get_last_error();
		}

		if (setsockopt(socket_handle, SOL_SOCKET, SO_REUSEADDR, &value, option_length) < 0)
		{
			const int error_code = udp_connection_get_last_error();
			(void)close(socket_handle);
			return error_code;
		}

		/* Set non-blocking */
		int flags = fcntl(socket_handle, F_GETFL, 0);
		if (flags < 0 || fcntl(socket_handle, F_SETFL, flags | O_NONBLOCK) < 0)
		{
			const int error_code = udp_connection_get_last_error();
			(void)close(socket_handle);
			return error_code;
		}

		*sock_handle = socket_handle;
		return 0;
	}

	int udp_connection_socket_bind(socket_t sock_handle, const char* ip, uint16_t port) noexcept
	{
		struct sockaddr_in local_address;

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
			return udp_connection_get_last_error();
		}

		return 0;
	}

	int udp_connection_socket_close(socket_t sock_handle) noexcept
	{
		if (sock_handle == INVALID_SOCKET_VAL)
		{
			return 0;
		}

		if (close(sock_handle) != 0)
		{
			return udp_connection_get_last_error();
		}

		return 0;
	}

	int udp_connection_socket_send_to(
		socket_t sock_handle,
		const char* ip,
		uint16_t port,
		const unsigned char* data,
		int size) noexcept
	{
		struct sockaddr_in remote_address;

		memset(&remote_address, 0, sizeof(remote_address));
		remote_address.sin_family = AF_INET;
		remote_address.sin_port = htons(port);
		if (inet_pton(AF_INET, ip, &remote_address.sin_addr) != 1)
		{
			return -1;
		}

		return (int)sendto(sock_handle, data, (size_t)size, 0, (struct sockaddr*)&remote_address, sizeof(remote_address));
	}

	int udp_connection_socket_receive(socket_t sock_handle, unsigned char* buffer, int capacity) noexcept
	{
		struct sockaddr_in sender_address;
		socklen_t sender_length = sizeof(sender_address);

		memset(&sender_address, 0, sizeof(sender_address));
		return (int)recvfrom(sock_handle, buffer, (size_t)capacity, 0, (struct sockaddr*)&sender_address, &sender_length);
	}

#endif /* _WIN32 / POSIX */

	bool udp_connection_is_usable_address(const char* ip, uint16_t port) noexcept
	{
		return (ip != nullptr) && ('\0' != ip[0]) && (0U != port);
	}
}

udp_connection::udp_connection() noexcept
	: socket_handle_(invalid_socket_handle())
	, has_configuration_(false)
	, is_open_(false)
	, has_default_peer_(false)
	, last_socket_error_(0)
	, local_port_(0U)
	, default_peer_port_(0U)
{
}

	udp_connection::udp_connection(
	const char* local_ip,
	uint16_t local_port,
	const char* default_peer_ip,
	uint16_t default_peer_port) noexcept
	: udp_connection()
{
		if ((local_ip != nullptr) && ('\0' != local_ip[0]))
		{
			local_ip_ = local_ip;
		}
		local_port_ = local_port;
		has_configuration_ = true;
		if (((default_peer_ip != nullptr) && ('\0' != default_peer_ip[0])) || (0U != default_peer_port))
		{
			if (udp_connection_is_usable_address(default_peer_ip, default_peer_port))
			{
				default_peer_ip_ = default_peer_ip;
				default_peer_port_ = default_peer_port;
				has_default_peer_ = true;
			}
		}
}

udp_connection::~udp_connection() noexcept
{
	(void)close();
}

udp_connection::udp_connection(udp_connection&& other) noexcept
	: socket_handle_(other.socket_handle_)
	, has_configuration_(other.has_configuration_)
	, is_open_(other.is_open_)
	, has_default_peer_(other.has_default_peer_)
	, last_socket_error_(other.last_socket_error_)
	, local_ip_(std::move(other.local_ip_))
	, local_port_(other.local_port_)
	, default_peer_ip_(std::move(other.default_peer_ip_))
	, default_peer_port_(other.default_peer_port_)
{
	other.reset_state();
}

udp_connection& udp_connection::operator=(udp_connection&& other) noexcept
{
	if (this != &other)
	{
		(void)close();
		socket_handle_ = other.socket_handle_;
		has_configuration_ = other.has_configuration_;
		is_open_ = other.is_open_;
		has_default_peer_ = other.has_default_peer_;
		last_socket_error_ = other.last_socket_error_;
		local_ip_ = std::move(other.local_ip_);
		local_port_ = other.local_port_;
		default_peer_ip_ = std::move(other.default_peer_ip_);
		default_peer_port_ = other.default_peer_port_;
		other.reset_state();
	}

	return *this;
}

void udp_connection::reset_state() noexcept
{
	socket_handle_ = invalid_socket_handle();
	has_configuration_ = false;
	is_open_ = false;
	has_default_peer_ = false;
	local_ip_.clear();
	local_port_ = 0U;
	default_peer_ip_.clear();
	default_peer_port_ = 0U;
}

std::uintptr_t udp_connection::invalid_socket_handle() noexcept
{
	return static_cast<std::uintptr_t>(INVALID_SOCKET_VAL);
}

connection_result udp_connection::open() noexcept
{
	socket_t socket_handle = INVALID_SOCKET_VAL;
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

	if ((!default_peer_ip_.empty()) || (default_peer_port_ != 0U))
	{
		if (!udp_connection_is_usable_address(default_peer_ip_.c_str(), default_peer_port_))
		{
			return connection_result::invalid_argument;
		}
	}

	transport_result = udp_connection_transport_acquire(&last_socket_error_);
	if (transport_result != connection_result::ok)
	{
		return transport_result;
	}

	result = udp_connection_socket_open(&socket_handle);
	if (result != 0)
	{
		last_socket_error_ = result;
		udp_connection_transport_release();
		return connection_result::socket_failed;
	}

	result = udp_connection_socket_bind(socket_handle, local_ip_.empty() ? nullptr : local_ip_.c_str(), local_port_);
	if (result != 0)
	{
		last_socket_error_ = result;
		(void)udp_connection_socket_close(socket_handle);
		udp_connection_transport_release();
		return connection_result::bind_failed;
	}

	socket_handle_ = static_cast<std::uintptr_t>(socket_handle);
	is_open_ = true;
	has_default_peer_ = !default_peer_ip_.empty() && (default_peer_port_ != 0U);
	return connection_result::ok;
}

connection_result udp_connection::init(
	const char* local_ip,
	uint16_t local_port,
	const char* default_peer_ip,
	uint16_t default_peer_port) noexcept
{
	if (is_open_)
	{
		return connection_result::already_open;
	}

	last_socket_error_ = 0;
	reset_state();
	if ((local_ip != nullptr) && ('\0' != local_ip[0]))
	{
		local_ip_ = local_ip;
	}
	local_port_ = local_port;
	has_configuration_ = true;
	if (((default_peer_ip != nullptr) && ('\0' != default_peer_ip[0])) || (0U != default_peer_port))
	{
		if (!udp_connection_is_usable_address(default_peer_ip, default_peer_port))
		{
			return connection_result::invalid_argument;
		}
		default_peer_ip_ = default_peer_ip;
		default_peer_port_ = default_peer_port;
		has_default_peer_ = true;
	}

	return open();
}

connection_result udp_connection::set_default_peer(const char* ip, uint16_t port) noexcept
{
	if (!udp_connection_is_usable_address(ip, port))
	{
		return connection_result::invalid_argument;
	}

	default_peer_ip_ = ip;
	default_peer_port_ = port;
	has_default_peer_ = true;
	return connection_result::ok;
}

connection_result udp_connection::send(const void* data, std::size_t size) noexcept
{
	if (!has_default_peer_)
	{
		return connection_result::no_default_peer;
	}

	return send_to(default_peer_ip_.c_str(), default_peer_port_, data, size);
}

	connection_result udp_connection::send_to(const char* ip, uint16_t port, const void* data, std::size_t size) noexcept
{
	socket_t socket_handle = static_cast<socket_t>(socket_handle_);
	int result = 0;

	if (data == nullptr)
	{
		return connection_result::invalid_argument;
	}
	if (0U == size)
	{
		return connection_result::ok;
	}
	if (!is_open_)
	{
		return connection_result::not_open;
	}

	if (!udp_connection_is_usable_address(ip, port))
	{
		return connection_result::invalid_argument;
	}

	if (size > (std::size_t)INT_MAX)
	{
		return connection_result::invalid_argument;
	}

	result = udp_connection_socket_send_to(socket_handle, ip, port, (const unsigned char*)data, (int)size);
	if (result < 0)
	{
		last_socket_error_ = udp_connection_get_last_error();
		get_udp_log()->warn("send_to local=%s:%u peer=%s:%u len=%zu err=%d",
			local_ip_.empty() ? "0.0.0.0" : local_ip_.c_str(),
			static_cast<unsigned int>(local_port_),
			ip != nullptr ? ip : "",
			static_cast<unsigned int>(port),
			size,
			last_socket_error_);
		return connection_result::send_failed;
	}

	return connection_result::ok;
}

connection_result udp_connection::receive(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept
{
	socket_t socket_handle = static_cast<socket_t>(socket_handle_);
	int result = 0;

	if ((buffer == nullptr) || (received_size == nullptr) || (0U == capacity))
	{
		return connection_result::invalid_argument;
	}

	if (!is_open_)
	{
		*received_size = 0U;
		return connection_result::not_open;
	}

	if (capacity > (std::size_t)INT_MAX)
	{
		*received_size = 0U;
		return connection_result::invalid_argument;
	}

	result = udp_connection_socket_receive(socket_handle, (unsigned char*)buffer, (int)capacity);
	if (result > 0)
	{
		*received_size = (std::size_t)result;
		return connection_result::ok;
	}

	*received_size = 0U;
	last_socket_error_ = udp_connection_get_last_error();
	if (last_socket_error_ == WOULD_BLOCK_ERR)
	{
		return connection_result::would_block;
	}

	return connection_result::receive_failed;
}

int udp_connection::last_socket_error() const noexcept
{
	return last_socket_error_;
}

connection_result udp_connection::close() noexcept
{
	socket_t socket_handle = static_cast<socket_t>(socket_handle_);
	int result = 0;

	if (!is_open_)
	{
		return connection_result::ok;
	}

	result = udp_connection_socket_close(socket_handle);
	if (result != 0)
	{
		last_socket_error_ = result;
	}

	socket_handle_ = invalid_socket_handle();
	is_open_ = false;
	udp_connection_transport_release();

	return (result == 0) ? connection_result::ok : connection_result::internal_error;
}

bool udp_connection::is_open() const noexcept
{
	return is_open_;
}

const char* udp_connection::result_string(connection_result result) noexcept
{
	switch (result)
	{
	case connection_result::ok:
		return "ok";
	case connection_result::would_block:
		return "would block";
	case connection_result::invalid_argument:
		return "invalid argument";
	case connection_result::not_open:
		return "not open";
	case connection_result::already_open:
		return "already open";
	case connection_result::no_default_peer:
		return "no default peer";
	case connection_result::startup_failed:
		return "startup failed";
	case connection_result::socket_failed:
		return "socket creation failed";
	case connection_result::bind_failed:
		return "bind failed";
	case connection_result::send_failed:
		return "send failed";
	case connection_result::receive_failed:
		return "receive failed";
	case connection_result::internal_error:
		return "internal error";
	default:
		return "unknown";
	}
}