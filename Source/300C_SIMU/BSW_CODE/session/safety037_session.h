#ifndef SAFETY037_SESSION_INCLUDE
#define SAFETY037_SESSION_INCLUDE

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include "session.h"

class safety037_session : public session
{
public:
	struct peer_mapping
	{
		std::string from_ip;
		unsigned int from_port;
		std::string to_ip;
		unsigned int to_port;
	};

	explicit safety037_session(const nlohmann::json& config);
	~safety037_session() override;

	session_result do_input() noexcept override;
	session_result do_output() noexcept override;
	session_result output(const unsigned char* buffer, std::size_t len, unsigned char msg_id, unsigned int peripheral_number) noexcept;
	unsigned int peripheral_number() const noexcept;

private:
	using clock_type = std::chrono::steady_clock;

	void schedule_connect_success() noexcept;
	void schedule_disconnect_failure() noexcept;
	void handle_receive_activity(clock_type::time_point now) noexcept;
	bool try_apply_dynamic_peer(const unsigned char* buffer, std::size_t len) noexcept;
	bool remap_dynamic_peer(std::string* ip, unsigned int* port) const noexcept;
	session_result emit_status(unsigned char msg_id) noexcept;

	int recv_no_data_send_lost_ms_;
	int connect_success_delay_ms_;
	int disconnect_failure_delay_ms_;
	unsigned int peripheral_number_;
	bool dynamic_peer_;
	std::vector<peer_mapping> peer_mappings_;
	bool connected_;
	bool connect_pending_;
	bool disconnect_pending_;
	bool recv_no_data_send_lost_armed_;
	clock_type::time_point connect_deadline_;
	clock_type::time_point disconnect_deadline_;
	clock_type::time_point last_receive_time_;
};

#endif