#ifndef A_TRAIN_SESSION_INCLUDE
#define A_TRAIN_SESSION_INCLUDE

#include <cstddef>
#include <string>

#include "../../ASW_300C/Interface_Data.h"
#include "sdmu_packet.h"
#include "session.h"

inline constexpr int a_train_hardcoded_cab_id() noexcept
{
	return 1;
}

std::string a_train_atp_payload_line(const IOData_t& vob_data);

bool a_train_extract_vib_input(const std::string& line, const std::string& signal_type, int cab, IOData_t* out_data);

bool a_train_extract_motion_sample(const std::string& line, asw_motion_sample* out_sample);

class a_train_session : public session
{
public:
	explicit a_train_session(const nlohmann::json& config);
	~a_train_session() override;

	const std::string& signal_type() const noexcept
	{
		return signal_type_;
	}

	int cab() const noexcept
	{
		return cab_;
	}

	session_result do_input() noexcept override;
	session_result do_output() noexcept override;

private:
	std::string signal_type_;
	int cab_;
	bool take_motion_;
};

#endif
