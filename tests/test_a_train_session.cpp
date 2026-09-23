#include "a_train_session.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "json.hpp"
#include "ASW_300C/Interface_Data.h"

extern "C" INT32S CVC_BSW_ITF_Read(CVC_BSW_MSG_TYPE_ENUM, INT8U*)
{
	return CVC_BUFFER_OPER_EMPTY;
}

extern "C" void API_ReadCurrentRunTime(INT32U* const opValue)
{
	if (opValue != nullptr)
	{
		*opValue = 5U;
	}
}

static IOData_t s_last_vib_write = { 0 };
static int s_vib_write_count = 0;
static std::vector<ASWRxData_t> s_asw_writes;

extern "C" BOOLEAN CVC_BSW_ITF_Write(CVC_BSW_MSG_TYPE_ENUM msgType, INT8U* pData, INT16U dataSize)
{
	if (msgType == CVC_BSW_VIB_RX_TYPE
		&& pData != nullptr
		&& dataSize == sizeof(IOData_t))
	{
		std::memcpy(&s_last_vib_write, pData, sizeof(s_last_vib_write));
		++s_vib_write_count;
	}
	if (msgType == CVC_BSW_ASW_COM_RX_TYPE
		&& pData != nullptr
		&& dataSize >= static_cast<INT16U>(sizeof(ASWRxData_t) - ASW_COM_DATA_SIZE + 1U))
	{
		ASWRxData_t captured = { 0 };
		const std::size_t copy_size = (dataSize < sizeof(ASWRxData_t))
			? dataSize
			: sizeof(ASWRxData_t);
		std::memcpy(&captured, pData, copy_size);
		s_asw_writes.push_back(captured);
	}
	return CVC_TRUE;
}

static std::string a_train_test_train_out_signal()
{
	return R"({"type":"train_state","equipment":[)"
		R"({"type":"door","cab_id":1,"state":{"state":"closed"}},)"
		R"({"type":"stcs_atp_duo","cab_id":1,"state":{"last_command":"0001000","train_out_signal":"10011"}},)"
		R"({"type":"stcs_atp_duo","cab_id":2,"state":{"train_out_signal":"00000"}}])"
		R"(})";
}

static std::string a_train_test_train_state_full()
{
	return R"({"type":"train_state","speed":22.31,"acceleration":-0.15,"position":15320.4,"direction":"forward","equipment":[)"
		R"({"type":"btm","cab_id":1,"state":{"pending":false}},)"
		R"({"type":"stcs_atp_duo","cab_id":1,"state":{"train_out_signal":"10011"}}])"
		R"(})";
}

static int s_passed = 0;
static int s_failed = 0;

static void check(bool condition, const char* name)
{
	if (condition)
	{
		++s_passed;
	}
	else
	{
		++s_failed;
		std::printf("  FAIL: %s\n", name);
	}
}

static void test_parses_common_fields()
{
	const nlohmann::json config = nlohmann::json::parse(R"({ "name": "a-train" })");
	a_train_session session(config);

	check(session.name() == "a-train", "a_train: name parsed");
	check(session.id() == 0, "a_train: id defaults to 0 (no id in spec)");
	check(session.is_skip() == false, "a_train: is_skip defaults to false");
	check(session.connection() == nullptr, "a_train: no connection without config");
}

static void test_no_connection_returns_invalid()
{
	const nlohmann::json config = nlohmann::json::parse(R"({ "name": "a-train" })");
	a_train_session session(config);

	check(session.input() == session_result::invalid_argument, "a_train: input invalid without connection");
	check(session.output() == session_result::invalid_argument, "a_train: output invalid without connection");
}

static void test_is_skip_short_circuits()
{
	const nlohmann::json config = nlohmann::json::parse(R"({ "name": "a-train", "is_skip": true })");
	a_train_session session(config);

	check(session.is_skip() == true, "a_train: is_skip parsed");
	check(session.input() == session_result::ok, "a_train: input ok when skipped");
	check(session.output() == session_result::ok, "a_train: output ok when skipped");
}

static void test_parses_signal_filters()
{
	const nlohmann::json with_filters = nlohmann::json::parse(R"({
		"name": "a-train",
		"signal_type": "stcs_atp",
		"cab": 1
	})");
	a_train_session keyed(with_filters);
	check(keyed.signal_type() == "stcs_atp", "a_train: signal_type parsed");
	check(keyed.cab() == 1, "a_train: cab parsed");

	const nlohmann::json missing = nlohmann::json::parse(R"({ "name": "a-train" })");
	a_train_session absent(missing);
	check(absent.signal_type().empty(), "a_train: signal_type defaults to empty");
	check(absent.cab() == 0, "a_train: cab defaults to 0 (no cab filtering)");

	const nlohmann::json wrong_type = nlohmann::json::parse(R"({ "name": "a-train", "signal_type": 42, "cab": "1" })");
	a_train_session ignored(wrong_type);
	check(ignored.signal_type().empty(), "a_train: non-string signal_type ignored");
	check(ignored.cab() == 0, "a_train: non-integer cab ignored");
}

