#include "serial_connection.h"

#include <limits.h>
#include <string.h>

#include <string>
#include <utility>

#ifdef _WIN32
#include <Windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace
{
	std::string normalize_port_name(const char* port_name)
	{
		if ((port_name == nullptr) || (port_name[0] == '\0'))
		{
			return std::string();
		}

#ifdef _WIN32
		if ((port_name[0] == '\\')
			&& (port_name[1] == '\\')
			&& (port_name[2] == '.')
			&& (port_name[3] == '\\'))
		{
			return std::string(port_name);
		}

		return std::string("\\\\.\\") + port_name;
#else
		/* On Linux, use device path as-is (e.g. /dev/ttyUSB0, /dev/ttyS0) */
		return std::string(port_name);
#endif
	}

	uint32_t serial_connection_get_last_error() noexcept
	{
#ifdef _WIN32
		return static_cast<uint32_t>(GetLastError());
#else
		return static_cast<uint32_t>(errno);
#endif
	}

#ifdef _WIN32

	BYTE serial_connection_parse_stop_bits(uint8_t stop_bits) noexcept
	{
		switch (stop_bits)
		{
		case 2U:
			return TWOSTOPBITS;
		case 1U:
		default:
			return ONESTOPBIT;
		}
	}

	void serial_connection_apply_parity(DCB* dcb, uint8_t parity) noexcept
	{
		if (dcb == nullptr)
		{
			return;
		}

		switch (parity)
		{
		case 1U:
			dcb->Parity = ODDPARITY;
			dcb->fParity = TRUE;
			break;
		case 2U:
			dcb->Parity = EVENPARITY;
			dcb->fParity = TRUE;
			break;
		default:
			dcb->Parity = NOPARITY;
			dcb->fParity = FALSE;
			break;
		}
	}

	bool serial_connection_configure(
		HANDLE handle,
		uint32_t baud_rate,
		uint8_t data_bits,
		uint8_t stop_bits,
		uint8_t parity,
		uint8_t rts_cts,
		uint32_t read_total_timeout_ms,
		uint32_t* last_error) noexcept
	{
		DCB dcb = { 0 };
		COMMTIMEOUTS timeouts = { 0 };

		dcb.DCBlength = sizeof(dcb);
		if (!GetCommState(handle, &dcb))
		{
			if (last_error != nullptr)
			{
				*last_error = serial_connection_get_last_error();
			}
			return false;
		}

		dcb.BaudRate = static_cast<DWORD>(baud_rate == 0U ? 115200U : baud_rate);
		dcb.ByteSize = static_cast<BYTE>(data_bits == 0U ? 8U : data_bits);
		dcb.StopBits = serial_connection_parse_stop_bits(stop_bits);
		serial_connection_apply_parity(&dcb, parity);
		dcb.fBinary = TRUE;
		dcb.fOutxCtsFlow = rts_cts ? TRUE : FALSE;
		dcb.fRtsControl = rts_cts ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE;
		dcb.fOutxDsrFlow = FALSE;
		dcb.fDtrControl = DTR_CONTROL_ENABLE;
		dcb.fOutX = FALSE;
		dcb.fInX = FALSE;

		if (!SetCommState(handle, &dcb))
		{
			if (last_error != nullptr)
			{
				*last_error = serial_connection_get_last_error();
			}
			return false;
		}

		timeouts.ReadIntervalTimeout = MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier = 0U;
		timeouts.ReadTotalTimeoutConstant = static_cast<DWORD>(read_total_timeout_ms);
		timeouts.WriteTotalTimeoutMultiplier = 0U;
		timeouts.WriteTotalTimeoutConstant = static_cast<DWORD>(read_total_timeout_ms);
		if (!SetCommTimeouts(handle, &timeouts))
		{
			if (last_error != nullptr)
			{
				*last_error = serial_connection_get_last_error();
			}
			return false;
		}

		if (!SetupComm(handle, 4096U, 4096U))
		{
			if (last_error != nullptr)
			{
				*last_error = serial_connection_get_last_error();
			}
			return false;
		}

		(void)PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
		return true;
	}

#else /* POSIX termios implementation */

	static speed_t serial_connection_baud_to_speed(uint32_t baud_rate) noexcept
	{
		switch (baud_rate)
		{
		case 0U:     return B115200;
		case 50U:    return B50;
		case 75U:    return B75;
		case 110U:   return B110;
		case 134U:   return B134;
		case 150U:   return B150;
		case 200U:   return B200;
		case 300U:   return B300;
		case 600U:   return B600;
		case 1200U:  return B1200;
		case 1800U:  return B1800;
		case 2400U:  return B2400;
		case 4800U:  return B4800;
		case 9600U:  return B9600;
		case 19200U: return B19200;
		case 38400U: return B38400;
		case 57600U: return B57600;
		case 115200U: return B115200;
		case 230400U: return B230400;
		case 460800U: return B460800;
		case 500000U: return B500000;
		case 576000U: return B576000;
		case 921600U: return B921600;
		case 1000000U: return B1000000;
		case 1152000U: return B1152000;
		case 1500000U: return B1500000;
		case 2000000U: return B2000000;
		case 2500000U: return B2500000;
		case 3000000U: return B3000000;
		default:      return B115200;
		}
	}

	bool serial_connection_configure(
		int fd,
		uint32_t baud_rate,
		uint8_t data_bits,
		uint8_t stop_bits,
		uint8_t parity,
		uint8_t rts_cts,
		uint32_t read_total_timeout_ms,
		uint32_t* last_error) noexcept
	{
		struct termios tty;
		memset(&tty, 0, sizeof(tty));

		if (tcgetattr(fd, &tty) != 0)
		{
			if (last_error != nullptr)
			{
				*last_error = static_cast<uint32_t>(errno);
			}
			return false;
		}

		/* Baud rate */
		speed_t speed = serial_connection_baud_to_speed(baud_rate == 0U ? 115200U : baud_rate);
		cfsetispeed(&tty, speed);
		cfsetospeed(&tty, speed);

		/* Data bits: 5,6,7,8 */
		tty.c_cflag &= ~CSIZE;
		switch (data_bits == 0U ? 8U : data_bits)
		{
		case 5U: tty.c_cflag |= CS5; break;
		case 6U: tty.c_cflag |= CS6; break;
		case 7U: tty.c_cflag |= CS7; break;
		case 8U:
		default: tty.c_cflag |= CS8; break;
		}

		/* Stop bits */
		if (stop_bits == 2U)
		{
			tty.c_cflag |= CSTOPB;
		}
		else
		{
			tty.c_cflag &= ~CSTOPB;
		}

		/* Parity */
		switch (parity)
		{
		case 1U: /* odd */
			tty.c_cflag |= PARENB;
			tty.c_cflag |= PARODD;
			break;
		case 2U: /* even */
			tty.c_cflag |= PARENB;
			tty.c_cflag &= ~PARODD;
			break;
		default: /* none */
			tty.c_cflag &= ~PARENB;
			break;
		}

		/* RTS/CTS flow control */
		if (rts_cts)
		{
			tty.c_cflag |= CRTSCTS;
		}
		else
		{
			tty.c_cflag &= ~CRTSCTS;
		}

		/* Raw mode, no canonical processing */
		tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
		tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR);
		tty.c_oflag &= ~OPOST;

		/* Enable receiver, ignore modem control lines */
		tty.c_cflag |= (CLOCAL | CREAD);

		/* Read timeout: VMIN=0 VTIME=timeout in 0.1s units */
		tty.c_cc[VMIN] = 0;
		tty.c_cc[VTIME] = static_cast<cc_t>((read_total_timeout_ms + 99U) / 100U);
		if (tty.c_cc[VTIME] == 0 && read_total_timeout_ms > 0U)
		{
			tty.c_cc[VTIME] = 1; /* at least 100ms */
		}

		if (tcsetattr(fd, TCSANOW, &tty) != 0)
		{
			if (last_error != nullptr)
			{
				*last_error = static_cast<uint32_t>(errno);
			}
			return false;
		}

		/* Flush buffers */
		tcflush(fd, TCIOFLUSH);

		return true;
	}

