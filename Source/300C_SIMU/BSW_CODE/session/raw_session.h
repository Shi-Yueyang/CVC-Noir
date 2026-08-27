#ifndef RAW_SESSION_INCLUDE
#define RAW_SESSION_INCLUDE

#include <cstddef>

#include "session.h"

class raw_session : public session
{
public:
	explicit raw_session(const nlohmann::json& config);
	~raw_session() override;

	session_result do_input() noexcept override;
	session_result do_output() noexcept override;
	session_result output(const unsigned char* buffer, std::size_t len) noexcept;
};

#endif