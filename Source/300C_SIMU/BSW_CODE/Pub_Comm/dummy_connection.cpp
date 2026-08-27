#include "dummy_connection.h"

#include <algorithm>

dummy_connection::dummy_connection() noexcept
	: is_open_(false)
	, payload_pending_(true)
{
}

dummy_connection::dummy_connection(std::vector<std::uint8_t> payload) noexcept
	: is_open_(false)
	, payload_pending_(true)
	, payload_(std::move(payload))
{
}

connection_result dummy_connection::open() noexcept
{
	if (is_open_)
	{
		return connection_result::already_open;
	}

	is_open_ = true;
	payload_pending_ = true;
	return connection_result::ok;
}

connection_result dummy_connection::close() noexcept
{
	if (!is_open_)
	{
		return connection_result::not_open;
	}

	is_open_ = false;
	payload_pending_ = true;
	return connection_result::ok;
}

connection_result dummy_connection::send(const void* data, std::size_t size) noexcept
{
	(void)data;
	(void)size;

	if (!is_open_)
	{
		return connection_result::not_open;
	}

	return connection_result::ok;
}

connection_result dummy_connection::receive(void* buffer, std::size_t capacity, std::size_t* received_size) noexcept
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

	if (!payload_pending_)
	{
		return connection_result::would_block;
	}

	if ((!payload_.empty() && buffer == nullptr) || payload_.size() > capacity)
	{
		return connection_result::invalid_argument;
	}

	if (!payload_.empty())
	{
		std::copy(payload_.begin(), payload_.end(), static_cast<std::uint8_t*>(buffer));
	}

	*received_size = payload_.size();
	payload_pending_ = false;
	return connection_result::ok;
}
