#include "runtime/app_launcher.h"

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <thread>
#endif

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <limits.h>

#include <json.hpp>

#include "logging/logger.h"

namespace
{
enum class RunningPolicy
{
	skip,
	restart,
	launch_new
};

enum class ConsoleDisposition
{
	new_console,
	inherit,
	none
};

struct AutoStartEntry
{
	std::string target;
	std::string args;
	uint32_t delay_ms;
	bool auto_close;
	RunningPolicy if_running;
	ConsoleDisposition console_disposition;
	std::string normalized_target;
	std::string executable_name;
	std::string working_directory;
	std::size_t entry_index;
};

#ifdef _WIN32
struct ManagedProcess
{
	std::string normalized_target;
	DWORD process_id;
	HANDLE process_handle;
	std::string target;
	std::size_t entry_index;
};

const DWORD k_restart_timeout_ms = 5000U;
const DWORD k_shutdown_timeout_ms = 3000U;

std::vector<ManagedProcess> g_managed_processes;
#else
struct ManagedProcess
{
	std::string normalized_target;
	pid_t process_id;
	std::string target;
	std::size_t entry_index;
};

const uint32_t k_restart_timeout_ms = 5000U;
const uint32_t k_shutdown_timeout_ms = 3000U;

std::vector<ManagedProcess> g_managed_processes;
#endif

Logger::Ptr launcher_log()
{
	return Logger::get("simulation");
}

std::string to_lower_copy(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
		return static_cast<char>(std::tolower(character));
	});
	return value;
}

std::string normalize_slashes(std::string path)
{
#ifdef _WIN32
	std::replace(path.begin(), path.end(), '/', '\\');
#endif
	/* On Linux, just keep forward slashes */
	return path;
}

std::string extract_file_name(const std::string& path)
{
	const std::string::size_type separator = path.find_last_of("\\/");
	return separator == std::string::npos ? path : path.substr(separator + 1U);
}

std::string extract_directory(const std::string& path)
{
	const std::string::size_type separator = path.find_last_of("\\/");
	return separator == std::string::npos ? std::string() : path.substr(0U, separator);
}

bool is_bare_name(const std::string& target)
{
	return extract_directory(target).empty();
}

#ifdef _WIN32
std::string normalize_path(std::string path)
{
	if (path.empty())
	{
		return path;
	}

	path = normalize_slashes(path);
	const DWORD required = GetFullPathNameA(path.c_str(), 0U, NULL, NULL);
	if (required > 0U)
	{
		std::vector<char> buffer(required);
		const DWORD length = GetFullPathNameA(path.c_str(), required, buffer.data(), NULL);
		if (length > 0U && length < required)
		{
			path.assign(buffer.data(), length);
		}
	}

	return to_lower_copy(normalize_slashes(path));
}
#else
std::string normalize_path(std::string path)
{
	if (path.empty())
	{
		return path;
	}

	path = normalize_slashes(path);

	/* Use realpath() to canonicalize */
	char* resolved = realpath(path.c_str(), NULL);
	if (resolved != NULL)
	{
		path = resolved;
		free(resolved);
	}

	return to_lower_copy(normalize_slashes(path));
}
#endif

std::string build_command_line(const AutoStartEntry& entry)
{
	std::string command_line = "\"" + entry.target + "\"";
	if (!entry.args.empty())
	{
		command_line.push_back(' ');
		command_line += entry.args;
	}
	return command_line;
}

uint32_t parse_delay_ms_internal(const nlohmann::json& value, bool* ok)
{
	*ok = false;

	if (value.is_number_unsigned())
	{
		const unsigned long long parsed_value = value.get<unsigned long long>();
#ifdef _WIN32
		if (parsed_value <= static_cast<unsigned long long>(MAXDWORD))
#else
		if (parsed_value <= static_cast<unsigned long long>(UINT32_MAX))
#endif
		{
			*ok = true;
			return static_cast<uint32_t>(parsed_value);
		}
		return 0U;
	}

	if (value.is_number_integer())
	{
		const long long parsed_value = value.get<long long>();
#ifdef _WIN32
		if (parsed_value >= 0LL && parsed_value <= static_cast<long long>(MAXDWORD))
#else
		if (parsed_value >= 0LL && parsed_value <= static_cast<long long>(UINT32_MAX))
#endif
		{
			*ok = true;
			return static_cast<uint32_t>(parsed_value);
		}
	}

	return 0U;
}

