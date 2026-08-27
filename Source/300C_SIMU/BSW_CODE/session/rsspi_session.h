#ifndef RSSPI_SESSION_INCLUDE
#define RSSPI_SESSION_INCLUDE

#include <cstddef>

#include "session.h"

class rsspi_session : public session
{
public:
	explicit rsspi_session(const nlohmann::json& config);
	~rsspi_session() override;

	session_result do_input() noexcept override;
	session_result do_output() noexcept override;
	session_result output(const unsigned char* buffer, std::size_t len) noexcept;
};

#endif