static void test_payload_is_atp_command()
{
	IOData_t vob_data = {};
	vob_data.Length = 4U;
	vob_data.IOPortData[0] = { 0U, 0U };
	vob_data.IOPortData[1] = { 2U, 1U };
	vob_data.IOPortData[2] = { 4U, 1U };
	vob_data.IOPortData[3] = { 16U, 1U };
	const std::string line = a_train_atp_payload_line(vob_data);

	check(!line.empty() && line.back() == '\n', "a_train: payload ends with newline");
	check(line.find('\n') == line.size() - 1U, "a_train: payload is exactly one NDJSON line");

	bool parsed = false;
	try
	{
		const nlohmann::json obj = nlohmann::json::parse(line);
		parsed = obj.is_object();
	}
	catch (const std::exception&)
	{
		parsed = false;
	}
	check(parsed, "a_train: payload line is valid JSON object");
	if (parsed)
	{
		const nlohmann::json obj = nlohmann::json::parse(line);
		check(obj["type"] == "atp_command", "a_train: payload type is atp_command");
		check(!obj.contains("train_id"), "a_train: payload has no train_id field");
		check(obj["cab_id"] == 1, "a_train: payload uses hardcoded cab id");
		check(obj["atp_signal"] == "00101_00000_00000_01", "a_train: payload maps and groups VOB bits");
	}
}

static void test_connection_created_from_config()
{
	const nlohmann::json config = nlohmann::json::parse(R"({
		"name": "a-train",
		"connection": { "type": "dummy", "received_data": [1, 2, 3] }
	})");
	a_train_session session(config);

	check(session.connection() != nullptr, "a_train: connection created from config");
	if (session.connection() == nullptr)
	{
		return;
	}

	check(session.connection()->open() == connection_result::ok, "a_train: dummy connection opens");
	check(session.input() == session_result::ok, "a_train: input drains pending payload");
	check(session.input() == session_result::would_block, "a_train: input would_block when drained");
	check(session.output() == session_result::would_block, "a_train: output would_block when no VOB data is pending");
}

static nlohmann::json bytes_to_json_array(const std::string& text)
{
	nlohmann::json arr = nlohmann::json::array();
	for (const unsigned char byte : text)
	{
		arr.push_back(static_cast<int>(byte));
	}
	return arr;
}