bool parse_running_policy(const nlohmann::json& value, RunningPolicy* policy)
{
	if (!value.is_string())
	{
		return false;
	}

	const std::string normalized_value = to_lower_copy(value.get<std::string>());
	if (normalized_value == "skip")
	{
		*policy = RunningPolicy::skip;
		return true;
	}

	if (normalized_value == "restart")
	{
		*policy = RunningPolicy::restart;
		return true;
	}

	if (normalized_value == "launch_new")
	{
		*policy = RunningPolicy::launch_new;
		return true;
	}

	return false;
}

bool parse_console_disposition(const nlohmann::json& value, ConsoleDisposition* disposition)
{
	if (!value.is_string())
	{
		return false;
	}

	const std::string normalized_value = to_lower_copy(value.get<std::string>());
	if (normalized_value == "new")
	{
		*disposition = ConsoleDisposition::new_console;
		return true;
	}

	if (normalized_value == "inherit")
	{
		*disposition = ConsoleDisposition::inherit;
		return true;
	}

	if (normalized_value == "none")
	{
		*disposition = ConsoleDisposition::none;
		return true;
	}

	return false;
}

bool parse_entry(const nlohmann::json& entry_config, std::size_t entry_index, AutoStartEntry* entry)
{
	Logger::Ptr log = launcher_log();
	if (!entry_config.is_object())
	{
		log->warn("launcher[%u] ignored: entry must be an object", static_cast<unsigned>(entry_index));
		return false;
	}

	AutoStartEntry parsed_entry;
	parsed_entry.args = std::string();
	parsed_entry.delay_ms = 0U;
	parsed_entry.auto_close = false;
	parsed_entry.if_running = RunningPolicy::skip;
	parsed_entry.console_disposition = ConsoleDisposition::new_console;
	parsed_entry.entry_index = entry_index;

	if (entry_config.contains("enabled"))
	{
		if (!entry_config["enabled"].is_boolean())
		{
			log->warn("launcher[%u] ignored: enabled must be a boolean", static_cast<unsigned>(entry_index));
			return false;
		}
		if (!entry_config["enabled"].get<bool>())
		{
			log->info("launcher[%u] skipped because enabled is false", static_cast<unsigned>(entry_index));
			return false;
		}
	}

	if (!entry_config.contains("target") || !entry_config["target"].is_string())
	{
		log->warn("launcher[%u] ignored: target is required and must be a string", static_cast<unsigned>(entry_index));
		return false;
	}

	parsed_entry.target = entry_config["target"].get<std::string>();
	if (parsed_entry.target.empty())
	{
		log->warn("launcher[%u] ignored: target must not be empty", static_cast<unsigned>(entry_index));
		return false;
	}

	if (entry_config.contains("args"))
	{
		if (!entry_config["args"].is_string())
		{
			log->warn("launcher[%u] ignored: args must be a string", static_cast<unsigned>(entry_index));
			return false;
		}
		parsed_entry.args = entry_config["args"].get<std::string>();
	}

	bool delay_ok = false;
	if (entry_config.contains("delay_ms"))
	{
		parsed_entry.delay_ms = parse_delay_ms_internal(entry_config["delay_ms"], &delay_ok);
		if (!delay_ok)
		{
			log->warn("launcher[%u] ignored: delay_ms must be a non-negative integer within range", static_cast<unsigned>(entry_index));
			return false;
		}
	}

	if (entry_config.contains("auto_close"))
	{
		if (!entry_config["auto_close"].is_boolean())
		{
			log->warn("launcher[%u] ignored: auto_close must be a boolean", static_cast<unsigned>(entry_index));
			return false;
		}
		parsed_entry.auto_close = entry_config["auto_close"].get<bool>();
	}

	if (entry_config.contains("if_running") && !parse_running_policy(entry_config["if_running"], &parsed_entry.if_running))
	{
		log->warn("launcher[%u] ignored: if_running must be skip, restart, or launch_new", static_cast<unsigned>(entry_index));
		return false;
	}

	if (entry_config.contains("console") && !parse_console_disposition(entry_config["console"], &parsed_entry.console_disposition))
	{
		log->warn("launcher[%u] ignored: console must be new, inherit, or none", static_cast<unsigned>(entry_index));
		return false;
	}

	if (entry_config.contains("working_dir"))
	{
		if (!entry_config["working_dir"].is_string())
		{
			log->warn("launcher[%u] ignored: working_dir must be a string", static_cast<unsigned>(entry_index));
			return false;
		}
		parsed_entry.working_directory = entry_config["working_dir"].get<std::string>();
	}
	else
	{
		parsed_entry.working_directory = extract_directory(parsed_entry.target);
	}
	parsed_entry.normalized_target = normalize_path(parsed_entry.target);
	parsed_entry.executable_name = extract_file_name(parsed_entry.normalized_target);
	if (is_bare_name(parsed_entry.target) && parsed_entry.executable_name.find('.') == std::string::npos)
	{
#ifdef _WIN32
		parsed_entry.executable_name += ".exe";
#endif
		/* On Linux, executables typically don't have extensions */
	}
	*entry = parsed_entry;
	return true;
}

