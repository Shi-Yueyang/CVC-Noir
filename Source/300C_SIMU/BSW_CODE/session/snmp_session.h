#ifndef SNMP_SESSION_INCLUDE
#define SNMP_SESSION_INCLUDE

#include <array>

#include "session.h"

class snmp_session : public session
{
public:
	explicit snmp_session(const nlohmann::json& config);
	~snmp_session() override;

	session_result do_input() noexcept override;
	session_result do_output() noexcept override;

private:
	std::array<std::int32_t, 4> mobile_state_;
};

#endif