#endif /* _WIN32 / POSIX */
}

serial_connection::serial_connection() noexcept
	: handle_(invalid_handle())
	, has_configuration_(false)
	, is_open_(false)
	, last_error_(0U)
	, baud_rate_(0U)
	, data_bits_(0U)
	, stop_bits_(0U)
	, parity_(0U)
	, rts_cts_(0U)
	, read_total_timeout_ms_(0U)
{
}

serial_connection::serial_connection(
	const char* port_name,
	uint32_t baud_rate,
	uint8_t data_bits,
	uint8_t stop_bits,
	uint8_t parity,
	uint8_t rts_cts,
	uint32_t read_total_timeout_ms) noexcept
	: serial_connection()
{
	(void)init(port_name, baud_rate, data_bits, stop_bits, parity, rts_cts, read_total_timeout_ms);
}

serial_connection::~serial_connection() noexcept
{
	(void)close();
}

serial_connection::serial_connection(serial_connection&& other) noexcept
	: handle_(other.handle_)
	, has_configuration_(other.has_configuration_)
	, is_open_(other.is_open_)
	, last_error_(other.last_error_)
	, port_name_(std::move(other.port_name_))
	, baud_rate_(other.baud_rate_)
	, data_bits_(other.data_bits_)
	, stop_bits_(other.stop_bits_)
	, parity_(other.parity_)
	, rts_cts_(other.rts_cts_)
	, read_total_timeout_ms_(other.read_total_timeout_ms_)
{
	other.reset_state();
}

