#include "a_train_session.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "../../ASW_300C/Interface_Data.h"
#include "../../logging/logger.h"
#include "../Pub_Comm/connection_factory.h"

namespace {

Logger::Ptr get_a_train_log()
{
	return Logger::get("a_train");
}

}

std::string a_train_atp_payload_line(const IOData_t& vob_data)
{
	std::size_t signal_size = 0U;
	for (std::size_t i = 0U; i < vob_data.Length; ++i)
	{
		signal_size = (std::max)(signal_size,
			static_cast<std::size_t>(vob_data.IOPortData[i].PortIndex) + 1U);
	}

	std::string atp_signal(signal_size, '0');
	for (std::size_t i = 0U; i < vob_data.Length; ++i)
	{
		const std::size_t port_index = vob_data.IOPortData[i].PortIndex;
		atp_signal[port_index] = vob_data.IOPortData[i].PortValue == 1U ? '1' : '0';
	}
	for (std::size_t position = 5U; position < atp_signal.size(); position += 6U)
	{
		atp_signal.insert(position, 1U, '_');
	}

	const nlohmann::ordered_json payload = {
		{"type", "atp_command"},
		{"cab_id", a_train_hardcoded_cab_id()},
		{"atp_signal", atp_signal}
	};
	return payload.dump() + "\n";
}

bool a_train_extract_vib_input(const std::string& line, const std::string& signal_type, int cab, IOData_t* out_data)
{
	if (out_data == nullptr || signal_type.empty())
	{
		return false;
	}

	const nlohmann::json root = nlohmann::json::parse(line, nullptr, false);
	if (root.is_discarded() || !root.is_object())
	{
		return false;
	}

	if (!root.contains("type") || !root["type"].is_string()
		|| root["type"].get<std::string>() != "train_state")
	{
		return false;
	}

	if (!root.contains("equipment") || !root["equipment"].is_array())
	{
		return false;
	}

	std::vector<std::string> seen_entries;
	for (const nlohmann::json& entry : root["equipment"])
	{
		bool matched = false;
		if (entry.is_object()
			&& entry.contains("type") && entry["type"].is_string()
			&& entry["type"].get_ref<const std::string&>().compare(0U, signal_type.size(), signal_type) == 0)
		{
			matched = true;
			if (cab != 0)
			{
				if (!entry.contains("cab_id") || !entry["cab_id"].is_number_integer()
					|| entry["cab_id"].get<int>() != cab)
				{
					matched = false;
				}
			}
		}

		if (!matched)
		{
			if (entry.is_object() && entry.contains("type") && entry["type"].is_string()
				&& seen_entries.size() < 8U)
			{
				std::string description = entry["type"].get<std::string>();
				if (entry.contains("cab_id") && entry["cab_id"].is_number_integer())
				{
					description += ":" + std::to_string(entry["cab_id"].get<int>());
				}
				seen_entries.push_back(description);
			}
			continue;
		}

		if (!entry.contains("state") || !entry["state"].is_object())
		{
			return false;
		}

		const nlohmann::json& state = entry["state"];
		if (!state.contains("train_out_signal") || !state["train_out_signal"].is_string())
		{
			return false;
		}

		const std::string train_out_signal = state["train_out_signal"].get<std::string>();
		if (train_out_signal.empty() || train_out_signal.size() > MAX_IOPORT_NUM)
		{
			return false;
		}

		IOData_t data = { 0 };
		for (std::size_t i = 0U; i < train_out_signal.size(); ++i)
		{
			if (train_out_signal[i] != '0' && train_out_signal[i] != '1')
			{
				return false;
			}
			data.IOPortData[i].PortIndex = static_cast<INT16U>(i);
			data.IOPortData[i].PortValue = train_out_signal[i] == '1' ? 1U : 0U;
		}
		data.Length = static_cast<INT16U>(train_out_signal.size());
		*out_data = data;
		return true;
	}

	std::string listed_entries;
	for (std::size_t i = 0U; i < seen_entries.size(); ++i)
	{
		if (i != 0U)
		{
			listed_entries += ',';
		}
		listed_entries += seen_entries[i];
	}
	if (root["equipment"].size() > seen_entries.size())
	{
		listed_entries += ",...";
	}
	if (cab != 0)
	{
		get_a_train_log()->warn("train_state has no equipment entry with type prefix '%s' and cab %d (entries seen: [%s])",
			signal_type.c_str(),
			cab,
			listed_entries.c_str());
	}
	else
	{
		get_a_train_log()->warn("train_state has no equipment entry with type prefix '%s' (entries seen: [%s])",
			signal_type.c_str(),
			listed_entries.c_str());
	}
	return false;
}

