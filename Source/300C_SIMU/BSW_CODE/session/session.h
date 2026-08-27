#ifndef SESSION_INCLUDE
#define SESSION_INCLUDE

#include <cstdlib>
#include <limits>
#include <memory>
#include <string>

#include "../Pub_Comm/IConnection.h"
#include "../../EXTERNAL/json.hpp"

inline int session_parse_id(const nlohmann::json& id_config) noexcept
{
	if (id_config.is_number_integer())
	{
		return id_config.get<int>();
	}

	if (id_config.is_string())
	{
		const std::string id_text = id_config.get<std::string>();
		char* end_ptr = nullptr;
		const long parsed_value = std::strtol(id_text.c_str(), &end_ptr, 0);

		if (end_ptr != nullptr
			&& *end_ptr == '\0'
			&& parsed_value >= static_cast<long>((std::numeric_limits<int>::min)())
			&& parsed_value <= static_cast<long>((std::numeric_limits<int>::max)()))
		{
			return static_cast<int>(parsed_value);
		}
	}

	return 0;
}

enum class session_result
{
	ok = 0,
	invalid_argument,
	would_block,
	not_connected,
	receive_failed,
	send_failed,
	forward_failed,
	internal_error
};

inline session_result session_map_receive_result(connection_result result) noexcept
{
	switch (result)
	{
	case connection_result::ok:
		return session_result::ok;
	case connection_result::would_block:
		return session_result::would_block;
	case connection_result::not_open:
		return session_result::not_connected;
	default:
		return session_result::receive_failed;
	}
}

inline session_result session_map_send_result(connection_result result) noexcept
{
	switch (result)
	{
	case connection_result::ok:
		return session_result::ok;
	case connection_result::would_block:
		return session_result::would_block;
	case connection_result::not_open:
		return session_result::not_connected;
	default:
		return session_result::send_failed;
	}
}

class session
{
public:
	explicit session(const nlohmann::json& config)
		: id_(0)
		, is_skip_(false)
	{
		if (config.is_object() && config.contains("name") && config["name"].is_string())
		{
			name_ = config["name"].get<std::string>();
		}

		if (config.is_object() && config.contains("id") && config["id"].is_number_integer())
		{
			id_ = config["id"].get<int>();
		}
		else if (config.is_object() && config.contains("id"))
		{
			id_ = session_parse_id(config["id"]);
		}

		if (config.is_object() && config.contains("is_skip") && config["is_skip"].is_boolean())
		{
			is_skip_ = config["is_skip"].get<bool>();
		}
	}

	virtual ~session() = default;

	session_result input() noexcept
	{
		if (is_skip_)
		{
			return session_result::ok;
		}

		return do_input();
	}

	session_result output() noexcept
	{
		if (is_skip_)
		{
			return session_result::ok;
		}

		return do_output();
	}

	int id() const noexcept
	{
		return id_;
	}

	const std::string& name() const noexcept
	{
		return name_;
	}

	bool is_skip() const noexcept
	{
		return is_skip_;
	}

	IConnection* connection() noexcept
	{
		return connection_.get();
	}

	const IConnection* connection() const noexcept
	{
		return connection_.get();
	}

protected:
	virtual session_result do_input() noexcept = 0;
	virtual session_result do_output() noexcept = 0;

	int id_;
	std::string name_;
	bool is_skip_;
	std::unique_ptr<IConnection> connection_;
};

#endif