std::vector<AutoStartEntry> load_entries(const std::string& config_path)
{
	Logger::Ptr log = launcher_log();
	std::ifstream config_file(config_path.c_str());
	if (!config_file.is_open())
	{
		log->warn("launcher skipped: failed to open config file: %s", config_path.c_str());
		return std::vector<AutoStartEntry>();
	}

	nlohmann::json root_config;
	try
	{
		config_file >> root_config;
	}
	catch (const std::exception& ex)
	{
		log->error("launcher skipped: failed to parse config file %s: %s", config_path.c_str(), ex.what());
		return std::vector<AutoStartEntry>();
	}

	if (!root_config.contains("launcher"))
	{
		return std::vector<AutoStartEntry>();
	}

	const nlohmann::json& launcher_config = root_config["launcher"];
	if (!launcher_config.is_array())
	{
		log->warn("launcher skipped: launcher must be an array");
		return std::vector<AutoStartEntry>();
	}

	std::vector<AutoStartEntry> entries;
	entries.reserve(launcher_config.size());
	for (std::size_t index = 0U; index < launcher_config.size(); ++index)
	{
		AutoStartEntry entry;
		if (parse_entry(launcher_config[index], index, &entry))
		{
			entries.push_back(entry);
		}
	}

	return entries;
}

/* ── Platform-specific process management ── */

#ifdef _WIN32

std::string query_process_image_path(HANDLE process_handle)
{
	DWORD buffer_size = MAX_PATH;
	std::vector<char> buffer(buffer_size);
	if (QueryFullProcessImageNameA(process_handle, 0U, buffer.data(), &buffer_size) != FALSE)
	{
		return normalize_path(std::string(buffer.data(), buffer_size));
	}

	return std::string();
}

bool process_matches_target(const PROCESSENTRY32& process_entry, const AutoStartEntry& target)
{
	if (process_entry.th32ProcessID == 0U || process_entry.th32ProcessID == GetCurrentProcessId())
	{
		return false;
	}

	const std::string process_name = to_lower_copy(process_entry.szExeFile);
	if (process_name != target.executable_name)
	{
		return false;
	}

	if (is_bare_name(target.target))
	{
		return true;
	}

	HANDLE process_handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_entry.th32ProcessID);
	if (process_handle == NULL)
	{
		return true;
	}

	const std::string process_path = query_process_image_path(process_handle);
	CloseHandle(process_handle);
	if (process_path.empty())
	{
		return true;
	}

	return process_path == target.normalized_target;
}

