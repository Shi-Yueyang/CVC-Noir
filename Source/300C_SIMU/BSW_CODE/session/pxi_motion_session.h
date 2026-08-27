#ifndef PXI_MOTION_SESSION_INCLUDE
#define PXI_MOTION_SESSION_INCLUDE

#include <cstdint>

#include "session.h"

class pxi_motion_session : public session
{
public:
	explicit pxi_motion_session(const nlohmann::json& config);
	~pxi_motion_session() override;

	session_result do_input() noexcept override;
	session_result do_output() noexcept override;

	bool try_get_train_pos_mm(std::int32_t* position_mm) const noexcept;

private:
	std::int32_t last_position_ = 0;
	bool has_last_position_ = false;
};

#endif