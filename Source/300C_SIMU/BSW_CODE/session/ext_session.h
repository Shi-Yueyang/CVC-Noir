#ifndef EXT_SESSION_INCLUDE
#define EXT_SESSION_INCLUDE

#include <cstddef>
#include <cstdint>
#include <vector>

#include "session.h"

class ext_session : public session
{
public:
	explicit ext_session(const nlohmann::json& config);
	~ext_session() override;

	session_result do_input() noexcept override;
	session_result do_output() noexcept override;
	session_result output(const std::uint8_t* buffer, std::size_t len) noexcept;

	std::uint32_t peripheral_number() const noexcept;

private:
	session_result publish_payload(const std::uint8_t* payload, std::size_t len) noexcept;

	std::size_t prefix_zero_bytes_;
	std::vector<std::uint8_t> idle_payload_;
};

#endif
