#ifndef PXI_SESSION_INCLUDE
#define PXI_SESSION_INCLUDE

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "session.h"

struct pxi_io_mapping_entry
{
	int from = 0;
	int to = 0;
	bool has_or = false;
	int or_from = 0;
};

struct pxi_packet_io_mapping
{
	std::size_t total_length = 0U;
	std::size_t length = 0U;
	std::uint8_t high_value = 0U;
	std::vector<pxi_io_mapping_entry> entries;
};

struct pxi_internal_io_mapping
{
	std::vector<pxi_io_mapping_entry> entries;
};

class pxi_session : public session
{
public:
	explicit pxi_session(const nlohmann::json& config);
	~pxi_session() override;

	session_result do_input() noexcept override;
	session_result do_output() noexcept override;

	void set_train_pos_cm(std::int32_t train_pos_cm) noexcept
	{
		train_pos_cm_ = train_pos_cm;
	}

	const std::vector<pxi_packet_io_mapping>& to_pxi_mappings() const noexcept
	{
		return to_pxi_mappings_;
	}

	const pxi_internal_io_mapping& from_internal_mapping() const noexcept
	{
		return from_internal_mapping_;
	}

	const std::vector<pxi_packet_io_mapping>& from_pxi_mappings() const noexcept
	{
		return from_pxi_mappings_;
	}

private:
	std::vector<pxi_packet_io_mapping> to_pxi_mappings_;
	pxi_internal_io_mapping from_internal_mapping_;
	std::vector<pxi_packet_io_mapping> from_pxi_mappings_;
	std::unordered_map<int, std::string> port_names_in_;
	std::unordered_map<int, std::string> port_names_out_;
	bool trace_unnamed_ports_ = true;
	std::int32_t train_pos_cm_ = 0;
	std::vector<std::uint8_t> internal_io_record_;
};

#endif