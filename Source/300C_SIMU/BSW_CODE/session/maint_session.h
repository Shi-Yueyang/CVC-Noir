#ifndef MAINT_SESSION_INCLUDE
#define MAINT_SESSION_INCLUDE

#include <cstddef>

#include "session.h"

class maint_session : public session
{
public:
	explicit maint_session(const nlohmann::json& config);
	~maint_session() override;

	session_result do_input() noexcept override;
	session_result do_output() noexcept override;
	session_result output(const unsigned char* buffer, std::size_t len) noexcept;
};

#endif