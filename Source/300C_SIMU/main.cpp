#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#include <time.h>
#endif

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <iostream>
#include <stdio.h>
#include <thread>

#include <json.hpp>

#include "BSW_CODE/AppContext.h"
#include "BSW_CODE/Initial.h"
#include "logging/logger.h"
#include "runtime/app_launcher.h"
#include "BSW_CODE/mainsafety.h"
#include "ASW_300C/interface_p2a.h"

#ifdef _MSC_VER
#pragma warning(disable : 4057 4100 4127 4189 4244 4293 4389)
#endif

namespace
{
static const char* const k_default_config_path = "simu_config.json";
static const uint32_t DEFAULT_MAIN_LOOP_PERIOD_MS = 200U;
std::atomic<bool> g_shutdown_performed(false);

void perform_application_shutdown();
uint32_t load_main_loop_period_ms(const std::string& config_path);
bool load_ignore_shutdown(const std::string& config_path);

std::string format_digit_groups(const long long value)
{
	std::string formatted = std::to_string(value);
	for (std::size_t position = formatted.size(); position > 3U; position -= 3U)
	{
		formatted.insert(position - 3U, 1U, '_');
	}
	return formatted;
}

#ifdef _WIN32
BOOL WINAPI console_control_handler(DWORD control_type)
{
	switch (control_type)
	{
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		perform_application_shutdown();
		return TRUE;
	default:
		return FALSE;
	}
}
#else
void posix_signal_handler(int /*signum*/)
{
	perform_application_shutdown();
}
#endif

void check_log_config_reload(const std::string& config_path);

void enable_virtual_terminal_processing()
{
#ifdef _WIN32
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD mode = 0;
	if (GetConsoleMode(hOut, &mode))
	{
		mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(hOut, mode);
	}
#endif
	/* On Linux, terminals natively support ANSI escape codes. */
}
}

