#ifndef OTHER_ASW_SESSION_INCLUDE
#define OTHER_ASW_SESSION_INCLUDE

#include <cstddef>

#include "session.h"

class other_asw_session : public session
{
public:
	explicit other_asw_session(const nlohmann::json& config);
	~other_asw_session() override;

	session_result do_input() noexcept override;
	session_result do_output() noexcept override;
	session_result output(const unsigned char* buffer, std::size_t len) noexcept;
};

#endif