std::vector<DWORD> find_matching_process_ids(const AutoStartEntry& target)
{
	std::vector<DWORD> process_ids;
	HANDLE snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0U);
	if (snapshot_handle == INVALID_HANDLE_VALUE)
	{
		launcher_log()->warn("launcher[%u] process scan failed: CreateToolhelp32Snapshot error=%lu",
			static_cast<unsigned>(target.entry_index),
			GetLastError());
		return process_ids;
	}

	PROCESSENTRY32 process_entry;
	ZeroMemory(&process_entry, sizeof(process_entry));
	process_entry.dwSize = sizeof(process_entry);
	if (Process32First(snapshot_handle, &process_entry) != FALSE)
	{
		do
		{
			if (process_matches_target(process_entry, target))
			{
				process_ids.push_back(process_entry.th32ProcessID);
			}
		} while (Process32Next(snapshot_handle, &process_entry) != FALSE);
	}

	CloseHandle(snapshot_handle);
	return process_ids;
}

void untrack_processes(const std::vector<DWORD>& process_ids)
{
	for (std::vector<ManagedProcess>::iterator iterator = g_managed_processes.begin(); iterator != g_managed_processes.end();)
	{
		const bool should_remove = std::find(process_ids.begin(), process_ids.end(), iterator->process_id) != process_ids.end();
		if (!should_remove)
		{
			++iterator;
			continue;
		}

		if (iterator->process_handle != NULL)
		{
			CloseHandle(iterator->process_handle);
		}
		iterator = g_managed_processes.erase(iterator);
	}
}

bool terminate_process_id(DWORD process_id, DWORD timeout_ms)
{
	HANDLE process_handle = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, process_id);
	if (process_handle == NULL)
	{
		launcher_log()->warn("failed to open process %lu for termination: error=%lu", process_id, GetLastError());
		return false;
	}

	bool terminated = true;
	if (TerminateProcess(process_handle, 1U) == FALSE)
	{
		launcher_log()->warn("failed to terminate process %lu: error=%lu", process_id, GetLastError());
		terminated = false;
	}
	else if (WaitForSingleObject(process_handle, timeout_ms) != WAIT_OBJECT_0)
	{
		launcher_log()->warn("timeout waiting for process %lu to exit", process_id);
		terminated = false;
	}

	CloseHandle(process_handle);
	return terminated;
}

bool restart_matching_processes(const AutoStartEntry& entry, const std::vector<DWORD>& process_ids)
{
	Logger::Ptr log = launcher_log();
	if (process_ids.empty())
	{
		return true;
	}

	log->info("launcher[%u] restarting %u existing instance(s) of %s",
		static_cast<unsigned>(entry.entry_index),
		static_cast<unsigned>(process_ids.size()),
		entry.target.c_str());

	untrack_processes(process_ids);
	bool terminated_all = true;
	for (std::vector<DWORD>::const_iterator iterator = process_ids.begin(); iterator != process_ids.end(); ++iterator)
	{
		if (!terminate_process_id(*iterator, k_restart_timeout_ms))
		{
			terminated_all = false;
		}
	}

	if (!terminated_all)
	{
		log->warn("launcher[%u] restart aborted because one or more existing processes could not be terminated",
			static_cast<unsigned>(entry.entry_index));
	}

	return terminated_all;
}

void track_launched_process(const AutoStartEntry& entry, const PROCESS_INFORMATION& process_info)
{
	ManagedProcess process;
	process.normalized_target = entry.normalized_target;
	process.process_id = process_info.dwProcessId;
	process.process_handle = process_info.hProcess;
	process.target = entry.target;
	process.entry_index = entry.entry_index;
	g_managed_processes.push_back(process);
}