static void test_extract_vib_input()
{
	const std::string line = a_train_test_train_out_signal();
	IOData_t data = { 0 };

	check(a_train_extract_vib_input(line, "stcs_atp", 1, &data), "a_train: extracts type prefix + cab match");
	check(data.Length == 5U, "a_train: vib length matches signal bits");
	check(data.IOPortData[0].PortIndex == 0U && data.IOPortData[0].PortValue == 1U, "a_train: bit 0 maps to port 0 high");
	check(data.IOPortData[1].PortIndex == 1U && data.IOPortData[1].PortValue == 0U, "a_train: bit 1 low");
	check(data.IOPortData[2].PortIndex == 2U && data.IOPortData[2].PortValue == 0U, "a_train: bit 2 low");
	check(data.IOPortData[3].PortIndex == 3U && data.IOPortData[3].PortValue == 1U, "a_train: bit 3 high");
	check(data.IOPortData[4].PortValue == 1U, "a_train: bit 4 high");

	IOData_t cab2 = { 0 };
	check(a_train_extract_vib_input(line, "stcs_atp_duo", 2, &cab2), "a_train: cab filters to second candidate");
	check(cab2.Length == 5U && cab2.IOPortData[0].PortValue == 0U, "a_train: cab 2 entry signal extracted");

	IOData_t first = { 0 };
	check(a_train_extract_vib_input(line, "stcs_atp", 0, &first), "a_train: cab 0 disables cab filtering");
	check(first.Length == 5U && first.IOPortData[4].PortValue == 1U, "a_train: first prefix candidate wins");

	IOData_t rejected = { 0 };
	check(!a_train_extract_vib_input(line, "stcs_atp", 9, &rejected), "a_train: missing cab not extracted");
	check(!a_train_extract_vib_input(line, "nonexistent", 1, &rejected), "a_train: missing type prefix not extracted");
	check(!a_train_extract_vib_input(line, "", 1, &rejected), "a_train: empty configured signal_type not extracted");
	check(!a_train_extract_vib_input("{not json", "stcs_atp", 1, &rejected), "a_train: invalid json not extracted");
	check(!a_train_extract_vib_input(R"({"type":"error","code":"x"})", "stcs_atp", 1, &rejected), "a_train: non-train_state not extracted");
	check(!a_train_extract_vib_input(R"({"type":"train_state","equipment":[{"type":"stcs_atp_solo","cab_id":1,"state":{"train_out_signal":"1_0"}}]})", "stcs_atp", 1, &rejected), "a_train: non-binary signal not extracted");
	check(!a_train_extract_vib_input(R"({"type":"train_state"})", "stcs_atp", 1, &rejected), "a_train: missing equipment not extracted");
	check(!a_train_extract_vib_input(R"({"type":"train_state","equipment":[{"type":"stcs_atp_solo","cab_id":1}]})", "stcs_atp", 1, &rejected), "a_train: missing state not extracted");
	check(!a_train_extract_vib_input(R"({"type":"train_state","equipment":[{"type":"stcs_atp_solo","cab_id":1,"state":{}}]})", "stcs_atp", 1, &rejected), "a_train: missing train_out_signal not extracted");
	check(!a_train_extract_vib_input(R"({"type":"train_state","equipment":[{"type":"stcs_atp_solo","cab_id":1,"state":{"train_out_signal":""}}]})", "stcs_atp", 1, &rejected), "a_train: empty signal not extracted");
	check(!a_train_extract_vib_input(R"({"type":"train_state","equipment":[{"type":"stcs_atp_solo","state":{"train_out_signal":"10011"}}]})", "stcs_atp", 1, &rejected), "a_train: entry without cab_id rejected when cab filter set");
	IOData_t exact = { 0 };
	check(a_train_extract_vib_input(R"({"type":"train_state","equipment":[{"type":"stcs_atp","cab_id":2,"state":{"train_out_signal":"10011"}}]})", "stcs_atp", 2, &exact), "a_train: full type prefix match with cab accepted");
}

static void test_extract_motion_sample()
{
	const std::string line = a_train_test_train_state_full();
	asw_motion_sample sample = {};

	check(a_train_extract_motion_sample(line, &sample), "a_train: motion extracted from train_state");
	check(sample.position_mm == 15320400, "a_train: position m converted to mm");
	check(sample.velocity_mm_s == 22310, "a_train: speed m/s converted to mm/s");
	check(sample.acceleration_mm_s2 == -150, "a_train: acceleration converted to mm/s^2");
	check(sample.motion == 1U, "a_train: motion flag set while moving");
	check(sample.direction == 0U, "a_train: direction from explicit forward field");
	check(sample.time_ms == 50U, "a_train: sample time uses run-time (5*10ms)");

	asw_motion_sample rearward = {};
	check(a_train_extract_motion_sample(R"({"type":"train_state","position":10.5,"speed":1.5,"acceleration":0.0,"direction":"rearward"})", &rearward), "a_train: motion with rearward direction field");
	check(rearward.velocity_mm_s == 1500, "a_train: positive speed kept signed");
	check(rearward.direction == 1U, "a_train: direction flag from rearward field");
	check(rearward.motion == 1U, "a_train: motion flag from positive speed");

	asw_motion_sample fallback = {};
	check(a_train_extract_motion_sample(R"({"type":"train_state","position":10.5,"speed":2.0,"acceleration":0.0,"direction":"sideways"})", &fallback), "a_train: motion with invalid direction field accepted");
	check(fallback.direction == 0U, "a_train: invalid direction field falls back to speed sign");

	asw_motion_sample backward = {};
	check(a_train_extract_motion_sample(R"({"type":"train_state","position":10.5,"speed":-1.5,"acceleration":0.0})", &backward), "a_train: rearward motion extracted");
	check(backward.velocity_mm_s == -1500, "a_train: negative speed kept signed");
	check(backward.motion == 0U && backward.direction == 1U, "a_train: rearward motion/direction flags");

	asw_motion_sample rejected = {};
	check(!a_train_extract_motion_sample("{not json", &rejected), "a_train: invalid json motion rejected");
	check(!a_train_extract_motion_sample(R"({"type":"error"})", &rejected), "a_train: non-train_state motion rejected");
	check(!a_train_extract_motion_sample(R"({"type":"train_state","position":1.0,"speed":2.0})", &rejected), "a_train: missing acceleration rejected");
	check(!a_train_extract_motion_sample(R"({"type":"train_state","position":true,"speed":2.0,"acceleration":0.1})", &rejected), "a_train: non-number position rejected");
	check(!a_train_extract_motion_sample(R"({"type":"train_state","position":1e300,"speed":2.0,"acceleration":0.1})", &rejected), "a_train: overflowing position rejected");
	check(!a_train_extract_motion_sample(a_train_test_train_out_signal().c_str(), &rejected), "a_train: train_state without motion fields rejected");
}

