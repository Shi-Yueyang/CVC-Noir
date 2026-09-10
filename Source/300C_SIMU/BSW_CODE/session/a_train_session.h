#ifndef A_TRAIN_SESSION_INCLUDE
#define A_TRAIN_SESSION_INCLUDE

#include <cstddef>
#include <string>

#include "../../ASW_300C/Interface_Data.h"
#include "session.h"

inline constexpr const char* a_train_hardcoded_train_id() noexcept
{
	return "TRAIN001";
}

inline constexpr int a_train_hardcoded_cab_id() noexcept
{
	return 1;
}

std::string a_train_atp_payload_line(const IOData_t& vob_data);

class a_train_session : public session
{
public:
	explicit a_train_session(const nlohmann::json& config);
	~a_train_session() override;

	session_result do_input() noexcept override;
	session_result do_output() noexcept override;
};

#endif