void launch_entry(const AutoStartEntry& entry)
{
	Logger::Ptr log = launcher_log();
	if (entry.delay_ms > 0U)
	{
		Sleep(entry.delay_ms);
	}

	const std::vector<DWORD> matching_process_ids = find_matching_process_ids(entry);
	if (!matching_process_ids.empty())
	{
		if (entry.if_running == RunningPolicy::skip)
		{
			log->info("launcher[%u] skipped because %s is already running",
				static_cast<unsigned>(entry.entry_index),
				entry.target.c_str());
			return;
		}

		if (entry.if_running == RunningPolicy::restart && !restart_matching_processes(entry, matching_process_ids))
		{
			return;
		}
	}

	std::string command_line = build_command_line(entry);
	std::vector<char> command_line_buffer(command_line.begin(), command_line.end());
	command_line_buffer.push_back('\0');

	STARTUPINFOA startup_info;
	PROCESS_INFORMATION process_info;
	ZeroMemory(&startup_info, sizeof(startup_info));
	ZeroMemory(&process_info, sizeof(process_info));
	startup_info.cb = sizeof(startup_info);

	DWORD creation_flags = 0U;
	switch (entry.console_disposition)
	{
	case ConsoleDisposition::new_console:
		creation_flags = CREATE_NEW_CONSOLE;
		break;
	case ConsoleDisposition::inherit:
		creation_flags = 0U;
		break;
	case ConsoleDisposition::none:
		creation_flags = CREATE_NO_WINDOW;
		break;
	}

	const BOOL created = CreateProcessA(
		is_bare_name(entry.target) ? NULL : entry.target.c_str(),
		command_line_buffer.data(),
		NULL,
		NULL,
		FALSE,
		creation_flags,
		NULL,
		entry.working_directory.empty() ? NULL : entry.working_directory.c_str(),
		&startup_info,
		&process_info);
	if (created == FALSE)
	{
		log->error("launcher[%u] failed to launch %s: error=%lu",
			static_cast<unsigned>(entry.entry_index),
			entry.target.c_str(),
			GetLastError());
		return;
	}

	log->info("launcher[%u] launched %s pid=%lu",
		static_cast<unsigned>(entry.entry_index),
		entry.target.c_str(),
		process_info.dwProcessId);

	CloseHandle(process_info.hThread);
	if (entry.auto_close)
	{
		track_launched_process(entry, process_info);
	}
	else
	{
		CloseHandle(process_info.hProcess);
	}
}

void shutdown_managed_process(ManagedProcess* process)
{
	Logger::Ptr log = launcher_log();
	if (process->process_handle == NULL)
	{
		return;
	}

	const DWORD wait_result = WaitForSingleObject(process->process_handle, 0U);
	if (wait_result == WAIT_TIMEOUT)
	{
		if (TerminateProcess(process->process_handle, 0U) == FALSE)
		{
			log->warn("auto_close failed for pid=%lu target=%s: error=%lu",
				process->process_id,
				process->target.c_str(),
				GetLastError());
		}
		else if (WaitForSingleObject(process->process_handle, k_shutdown_timeout_ms) != WAIT_OBJECT_0)
		{
			log->warn("auto_close timed out for pid=%lu target=%s",
				process->process_id,
				process->target.c_str());
		}
		else
		{
			log->info("auto_close terminated pid=%lu target=%s",
				process->process_id,
				process->target.c_str());
		}
	}
	else if (wait_result == WAIT_OBJECT_0)
	{
		log->info("auto_close process already exited pid=%lu target=%s",
			process->process_id,
			process->target.c_str());
	}
	else
	{
		const DWORD last_error = GetLastError();
		log->warn("auto_close wait check failed for pid=%lu target=%s: error=%lu",
			process->process_id,
			process->target.c_str(),
			last_error);
	}

	CloseHandle(process->process_handle);
	process->process_handle = NULL;
}

#else /* POSIX process management */

std::vector<pid_t> find_matching_process_ids(const AutoStartEntry& target)
{
	std::vector<pid_t> process_ids;

	/* Scan /proc for processes matching the executable name */
	DIR* proc_dir = opendir("/proc");
	if (proc_dir == nullptr)
	{
		return process_ids;
	}

	pid_t my_pid = getpid();

	struct dirent* entry;
	while ((entry = readdir(proc_dir)) != nullptr)
	{
		/* Only process numeric directories */
		if (entry->d_type != DT_DIR)
			continue;

		pid_t pid = (pid_t)atoi(entry->d_name);
		if (pid <= 0 || pid == my_pid)
			continue;

		/* Read /proc/<pid>/comm to get the process name */
		char comm_path[64];
		snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
		FILE* comm_file = fopen(comm_path, "r");
		if (comm_file == nullptr)
			continue;

		char comm_name[256] = {0};
		if (fgets(comm_name, sizeof(comm_name), comm_file) != nullptr)
		{
			/* Strip trailing newline */
			size_t len = strlen(comm_name);
			if (len > 0 && comm_name[len-1] == '\n')
				comm_name[len-1] = '\0';

			std::string proc_name = to_lower_copy(comm_name);
			if (proc_name == target.executable_name)
			{
				process_ids.push_back(pid);
			}
		}
		fclose(comm_file);
	}

	closedir(proc_dir);
	return process_ids;
}