static void test_input_publishes_motion()
{
	s_last_vib_write = { 0 };
	s_vib_write_count = 0;
	s_asw_writes.clear();

	nlohmann::json config = {
		{ "name", "a-train" },
		{ "signal_type", "stcs_atp" },
		{ "cab", 1 },
		{ "take_motion", true },
		{ "connection", {
			{ "type", "dummy" },
			{ "received_data", bytes_to_json_array(a_train_test_train_state_full()) }
		} }
	};
	a_train_session session(config);

	check(session.connection()->open() == connection_result::ok, "a_train: dummy connection opens for motion test");
	check(session.input() == session_result::ok, "a_train: input ok with motion payload");
	check(s_asw_writes.size() == 2, "a_train: secure and accurate sdmu messages written");
	check(s_vib_write_count == 1, "a_train: vib and motion publish together");
	if (s_asw_writes.size() == 2)
	{
		check(s_asw_writes[0].SrcID == ATP_SDLU_IN_ATP_SESSION_ID
			&& s_asw_writes[1].SrcID == ATP_SDLU_IN_ATP_SESSION_ID, "a_train: sdmu src id is ATP SDLU in ATP");
		check(static_cast<unsigned char>(s_asw_writes[0].Data[1]) == 0x33U, "a_train: first sdmu packet is secure type");
		check(static_cast<unsigned char>(s_asw_writes[1].Data[1]) == 0x39U, "a_train: second sdmu packet is accurate type");
		check(s_asw_writes[0].Size > 0U && s_asw_writes[1].Size > 0U, "a_train: sdmu packet payload sizes set");
	}
	check(session.input() == session_result::would_block, "a_train: motion input would_block after drain");
	check(s_asw_writes.size() == 2, "a_train: no extra sdmu writes when nothing received");
}

static void test_input_motion_disabled()
{
	s_last_vib_write = { 0 };
	s_vib_write_count = 0;
	s_asw_writes.clear();

	nlohmann::json config = {
		{ "name", "a-train" },
		{ "signal_type", "stcs_atp" },
		{ "cab", 1 },
		{ "take_motion", false },
		{ "connection", {
			{ "type", "dummy" },
			{ "received_data", bytes_to_json_array(a_train_test_train_state_full()) }
		} }
	};
	a_train_session session(config);

	check(session.connection()->open() == connection_result::ok, "a_train: dummy connection opens for disabled test");
	check(session.input() == session_result::ok, "a_train: input ok with motion disabled");
	check(s_asw_writes.empty(), "a_train: no sdmu writes when take_motion false");
	check(s_vib_write_count == 1, "a_train: vib publish unaffected by take_motion false");
}

static void test_input_publishes_vib()
{
	s_last_vib_write = { 0 };
	s_vib_write_count = 0;

	nlohmann::json config = {
		{ "name", "a-train" },
		{ "signal_type", "stcs_atp" },
		{ "cab", 1 },
		{ "connection", {
			{ "type", "dummy" },
			{ "received_data", bytes_to_json_array(a_train_test_train_out_signal()) }
		} }
	};
	a_train_session session(config);

	check(session.connection() != nullptr, "a_train: dummy connection created");
	check(session.connection()->open() == connection_result::ok, "a_train: dummy connection opens for vib test");
	check(session.input() == session_result::ok, "a_train: input ok with train_state payload");
	check(s_vib_write_count == 1, "a_train: one VIB write published");
	check(s_last_vib_write.Length == 5U, "a_train: published VIB length");
	check(s_last_vib_write.IOPortData[0].PortValue == 1U
		&& s_last_vib_write.IOPortData[4].PortValue == 1U
		&& s_last_vib_write.IOPortData[2].PortValue == 0U, "a_train: published VIB bits match");
	check(session.input() == session_result::would_block, "a_train: input would_block after drain");
	check(s_vib_write_count == 1, "a_train: no extra VIB write when nothing received");
}

int main()
{
	test_parses_common_fields();
	test_no_connection_returns_invalid();
	test_is_skip_short_circuits();
	test_parses_signal_filters();
	test_extract_vib_input();
	test_payload_is_atp_command();
	test_connection_created_from_config();
	test_input_publishes_vib();
	test_extract_motion_sample();
	test_input_publishes_motion();
	test_input_motion_disabled();

	std::printf("test_a_train_session: %d passed, %d failed\n", s_passed, s_failed);
	return s_failed == 0 ? 0 : 1;
}