serial_connection& serial_connection::operator=(serial_connection&& other) noexcept
{
	if (this != &other)
	{
		(void)close();
		handle_ = other.handle_;
		has_configuration_ = other.has_configuration_;
		is_open_ = other.is_open_;
		last_error_ = other.last_error_;
		port_name_ = std::move(other.port_name_);
		baud_rate_ = other.baud_rate_;
		data_bits_ = other.data_bits_;
		stop_bits_ = other.stop_bits_;
		parity_ = other.parity_;
		rts_cts_ = other.rts_cts_;
		read_total_timeout_ms_ = other.read_total_timeout_ms_;
		other.reset_state();
	}

	return *this;
}

connection_result serial_connection::init(
	const char* port_name,
	uint32_t baud_rate,
	uint8_t data_bits,
	uint8_t stop_bits,
	uint8_t parity,
	uint8_t rts_cts,
	uint32_t read_total_timeout_ms) noexcept
{
	if (is_open_)
	{
		return connection_result::already_open;
	}

	port_name_ = normalize_port_name(port_name);
	if (port_name_.empty())
	{
		return connection_result::invalid_argument;
	}

	baud_rate_ = baud_rate;
	data_bits_ = data_bits;
	stop_bits_ = stop_bits;
	parity_ = parity;
	rts_cts_ = rts_cts;
	read_total_timeout_ms_ = read_total_timeout_ms;
	has_configuration_ = true;
	last_error_ = 0U;
	return connection_result::ok;
}

connection_result serial_connection::open() noexcept
{
	if (is_open_)
	{
		return connection_result::already_open;
	}

	last_error_ = 0U;
	if (!has_configuration_)
	{
		return connection_result::invalid_argument;
	}

#ifdef _WIN32
	HANDLE handle = CreateFileA(
		port_name_.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr);
	if (handle == INVALID_HANDLE_VALUE)
	{
		last_error_ = serial_connection_get_last_error();
		return connection_result::internal_error;
	}

	if (!serial_connection_configure(handle, baud_rate_, data_bits_, stop_bits_, parity_, rts_cts_, read_total_timeout_ms_, &last_error_))
	{
		(void)CloseHandle(handle);
		return connection_result::internal_error;
	}

	handle_ = reinterpret_cast<std::uintptr_t>(handle);
#else
	int fd = ::open(port_name_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0)
	{
		last_error_ = serial_connection_get_last_error();
		return connection_result::internal_error;
	}

	if (!serial_connection_configure(fd, baud_rate_, data_bits_, stop_bits_, parity_, rts_cts_, read_total_timeout_ms_, &last_error_))
	{
		::close(fd);
		return connection_result::internal_error;
	}

	handle_ = static_cast<std::uintptr_t>(fd);
#endif

	is_open_ = true;
	return connection_result::ok;
}

connection_result serial_connection::close() noexcept
{
	if (!is_open_)
	{
		return connection_result::ok;
	}

#ifdef _WIN32
	if (!CloseHandle(reinterpret_cast<HANDLE>(handle_)))
	{
		last_error_ = serial_connection_get_last_error();
		return connection_result::internal_error;
	}
#else
	if (::close(static_cast<int>(handle_)) != 0)
	{
		last_error_ = serial_connection_get_last_error();
		return connection_result::internal_error;
	}
#endif

	handle_ = invalid_handle();
	is_open_ = false;
	last_error_ = 0U;
	return connection_result::ok;
}