void untrack_processes(const std::vector<pid_t>& process_ids)
{
	for (std::vector<ManagedProcess>::iterator iterator = g_managed_processes.begin();
	     iterator != g_managed_processes.end();)
	{
		const bool should_remove = std::find(process_ids.begin(), process_ids.end(), iterator->process_id) != process_ids.end();
		if (!should_remove)
		{
			++iterator;
			continue;
		}
		iterator = g_managed_processes.erase(iterator);
	}
}

bool terminate_process_id(pid_t process_id, uint32_t timeout_ms)
{
	/* Send SIGTERM first */
	if (kill(process_id, SIGTERM) != 0)
	{
		launcher_log()->warn("failed to send SIGTERM to process %d: %s", process_id, strerror(errno));
		return false;
	}

	/* Wait for process to exit with timeout */
	uint32_t elapsed = 0U;
	const uint32_t poll_interval_ms = 100U;
	while (elapsed < timeout_ms)
	{
		int status = 0;
		pid_t result = waitpid(process_id, &status, WNOHANG);
		if (result == process_id)
		{
			return true; /* Process exited */
		}
		if (result < 0 && errno == ECHILD)
		{
			return true; /* No such process - already gone */
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
		elapsed += poll_interval_ms;
	}

	/* Timeout - force kill */
	launcher_log()->warn("timeout waiting for process %d to exit, sending SIGKILL", process_id);
	kill(process_id, SIGKILL);
	return false;
}

bool restart_matching_processes(const AutoStartEntry& entry, const std::vector<pid_t>& process_ids)
{
	Logger::Ptr log = launcher_log();
	if (process_ids.empty())
	{
		return true;
	}

	log->info("launcher[%u] restarting %u existing instance(s) of %s",
		static_cast<unsigned>(entry.entry_index),
		static_cast<unsigned>(process_ids.size()),
		entry.target.c_str());

	untrack_processes(process_ids);
	bool terminated_all = true;
	for (std::vector<pid_t>::const_iterator iterator = process_ids.begin(); iterator != process_ids.end(); ++iterator)
	{
		if (!terminate_process_id(*iterator, k_restart_timeout_ms))
		{
			terminated_all = false;
		}
	}

	if (!terminated_all)
	{
		log->warn("launcher[%u] restart aborted because one or more existing processes could not be terminated",
			static_cast<unsigned>(entry.entry_index));
	}

	return terminated_all;
}

void track_launched_process(const AutoStartEntry& entry, pid_t pid)
{
	ManagedProcess process;
	process.normalized_target = entry.normalized_target;
	process.process_id = pid;
	process.target = entry.target;
	process.entry_index = entry.entry_index;
	g_managed_processes.push_back(process);
}

void launch_entry(const AutoStartEntry& entry)
{
	Logger::Ptr log = launcher_log();
	if (entry.delay_ms > 0U)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(entry.delay_ms));
	}

	const std::vector<pid_t> matching_process_ids = find_matching_process_ids(entry);
	if (!matching_process_ids.empty())
	{
		if (entry.if_running == RunningPolicy::skip)
		{
			log->info("launcher[%u] skipped because %s is already running",
				static_cast<unsigned>(entry.entry_index),
				entry.target.c_str());
			return;
		}

		if (entry.if_running == RunningPolicy::restart && !restart_matching_processes(entry, matching_process_ids))
		{
			return;
		}
	}

	pid_t pid = fork();
	if (pid < 0)
	{
		log->error("launcher[%u] failed to fork for %s: %s",
			static_cast<unsigned>(entry.entry_index),
			entry.target.c_str(),
			strerror(errno));
		return;
	}

	if (pid == 0)
	{
		/* Child process */
		if (!entry.working_directory.empty())
		{
			chdir(entry.working_directory.c_str());
		}

		/* Build argument list */
		std::vector<std::string> arg_strings;
		arg_strings.push_back(entry.target);
		/* Simple space-split of args (matches Windows CreateProcess behavior) */
		if (!entry.args.empty())
		{
			std::string current_arg;
			for (std::size_t i = 0; i < entry.args.size(); ++i)
			{
				if (entry.args[i] == ' ' && !current_arg.empty())
				{
					arg_strings.push_back(current_arg);
					current_arg.clear();
				}
				else if (entry.args[i] != ' ')
				{
					current_arg += entry.args[i];
				}
			}
			if (!current_arg.empty())
			{
				arg_strings.push_back(current_arg);
			}
		}

		std::vector<char*> argv;
		for (std::size_t i = 0; i < arg_strings.size(); ++i)
		{
			argv.push_back(const_cast<char*>(arg_strings[i].c_str()));
		}
		argv.push_back(nullptr);

		execvp(entry.target.c_str(), argv.data());

		/* execvp failed */
		log->error("launcher[%u] execvp failed for %s: %s",
			static_cast<unsigned>(entry.entry_index),
			entry.target.c_str(),
			strerror(errno));
		_exit(1);
	}

	/* Parent process */
	log->info("launcher[%u] launched %s pid=%d",
		static_cast<unsigned>(entry.entry_index),
		entry.target.c_str(),
		pid);

	if (entry.auto_close)
	{
		track_launched_process(entry, pid);
	}
}

