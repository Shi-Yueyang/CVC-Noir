#include "a_train_session.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "json.hpp"

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

static void test_payload_is_ndjson()
{
	const std::string line = a_train_dummy_payload_line();

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
	check(session.output() == session_result::ok, "a_train: output sends dummy payload");
}

int main()
{
	test_parses_common_fields();
	test_no_connection_returns_invalid();
	test_is_skip_short_circuits();
	test_payload_is_ndjson();
	test_connection_created_from_config();

	std::printf("test_a_train_session: %d passed, %d failed\n", s_passed, s_failed);
	return s_failed == 0 ? 0 : 1;
}
