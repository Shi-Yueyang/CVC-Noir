#ifndef A_TRAIN_SESSION_INCLUDE
#define A_TRAIN_SESSION_INCLUDE

#include <cstddef>

#include "session.h"

inline const char* a_train_dummy_payload_line() noexcept
{
	return "{\"type\":\"a_train\",\"dummy\":true}\n";
}

class a_train_session : public session
{
public:
	explicit a_train_session(const nlohmann::json& config);
	~a_train_session() override;

	session_result do_input() noexcept override;
	session_result do_output() noexcept override;
};

#endif