void shutdown_managed_process(ManagedProcess* process)
{
	Logger::Ptr log = launcher_log();

	/* Check if process is still alive */
	int status = 0;
	pid_t result = waitpid(process->process_id, &status, WNOHANG);

	if (result == 0)
	{
		/* Process still running - terminate */
		if (kill(process->process_id, SIGTERM) == 0)
		{
			/* Wait with timeout */
			uint32_t elapsed = 0U;
			const uint32_t poll_interval_ms = 100U;
			bool exited = false;
			while (elapsed < k_shutdown_timeout_ms)
			{
				result = waitpid(process->process_id, &status, WNOHANG);
				if (result == process->process_id)
				{
					exited = true;
					break;
				}
				if (result < 0 && errno == ECHILD)
				{
					exited = true;
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
				elapsed += poll_interval_ms;
			}

			if (!exited)
			{
				log->warn("auto_close timed out for pid=%d target=%s, sending SIGKILL",
					process->process_id,
					process->target.c_str());
				kill(process->process_id, SIGKILL);
			}
			else
			{
				log->info("auto_close terminated pid=%d target=%s",
					process->process_id,
					process->target.c_str());
			}
		}
		else
		{
			log->warn("auto_close failed to send SIGTERM to pid=%d target=%s: %s",
				process->process_id,
				process->target.c_str(),
				strerror(errno));
		}
	}
	else if (result == process->process_id || (result < 0 && errno == ECHILD))
	{
		log->info("auto_close process already exited pid=%d target=%s",
			process->process_id,
			process->target.c_str());
	}
	else
	{
		log->warn("auto_close wait check failed for pid=%d target=%s: %s",
			process->process_id,
			process->target.c_str(),
			strerror(errno));
	}
}

#endif /* _WIN32 / POSIX */

} /* anonymous namespace */

void AppLauncher::launch_from_file(const std::string& config_path)
{
	const std::vector<AutoStartEntry> entries = load_entries(config_path);
	for (std::vector<AutoStartEntry>::const_iterator iterator = entries.begin(); iterator != entries.end(); ++iterator)
	{
		launch_entry(*iterator);
	}
}

void AppLauncher::shutdown_auto_close_apps()
{
	for (std::vector<ManagedProcess>::reverse_iterator iterator = g_managed_processes.rbegin(); iterator != g_managed_processes.rend(); ++iterator)
	{
		shutdown_managed_process(&(*iterator));
	}
	g_managed_processes.clear();
}