bool a_train_extract_motion_sample(const std::string& line, asw_motion_sample* out_sample)
{
	if (out_sample == nullptr)
	{
		return false;
	}

	const nlohmann::json root = nlohmann::json::parse(line, nullptr, false);
	if (root.is_discarded() || !root.is_object())
	{
		return false;
	}

	if (!root.contains("type") || !root["type"].is_string()
		|| root["type"].get<std::string>() != "train_state")
	{
		return false;
	}

	if (!root.contains("position") || !root["position"].is_number()
		|| !root.contains("speed") || !root["speed"].is_number()
		|| !root.contains("acceleration") || !root["acceleration"].is_number())
	{
		return false;
	}

	const double position_m = root["position"].get<double>();
	const double speed_m_s = root["speed"].get<double>();
	const double acceleration_m_s2 = root["acceleration"].get<double>();
	if (!std::isfinite(position_m)
		|| !std::isfinite(speed_m_s)
		|| !std::isfinite(acceleration_m_s2))
	{
		return false;
	}

	constexpr double kInt32Max = static_cast<double>((std::numeric_limits<std::int32_t>::max)());
	constexpr double kInt32Min = static_cast<double>((std::numeric_limits<std::int32_t>::min)());
	constexpr double kInt16Max = static_cast<double>((std::numeric_limits<std::int16_t>::max)());
	constexpr double kInt16Min = static_cast<double>((std::numeric_limits<std::int16_t>::min)());
	if (position_m * 1000.0 > kInt32Max || position_m * 1000.0 < kInt32Min
		|| speed_m_s * 1000.0 > kInt32Max || speed_m_s * 1000.0 < kInt32Min
		|| acceleration_m_s2 * 1000.0 > kInt16Max || acceleration_m_s2 * 1000.0 < kInt16Min)
	{
		return false;
	}

	const long long position_mm = std::llround(position_m * 1000.0);
	const long long velocity_mm_s = std::llround(speed_m_s * 1000.0);
	const long long acceleration_mm_s2 = std::llround(acceleration_m_s2 * 1000.0);
	if (position_mm < INT32_MIN || position_mm > INT32_MAX
		|| velocity_mm_s < INT32_MIN || velocity_mm_s > INT32_MAX
		|| acceleration_mm_s2 < INT16_MIN || acceleration_mm_s2 > INT16_MAX)
	{
		return false;
	}

	asw_motion_sample sample = {};
	sample.time_ms = sdmu_current_time_ms();
	sample.position_mm = static_cast<std::int32_t>(position_mm);
	sample.velocity_mm_s = static_cast<std::int32_t>(velocity_mm_s);
	sample.acceleration_mm_s2 = static_cast<std::int32_t>(acceleration_mm_s2);
	sample.motion = velocity_mm_s > 0 ? 1U : 0U;
	sample.direction = velocity_mm_s < 0 ? 1U : 0U;
	if (root.contains("direction") && root["direction"].is_string())
	{
		const std::string& direction = root["direction"].get_ref<const std::string&>();
		if (direction == "forward")
		{
			sample.direction = 0U;
		}
		else if (direction == "rearward")
		{
			sample.direction = 1U;
		}
	}
	*out_sample = sample;
	return true;
}

a_train_session::a_train_session(const nlohmann::json& config)
	: session(config)
	, cab_(0)
	, take_motion_(false)
{
	if (config.is_object() && config.contains("signal_type") && config["signal_type"].is_string())
	{
		signal_type_ = config["signal_type"].get<std::string>();
	}

	if (config.is_object() && config.contains("cab") && config["cab"].is_number_integer())
	{
		cab_ = config["cab"].get<int>();
	}

	if (config.is_object() && config.contains("take_motion") && config["take_motion"].is_boolean())
	{
		take_motion_ = config["take_motion"].get<bool>();
	}

	if (config.is_object() && config.contains("connection") && config["connection"].is_object())
	{
		connection_ = connection_factory::create(config["connection"]);
	}
}