int main(int argc, char* argv[])
{
	enable_virtual_terminal_processing();
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCtrlHandler(console_control_handler, TRUE);
#else
	signal(SIGINT, posix_signal_handler);
	signal(SIGTERM, posix_signal_handler);
#endif

	const std::string config_path = (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0')
		? std::string(argv[1])
		: std::string(k_default_config_path);

	std::cout << "\n\n\033[96m"
		<< "\xe2\x96\x84\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88 \xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88 \xe2\x96\x84\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88     \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88 \xe2\x96\x84\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x84 \xe2\x96\x88\xe2\x96\x88 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x84  \n"
		<< "\xe2\x96\x88\xe2\x96\x88     \xe2\x96\x88\xe2\x96\x88\xe2\x96\x84\xe2\x96\x84\xe2\x96\x88\xe2\x96\x88 \xe2\x96\x88\xe2\x96\x88     \xe2\x96\x84\xe2\x96\x84\xe2\x96\x84 \xe2\x96\x88\xe2\x96\x88 \xe2\x96\x80\xe2\x96\x84\xe2\x96\x88\xe2\x96\x88 \xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88 \xe2\x96\x88\xe2\x96\x88 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x84\xe2\x96\x84\xe2\x96\x88\xe2\x96\x88\xe2\x96\x84 \n"
		<< "\xe2\x96\x80\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x80\xe2\x96\x88\xe2\x96\x88\xe2\x96\x80  \xe2\x96\x80\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88     \xe2\x96\x88\xe2\x96\x88   \xe2\x96\x88\xe2\x96\x88 \xe2\x96\x80\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x80 \xe2\x96\x88\xe2\x96\x88 \xe2\x96\x88\xe2\x96\x88   \xe2\x96\x88\xe2\x96\x88 \n"
		<< "                                                     \n"
		<< "\n";
	AppContext ctx;
	const uint32_t main_loop_period_ms = load_main_loop_period_ms(config_path);
	const bool ignore_shutdown = load_ignore_shutdown(config_path);
	CVC_SetIgnoreShutdown(ignore_shutdown ? 1 : 0);

	long long kill_delay = 0;
	const Logger::Ptr log = Logger::get("simulation");
	if (!Initial(&ctx, config_path))
	{
		log->error("application initialization failed, exiting");
		perform_application_shutdown();
#ifdef _WIN32
		system("pause");
#else
		std::cout << "Press Enter to continue..." << std::endl;
		getchar();
#endif
		return 1;
	}

	AppLauncher::launch_from_file(config_path);

#ifdef _WIN32
	ULONGLONG next_cycle_time = GetTickCount64();
	while (1)
	{
		ULONGLONG current_time = GetTickCount64();
		if (current_time < next_cycle_time)
		{
			Sleep((DWORD)(next_cycle_time - current_time));
			continue;
		}
		do
		{
			next_cycle_time += main_loop_period_ms;
		} while (next_cycle_time <= current_time);
#else
	auto next_cycle_time = std::chrono::steady_clock::now();
	while (1)
	{
		auto current_time = std::chrono::steady_clock::now();
		if (current_time < next_cycle_time)
		{
			std::this_thread::sleep_for(next_cycle_time - current_time);
			continue;
		}
		do
		{
			next_cycle_time += std::chrono::milliseconds(main_loop_period_ms);
		} while (next_cycle_time <= current_time);
#endif

		const std::chrono::steady_clock::time_point cycle_start = std::chrono::steady_clock::now();
		CVC_BSW_ITF_updateVSN();

		SyncInput(&ctx);

		MAIN_SAFETY_F_ProcessInData();

		SyncOutput(&ctx);

		check_log_config_reload(config_path);
		const long long cycle_elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - cycle_start).count();
		log->trace("cycle elapsed: %s us", format_digit_groups(cycle_elapsed_us).c_str());

		if (g_shutdown_performed)
		{
			break;
		}
		if (GetShutDownState() > 0 && !ignore_shutdown)
		{
			if ((kill_delay++) > 5)
			{
				break;
			}
		}

	}

	perform_application_shutdown();

	return 0;
}

namespace
{
void perform_application_shutdown()
{
	const bool already_shutdown = g_shutdown_performed.exchange(true);
	if (already_shutdown)
	{
		return;
	}

	AppLauncher::shutdown_auto_close_apps();
	Logger::shutdown();
}

uint32_t load_main_loop_period_ms(const std::string& config_path)
{
	std::ifstream json_file(config_path);
	if (!json_file.is_open())
	{
		return DEFAULT_MAIN_LOOP_PERIOD_MS;
	}

	try
	{
		nlohmann::json root_config;
		json_file >> root_config;

		if (!root_config.contains("cycle_ms"))
		{
			return DEFAULT_MAIN_LOOP_PERIOD_MS;
		}

		const nlohmann::json& cycle_config = root_config["cycle_ms"];
		if (cycle_config.is_number_unsigned())
		{
			const unsigned long long cycle_ms = cycle_config.get<unsigned long long>();
#ifdef _WIN32
			if (cycle_ms > 0ULL && cycle_ms <= static_cast<unsigned long long>(MAXDWORD))
			{
				return static_cast<uint32_t>(cycle_ms);
			}
#else
			if (cycle_ms > 0ULL && cycle_ms <= static_cast<unsigned long long>(UINT32_MAX))
			{
				return static_cast<uint32_t>(cycle_ms);
			}
#endif
		}
		else if (cycle_config.is_number_integer())
		{
			const long long cycle_ms = cycle_config.get<long long>();
#ifdef _WIN32
			if (cycle_ms > 0 && cycle_ms <= static_cast<long long>(MAXDWORD))
			{
				return static_cast<uint32_t>(cycle_ms);
			}
#else
			if (cycle_ms > 0 && cycle_ms <= static_cast<long long>(UINT32_MAX))
			{
				return static_cast<uint32_t>(cycle_ms);
			}
#endif
		}
	}
	catch (...)
	{
	}

	return DEFAULT_MAIN_LOOP_PERIOD_MS;
}

bool load_ignore_shutdown(const std::string& config_path)
{
	std::ifstream json_file(config_path);
	if (!json_file.is_open())
	{
		return false;
	}

	try
	{
		nlohmann::json root_config;
		json_file >> root_config;
		if (root_config.is_object()
			&& root_config.contains("ignore_shutdown")
			&& root_config["ignore_shutdown"].is_boolean())
		{
			return root_config["ignore_shutdown"].get<bool>();
		}
	}
	catch (...)
	{
	}

	return false;
}

void check_log_config_reload(const std::string& config_path)
{
	static bool s_seeded = false;
	static std::filesystem::file_time_type s_last_mtime{};

	std::error_code ec;
	const std::filesystem::file_time_type current_mtime =
		std::filesystem::last_write_time(config_path, ec);
	if (ec)
	{
		return;
	}

	if (!s_seeded)
	{
		s_seeded = true;
		s_last_mtime = current_mtime;
		return;
	}
	if (current_mtime == s_last_mtime)
	{
		return;
	}

	s_last_mtime = current_mtime;
	if (Logger::reload_from_file(config_path))
	{
		::log_info("simulation", "log config reloaded");
	}
}
}