connection_result serial_connection::send(const void* data, std::size_t size) noexcept
{
	if (!is_open_)
	{
		return connection_result::not_open;
	}

	if ((data == nullptr) || (size == 0U) || (size > static_cast<std::size_t>(INT_MAX)))
	{
		return connection_result::invalid_argument;
	}

#ifdef _WIN32
	DWORD bytes_written = 0U;
	if (!WriteFile(reinterpret_cast<HANDLE>(handle_), data, static_cast<DWORD>(size), &bytes_written, nullptr))
	{
		last_error_ = serial_connection_get_last_error();
		return connection_result::send_failed;
	}

	if (bytes_written != static_cast<DWORD>(size))
	{
		return connection_result::send_failed;
	}
#else
	ssize_t bytes_written = ::write(static_cast<int>(handle_), data, size);
	if (bytes_written < 0)
	{
		last_error_ = serial_connection_get_last_error();
		return connection_result::send_failed;
	}

	if (static_cast<std::size_t>(bytes_written) != size)
	{
		return connection_result::send_failed;
	}
#endif

	return connection_result::ok;
}

connection_result serial_connection::receive(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept
{
	if (received_size == nullptr)
	{
		return connection_result::invalid_argument;
	}
	*received_size = 0U;

	if (!is_open_)
	{
		return connection_result::not_open;
	}

	if ((buffer == nullptr) || (capacity == 0U) || (capacity > static_cast<std::size_t>(INT_MAX)))
	{
		return connection_result::invalid_argument;
	}

#ifdef _WIN32
	COMSTAT status = { 0 };
	DWORD errors = 0U;
	DWORD bytes_to_read = 0U;
	DWORD bytes_read = 0U;

	if (!ClearCommError(reinterpret_cast<HANDLE>(handle_), &errors, &status))
	{
		last_error_ = serial_connection_get_last_error();
		return connection_result::receive_failed;
	}

	if (status.cbInQue == 0U)
	{
		return connection_result::would_block;
	}

	bytes_to_read = static_cast<DWORD>(capacity < static_cast<std::size_t>(status.cbInQue)
		? capacity
		: static_cast<std::size_t>(status.cbInQue));
	if (!ReadFile(reinterpret_cast<HANDLE>(handle_), buffer, bytes_to_read, &bytes_read, nullptr))
	{
		last_error_ = serial_connection_get_last_error();
		return connection_result::receive_failed;
	}

	if (bytes_read == 0U)
	{
		return connection_result::would_block;
	}

	*received_size = static_cast<std::size_t>(bytes_read);
#else
	int fd = static_cast<int>(handle_);
	int bytes_available = 0;
	if (ioctl(fd, FIONREAD, &bytes_available) < 0)
	{
		last_error_ = serial_connection_get_last_error();
		return connection_result::receive_failed;
	}

	if (bytes_available == 0)
	{
		return connection_result::would_block;
	}

	std::size_t bytes_to_read = (static_cast<std::size_t>(bytes_available) < capacity)
		? static_cast<std::size_t>(bytes_available)
		: capacity;

	ssize_t bytes_read = ::read(fd, buffer, bytes_to_read);
	if (bytes_read < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			return connection_result::would_block;
		}
		last_error_ = serial_connection_get_last_error();
		return connection_result::receive_failed;
	}

	if (bytes_read == 0)
	{
		return connection_result::would_block;
	}

	*received_size = static_cast<std::size_t>(bytes_read);
#endif

	return connection_result::ok;
}

uint32_t serial_connection::last_error() const noexcept
{
	return last_error_;
}

bool serial_connection::is_open() const noexcept
{
	return is_open_;
}

void serial_connection::reset_state() noexcept
{
	handle_ = invalid_handle();
	has_configuration_ = false;
	is_open_ = false;
	last_error_ = 0U;
	port_name_.clear();
	baud_rate_ = 0U;
	data_bits_ = 0U;
	stop_bits_ = 0U;
	parity_ = 0U;
	rts_cts_ = 0U;
	read_total_timeout_ms_ = 0U;
}

std::uintptr_t serial_connection::invalid_handle() noexcept
{
#ifdef _WIN32
	return static_cast<std::uintptr_t>(-1);
#else
	return static_cast<std::uintptr_t>(-1);
#endif
}