a_train_session::~a_train_session()
{
}

session_result a_train_session::do_input() noexcept
{
	if (!connection_)
	{
		return session_result::invalid_argument;
	}

	std::array<unsigned char, 4096> buffer = {};
	std::size_t received_size = 0U;
	bool received_any_packet = false;
	bool vib_updated = false;
	IOData_t vib_data = { 0 };
	bool motion_updated = false;
	asw_motion_sample motion_sample = {};

	for (;;)
	{
		const connection_result res = connection_->receive(buffer.data(), buffer.size(), &received_size);
		if (res == connection_result::ok)
		{
			const std::string line(reinterpret_cast<const char*>(buffer.data()), received_size);
			get_a_train_log()->trace("%s in: len=%zu data=%s",
				name().c_str(),
				received_size,
				line.c_str());
			received_any_packet = true;

			IOData_t candidate = { 0 };
			if (a_train_extract_vib_input(line, signal_type_, cab_, &candidate))
			{
				vib_data = candidate;
				vib_updated = true;
			}

			if (take_motion_)
			{
				asw_motion_sample motion_candidate = {};
				if (a_train_extract_motion_sample(line, &motion_candidate))
				{
					motion_sample = motion_candidate;
					motion_updated = true;
				}
			}
			continue;
		}

		if (res == connection_result::would_block)
		{
			break;
		}

		return session_map_receive_result(res);
	}

	if (vib_updated)
	{
		if (CVC_FALSE == CVC_BSW_ITF_Write(CVC_BSW_VIB_RX_TYPE, reinterpret_cast<INT8U*>(&vib_data), sizeof(vib_data)))
		{
			return session_result::forward_failed;
		}
		get_a_train_log()->debug("%s in: vib rx len=%u", name().c_str(), vib_data.Length);
	}

	if (motion_updated)
	{
		unsigned char packet_buffer[256] = { 0U };
		std::uint16_t packet_length = 0U;

		assemble_secure_sdmu_packet(motion_sample, packet_buffer, &packet_length);
		if (!write_asw_message(packet_buffer, packet_length))
		{
			return session_result::forward_failed;
		}

		assemble_accurate_sdmu_packet(motion_sample, packet_buffer, &packet_length);
		if (!write_asw_message(packet_buffer, packet_length))
		{
			return session_result::forward_failed;
		}

		get_a_train_log()->debug("%s in: sdmu motion pos_mm=%ld vel_mm_s=%ld acc_mm_s2=%ld",
			name().c_str(),
			static_cast<long>(motion_sample.position_mm),
			static_cast<long>(motion_sample.velocity_mm_s),
			static_cast<long>(motion_sample.acceleration_mm_s2));
	}

	if (!received_any_packet)
	{
		return session_result::would_block;
	}

	return session_result::ok;
}

session_result a_train_session::do_output() noexcept
{
	if (!connection_)
	{
		return session_result::invalid_argument;
	}

	bool sent_any_packet = false;
	IOData_t vob_data = {};
	while (CVC_BUFFER_OPER_SUCCESS == CVC_BSW_ITF_Read(CVC_BSW_VOB_TX_TYPE, reinterpret_cast<INT8U*>(&vob_data)))
	{
		if (vob_data.Length > MAX_IOPORT_NUM)
		{
			return session_result::invalid_argument;
		}

		const std::string payload = a_train_atp_payload_line(vob_data);
		const std::string log_payload = payload.substr(0U, payload.size() - 1U);
		get_a_train_log()->trace("a-train out: %s", log_payload.c_str());
		const connection_result send_result = connection_->send(
			reinterpret_cast<const unsigned char*>(payload.data()), payload.size());
		if (send_result != connection_result::ok)
		{
			return session_map_send_result(send_result);
		}

		sent_any_packet = true;
		vob_data = {};
	}

	return sent_any_packet ? session_result::ok : session_result::would_block;
}
