#include "logger.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <ctime>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <json.hpp>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace {

using json = nlohmann::json;

const char* const k_default_console_pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v";
const char* const k_default_file_pattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v";
const char* const k_default_file_name = "{module}.log";
const char* const k_default_config_path = "simu_config.json";

struct sink_config
{
	bool enabled = true;
	log_level_t level = LOG_LEVEL_INFO;
	log_level_t flush_level = LOG_LEVEL_ERROR;
	std::string pattern;
};

struct console_sink_config : sink_config
{
	bool use_color = true;
};

struct file_sink_config : sink_config
{
	std::string directory = "logs";
	std::string file_name = k_default_file_name;
	int max_file_size_bytes = 5 * 1024 * 1024;
	int max_files = 5;
};

struct module_config
{
	console_sink_config console;
	file_sink_config local_file;
};

struct runtime_config
{
	module_config defaults;
	std::unordered_map<std::string, module_config> modules;
	int async_queue_size = 8192;
	int async_thread_count = 1;
};

struct logger_state
{
	std::mutex mutex;
	bool initialized = false;
	runtime_config config;
	std::unordered_map<std::string, log_level_t> logger_levels;
	std::unordered_set<std::string> disabled_loggers;
	bool has_global_level_override = false;
	log_level_t global_level_override = LOG_LEVEL_INFO;
	std::string run_file_prefix;
};

logger_state& state()
{
	static logger_state instance;
	return instance;
}

void enable_console_virtual_terminal_processing() noexcept
{
#ifdef _WIN32
	const HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (console_handle == nullptr || console_handle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	DWORD console_mode = 0;
	if (GetConsoleMode(console_handle, &console_mode) == 0)
	{
		return;
	}

	if ((console_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0)
	{
		return;
	}

	SetConsoleMode(console_handle, console_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

Logger::Config sanitize_basic_config(const Logger::Config& config)
{
	Logger::Config sanitized = config;

	if (sanitized.log_dir.empty())
	{
		sanitized.log_dir = "logs";
	}

	if (sanitized.max_file_size <= 0)
	{
		sanitized.max_file_size = 5 * 1024 * 1024;
	}

	if (sanitized.max_files <= 0)
	{
		sanitized.max_files = 5;
	}

	if (sanitized.async_queue_size <= 0)
	{
		sanitized.async_queue_size = 8192;
	}

	if (sanitized.async_thread_count <= 0)
	{
		sanitized.async_thread_count = 1;
	}

	if (sanitized.level < LOG_LEVEL_TRACE || sanitized.level > LOG_LEVEL_OFF)
	{
		sanitized.level = LOG_LEVEL_INFO;
	}

	return sanitized;
}

std::string to_lower_copy(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
		{
			return static_cast<char>(std::tolower(ch));
		});
	return value;
}

spdlog::level::level_enum to_spdlog_level(log_level_t level)
{
	switch (level)
	{
	case LOG_LEVEL_TRACE:
		return spdlog::level::trace;
	case LOG_LEVEL_DEBUG:
		return spdlog::level::debug;
	case LOG_LEVEL_INFO:
		return spdlog::level::info;
	case LOG_LEVEL_WARN:
		return spdlog::level::warn;
	case LOG_LEVEL_ERROR:
		return spdlog::level::err;
	case LOG_LEVEL_CRITICAL:
		return spdlog::level::critical;
	default:
		return spdlog::level::off;
	}
}

log_level_t sanitize_level(log_level_t level, log_level_t fallback)
{
	if (level < LOG_LEVEL_TRACE || level > LOG_LEVEL_OFF)
	{
		return fallback;
	}

	return level;
}

log_level_t parse_level_string(const std::string& value, log_level_t fallback)
{
	const std::string lowered = to_lower_copy(value);
	if (lowered == "trace")
	{
		return LOG_LEVEL_TRACE;
	}
	if (lowered == "debug")
	{
		return LOG_LEVEL_DEBUG;
	}
	if (lowered == "info")
	{
		return LOG_LEVEL_INFO;
	}
	if (lowered == "warn" || lowered == "warning")
	{
		return LOG_LEVEL_WARN;
	}
	if (lowered == "error" || lowered == "err")
	{
		return LOG_LEVEL_ERROR;
	}
	if (lowered == "critical")
	{
		return LOG_LEVEL_CRITICAL;
	}
	if (lowered == "off")
	{
		return LOG_LEVEL_OFF;
	}

	return fallback;
}

runtime_config build_runtime_config(const Logger::Config& config)
{
	const Logger::Config sanitized = sanitize_basic_config(config);
	runtime_config runtime;
	runtime.defaults.console.enabled = sanitized.enable_console;
	runtime.defaults.console.level = sanitized.level;
	runtime.defaults.console.flush_level = LOG_LEVEL_ERROR;
	runtime.defaults.console.use_color = true;
	runtime.defaults.console.pattern = k_default_console_pattern;
	runtime.defaults.local_file.enabled = true;
	runtime.defaults.local_file.level = sanitized.level;
	runtime.defaults.local_file.flush_level = LOG_LEVEL_ERROR;
	runtime.defaults.local_file.directory = sanitized.log_dir;
	runtime.defaults.local_file.file_name = k_default_file_name;
	runtime.defaults.local_file.pattern = k_default_file_pattern;
	runtime.defaults.local_file.max_file_size_bytes = sanitized.max_file_size;
	runtime.defaults.local_file.max_files = sanitized.max_files;
	runtime.async_queue_size = sanitized.async_queue_size;
	runtime.async_thread_count = sanitized.async_thread_count;
	return runtime;
}

void sanitize_sink_config(sink_config& config, const char* default_pattern)
{
	config.level = sanitize_level(config.level, LOG_LEVEL_INFO);
	config.flush_level = sanitize_level(config.flush_level, LOG_LEVEL_ERROR);
	if (config.pattern.empty())
	{
		config.pattern = default_pattern;
	}
}

void sanitize_console_config(console_sink_config& config)
{
	sanitize_sink_config(config, k_default_console_pattern);
}

void sanitize_file_config(file_sink_config& config, const std::string& fallback_directory)
{
	sanitize_sink_config(config, k_default_file_pattern);
	if (config.directory.empty())
	{
		config.directory = fallback_directory.empty() ? std::string("logs") : fallback_directory;
	}
	if (config.file_name.empty())
	{
		config.file_name = k_default_file_name;
	}
	if (config.max_file_size_bytes <= 0)
	{
		config.max_file_size_bytes = 5 * 1024 * 1024;
	}
	if (config.max_files <= 0)
	{
		config.max_files = 5;
	}
}

runtime_config sanitize_runtime_config(runtime_config config)
{
	if (config.async_queue_size <= 0)
	{
		config.async_queue_size = 8192;
	}
	if (config.async_thread_count <= 0)
	{
		config.async_thread_count = 1;
	}
	sanitize_console_config(config.defaults.console);
	sanitize_file_config(config.defaults.local_file, config.defaults.local_file.directory);
	for (std::unordered_map<std::string, module_config>::iterator entry = config.modules.begin(); entry != config.modules.end(); ++entry)
	{
		sanitize_console_config(entry->second.console);
		sanitize_file_config(entry->second.local_file, config.defaults.local_file.directory);
	}
	return config;
}

std::string make_run_directory_name()
{
	std::time_t now = std::time(nullptr);
	std::tm local_time = {};
#ifdef _MSC_VER
	localtime_s(&local_time, &now);
#else
	localtime_r(&now, &local_time);
#endif
	char buffer[32] = {};
	std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &local_time);
	return std::string(buffer);
}

template <typename T>
void load_common_sink_fields(const json& node, T& config)
{
	if (!node.is_object())
	{
		return;
	}

	if (node.contains("enabled") && node["enabled"].is_boolean())
	{
		config.enabled = node["enabled"].get<bool>();
	}
	if (node.contains("level") && node["level"].is_string())
	{
		config.level = parse_level_string(node["level"].get<std::string>(), config.level);
	}
	if (node.contains("flush_level") && node["flush_level"].is_string())
	{
		config.flush_level = parse_level_string(node["flush_level"].get<std::string>(), config.flush_level);
	}
	if (node.contains("pattern") && node["pattern"].is_string())
	{
		config.pattern = node["pattern"].get<std::string>();
	}
}

void load_console_sink_fields(const json& node, console_sink_config& config)
{
	load_common_sink_fields(node, config);
	if (node.contains("use_color") && node["use_color"].is_boolean())
	{
		config.use_color = node["use_color"].get<bool>();
	}
}

void load_file_sink_fields(const json& node, file_sink_config& config)
{
	load_common_sink_fields(node, config);
	if (node.contains("directory") && node["directory"].is_string())
	{
		config.directory = node["directory"].get<std::string>();
	}
	if (node.contains("file_name") && node["file_name"].is_string())
	{
		config.file_name = node["file_name"].get<std::string>();
	}
	if (node.contains("rotation") && node["rotation"].is_object())
	{
		const json& rotation = node["rotation"];
		if (rotation.contains("max_file_size_mb") && rotation["max_file_size_mb"].is_number_integer())
		{
			config.max_file_size_bytes = rotation["max_file_size_mb"].get<int>() * 1024 * 1024;
		}
		if (rotation.contains("max_files") && rotation["max_files"].is_number_integer())
		{
			config.max_files = rotation["max_files"].get<int>();
		}
	}
}

runtime_config load_runtime_config_from_file(const std::string& config_path, const Logger::Config& fallback)
{
	runtime_config runtime = build_runtime_config(fallback);
	std::ifstream input(config_path.c_str(), std::ios::in | std::ios::binary);
	if (!input)
	{
		throw std::runtime_error("log config file not found");
	}

	json root;
	input >> root;

	const json& log_root = (root.contains("log") && root["log"].is_object()) ? root["log"] : root;

	if (log_root.contains("default_console") && log_root["default_console"].is_object())
	{
		load_console_sink_fields(log_root["default_console"], runtime.defaults.console);
	}
	if (log_root.contains("default_file") && log_root["default_file"].is_object())
	{
		load_file_sink_fields(log_root["default_file"], runtime.defaults.local_file);
	}

	if (log_root.contains("modules") && log_root["modules"].is_object())
	{
		for (json::const_iterator module_it = log_root["modules"].begin(); module_it != log_root["modules"].end(); ++module_it)
		{
			if (!module_it.value().is_object())
			{
				continue;
			}

			module_config module = runtime.defaults;
			const json& module_node = module_it.value();
			if (module_node.contains("console"))
			{
				const json& console_node = module_node["console"];
				if (console_node.is_boolean())
				{
					module.console.enabled = console_node.get<bool>();
				}
				else if (console_node.is_object())
				{
					load_console_sink_fields(console_node, module.console);
				}
			}
			if (module_node.contains("local_file"))
			{
				const json& file_node = module_node["local_file"];
				if (file_node.is_boolean())
				{
					module.local_file.enabled = file_node.get<bool>();
				}
				else if (file_node.is_object())
				{
					load_file_sink_fields(file_node, module.local_file);
				}
			}
			runtime.modules[module_it.key()] = module;
		}
	}

	return sanitize_runtime_config(runtime);
}

const char* level_name(log_level_t level) noexcept
{
	switch (level)
	{
	case LOG_LEVEL_TRACE:
		return "TRACE";
	case LOG_LEVEL_DEBUG:
		return "DEBUG";
	case LOG_LEVEL_INFO:
		return "INFO";
	case LOG_LEVEL_WARN:
		return "WARN";
	case LOG_LEVEL_ERROR:
		return "ERROR";
	case LOG_LEVEL_CRITICAL:
		return "CRITICAL";
	default:
		return "OFF";
	}
}

std::string normalize_name(const std::string& name)
{
	return name.empty() ? std::string("app") : name;
}

std::string normalize_name(const char* name)
{
	return (name == nullptr || name[0] == '\0') ? std::string("app") : std::string(name);
}

std::string replace_module_token(std::string value, const std::string& module_name)
{
	const std::string token = "{module}";
	std::string::size_type pos = value.find(token);
	while (pos != std::string::npos)
	{
		value.replace(pos, token.length(), module_name);
		pos = value.find(token, pos + module_name.length());
	}
	return value;
}

std::string strip_color_markers(std::string pattern)
{
	const std::string color_start = "%^";
	const std::string color_end = "%$";
	std::string::size_type pos = pattern.find(color_start);
	while (pos != std::string::npos)
	{
		pattern.erase(pos, color_start.length());
		pos = pattern.find(color_start, pos);
	}
	pos = pattern.find(color_end);
	while (pos != std::string::npos)
	{
		pattern.erase(pos, color_end.length());
		pos = pattern.find(color_end, pos);
	}
	return pattern;
}

std::string format_message(const char* fmt, va_list args)
{
	if (fmt == nullptr)
	{
		return std::string();
	}

	va_list args_copy;
	va_copy(args_copy, args);
#ifdef _MSC_VER
	const int length = _vscprintf(fmt, args_copy);
	va_end(args_copy);
	if (length <= 0)
	{
		return std::string(fmt);
	}
	std::vector<char> buffer(static_cast<std::size_t>(length) + 1U, '\0');
	vsnprintf_s(buffer.data(), buffer.size(), _TRUNCATE, fmt, args);
#else
	const int length = vsnprintf(NULL, 0, fmt, args_copy);
	va_end(args_copy);
	if (length <= 0)
	{
		return std::string(fmt);
	}
	std::vector<char> buffer(static_cast<std::size_t>(length) + 1U, '\0');
	vsnprintf(buffer.data(), buffer.size(), fmt, args);
#endif
	std::string message(buffer.data());
	return message;
}

void fallback_write(const char* logger_name, log_level_t level, const std::string& message) noexcept
{
	const char* resolved_name = (logger_name != nullptr && logger_name[0] != '\0') ? logger_name : "app";
	std::fprintf(stderr, "[%s] [%s] %s\n", level_name(level), resolved_name, message.c_str());
	std::fflush(stderr);
}

module_config resolve_module_config_locked(const std::string& name)
{
	logger_state& log_state = state();
	const std::unordered_map<std::string, module_config>::const_iterator module = log_state.config.modules.find(name);
	if (module != log_state.config.modules.end())
	{
		return module->second;
	}

	return log_state.config.defaults;
}

log_level_t minimum_enabled_level(const module_config& config)
{
	log_level_t level = LOG_LEVEL_OFF;
	if (config.console.enabled)
	{
		level = config.console.level;
	}
	if (config.local_file.enabled)
	{
		level = std::min(level, config.local_file.level);
	}
	return level;
}

log_level_t minimum_flush_level(const module_config& config)
{
	log_level_t level = LOG_LEVEL_OFF;
	if (config.console.enabled)
	{
		level = config.console.flush_level;
	}
	if (config.local_file.enabled)
	{
		level = std::min(level, config.local_file.flush_level);
	}
	return level;
}

log_level_t resolve_logger_level_locked(const std::string& name, const module_config& config)
{
	logger_state& log_state = state();
	const std::unordered_map<std::string, log_level_t>::const_iterator logger_level = log_state.logger_levels.find(name);
	if (logger_level != log_state.logger_levels.end())
	{
		return logger_level->second;
	}

	if (log_state.has_global_level_override)
	{
		return log_state.global_level_override;
	}

	return minimum_enabled_level(config);
}

std::shared_ptr<spdlog::logger> create_logger_locked(const std::string& name)
{
	if (std::shared_ptr<spdlog::logger> existing_logger = spdlog::get(name))
	{
		return existing_logger;
	}

	logger_state& log_state = state();
	const module_config module = resolve_module_config_locked(name);
	std::vector<spdlog::sink_ptr> sinks;
	if (module.console.enabled)
	{
		std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink =
			std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		console_sink->set_level(to_spdlog_level(module.console.level));
		console_sink->set_pattern(module.console.use_color ? module.console.pattern : strip_color_markers(module.console.pattern));
		sinks.push_back(console_sink);
	}

	if (module.local_file.enabled)
	{
		const std::filesystem::path log_directory(module.local_file.directory);
		std::filesystem::create_directories(log_directory);
		const std::string file_name = log_state.run_file_prefix + "." + replace_module_token(module.local_file.file_name, name);
		const std::filesystem::path log_file_path = log_directory / file_name;
		std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> file_sink =
			std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
				log_file_path.string(),
				static_cast<std::size_t>(module.local_file.max_file_size_bytes),
				static_cast<std::size_t>(module.local_file.max_files));
		file_sink->set_level(to_spdlog_level(module.local_file.level));
		file_sink->set_pattern(module.local_file.pattern);
		sinks.push_back(file_sink);
	}

	if (sinks.empty())
	{
		log_state.disabled_loggers.insert(name);
		return nullptr;
	}

	log_state.disabled_loggers.erase(name);

	std::shared_ptr<spdlog::async_logger> logger = std::make_shared<spdlog::async_logger>(
		name,
		sinks.begin(),
		sinks.end(),
		spdlog::thread_pool(),
		spdlog::async_overflow_policy::overrun_oldest);
	logger->set_level(to_spdlog_level(resolve_logger_level_locked(name, module)));
	const log_level_t flush_level = minimum_flush_level(module);
	if (flush_level != LOG_LEVEL_OFF)
	{
		logger->flush_on(to_spdlog_level(flush_level));
	}
	spdlog::register_logger(logger);
	return logger;
}

void initialize_locked(const runtime_config& config)
{
	logger_state& log_state = state();
	log_state.config = sanitize_runtime_config(config);
	enable_console_virtual_terminal_processing();
	log_state.disabled_loggers.clear();
	log_state.logger_levels.clear();
	log_state.has_global_level_override = false;
	log_state.global_level_override = LOG_LEVEL_INFO;
	log_state.run_file_prefix = make_run_directory_name();
	spdlog::init_thread_pool(
		static_cast<std::size_t>(log_state.config.async_queue_size),
		static_cast<std::size_t>(log_state.config.async_thread_count));

	log_state.initialized = true;
	std::shared_ptr<spdlog::logger> app_logger = create_logger_locked("app");
	if (app_logger)
	{
		spdlog::set_default_logger(app_logger);
	}
}

void ensure_initialized_locked()
{
	logger_state& log_state = state();
	if (!log_state.initialized)
	{
		initialize_locked(build_runtime_config(Logger::Config()));
	}
}

void log_internal(const char* logger_name, log_level_t level, const char* fmt, va_list args) noexcept
{
	const std::string resolved_name = normalize_name(logger_name);
	va_list args_copy;
	va_copy(args_copy, args);
	const std::string message = format_message(fmt, args_copy);
	va_end(args_copy);

	try
	{
		std::shared_ptr<spdlog::logger> logger;
		bool disabled = false;
		log_level_t flush_level = LOG_LEVEL_OFF;
		{
			std::lock_guard<std::mutex> lock(state().mutex);
			ensure_initialized_locked();
			logger = create_logger_locked(resolved_name);
			disabled = state().disabled_loggers.find(resolved_name) != state().disabled_loggers.end();
			flush_level = minimum_flush_level(resolve_module_config_locked(resolved_name));
		}

		if (!logger)
		{
			if (disabled)
			{
				return;
			}
			fallback_write(resolved_name.c_str(), level, message);
			return;
		}

		logger->log(to_spdlog_level(level), message);
		if (flush_level != LOG_LEVEL_OFF && level >= flush_level)
		{
			logger->flush();
		}
	}
	catch (...)
	{
		fallback_write(resolved_name.c_str(), level, message);
	}
}

void log_variadic(const char* logger_name, log_level_t level, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_internal(logger_name, level, fmt, args);
	va_end(args);
}

} // namespace

namespace Logger
{

Config::Config()
	: log_dir("logs")
	, max_file_size(5 * 1024 * 1024)
	, max_files(5)
	, level(LOG_LEVEL_INFO)
	, async_queue_size(8192)
	, async_thread_count(1)
	, enable_console(true)
{
}

Handle::Handle(std::string name)
	: name_(normalize_name(name))
{
}

const std::string& Handle::name() const
{
	return name_;
}

void Handle::trace(const char* fmt, ...) const
{
	va_list args;
	va_start(args, fmt);
	log_internal(name_.c_str(), LOG_LEVEL_TRACE, fmt, args);
	va_end(args);
}

void Handle::debug(const char* fmt, ...) const
{
	va_list args;
	va_start(args, fmt);
	log_internal(name_.c_str(), LOG_LEVEL_DEBUG, fmt, args);
	va_end(args);
}

void Handle::info(const char* fmt, ...) const
{
	va_list args;
	va_start(args, fmt);
	log_internal(name_.c_str(), LOG_LEVEL_INFO, fmt, args);
	va_end(args);
}

void Handle::warn(const char* fmt, ...) const
{
	va_list args;
	va_start(args, fmt);
	log_internal(name_.c_str(), LOG_LEVEL_WARN, fmt, args);
	va_end(args);
}

void Handle::error(const char* fmt, ...) const
{
	va_list args;
	va_start(args, fmt);
	log_internal(name_.c_str(), LOG_LEVEL_ERROR, fmt, args);
	va_end(args);
}

void Handle::critical(const char* fmt, ...) const
{
	va_list args;
	va_start(args, fmt);
	log_internal(name_.c_str(), LOG_LEVEL_CRITICAL, fmt, args);
	va_end(args);
}

void init(const Config& config)
{
	try
	{
		std::lock_guard<std::mutex> lock(state().mutex);
		if (state().initialized)
		{
			spdlog::drop_all();
			spdlog::shutdown();
			state().initialized = false;
		}

		initialize_locked(build_runtime_config(config));
	}
	catch (...)
	{
		fallback_write("app", LOG_LEVEL_ERROR, "Logger initialization failed");
	}
}

bool init_from_file(const std::string& config_path)
{
	try
	{
		std::lock_guard<std::mutex> lock(state().mutex);
		if (state().initialized)
		{
			spdlog::drop_all();
			spdlog::shutdown();
			state().initialized = false;
		}

		initialize_locked(load_runtime_config_from_file(config_path, Logger::Config()));
		return true;
	}
	catch (...)
	{
		fallback_write("app", LOG_LEVEL_ERROR, "Logger config load failed");
		return false;
	}
}

bool reload_from_file(const std::string& config_path)
{
	runtime_config new_config;
	try
	{
		new_config = load_runtime_config_from_file(config_path, Logger::Config());
	}
	catch (const std::exception& ex)
	{
		::log_warn("simulation", "log config reload skipped (%s): %s",
			config_path.c_str(), ex.what());
		return false;
	}

	bool commit_failed = false;
	std::string fail_msg;

	try
	{
		std::lock_guard<std::mutex> lock(state().mutex);
		if (!state().initialized)
		{
			return false;
		}

		// Snapshot current state and the live logger objects so the reload can
		// be rolled back if rebuilding sinks throws (e.g. an unwritable log
		// directory in the new config). The captured shared_ptrs keep the old
		// logger objects alive across drop_all() even after they leave the
		// spdlog registry.
		const runtime_config old_config = state().config;
		const std::unordered_map<std::string, log_level_t> old_logger_levels = state().logger_levels;
		const std::unordered_set<std::string> old_disabled_loggers = state().disabled_loggers;
		const bool old_has_global_level_override = state().has_global_level_override;
		const log_level_t old_global_level_override = state().global_level_override;

		std::vector<std::shared_ptr<spdlog::logger>> old_loggers;
		const std::shared_ptr<spdlog::logger> old_default = spdlog::default_logger();
		spdlog::apply_all([&](std::shared_ptr<spdlog::logger> logger)
		{
			if (logger)
			{
				old_loggers.push_back(std::move(logger));
			}
		});

		bool dropped = false;
		try
		{
			state().config = sanitize_runtime_config(new_config);
			state().disabled_loggers.clear();
			state().logger_levels.clear();
			state().has_global_level_override = false;
			state().global_level_override = LOG_LEVEL_INFO;

			spdlog::drop_all();
			dropped = true;

			if (std::shared_ptr<spdlog::logger> app_logger = create_logger_locked("app"))
			{
				spdlog::set_default_logger(app_logger);
			}
		}
		catch (const std::exception& ex)
		{
			// Roll back the config blueprint.
			state().config = old_config;
			state().logger_levels = old_logger_levels;
			state().disabled_loggers = old_disabled_loggers;
			state().has_global_level_override = old_has_global_level_override;
			state().global_level_override = old_global_level_override;

			// Re-register the old logger objects (only if drop_all ran, so the
			// registry is empty and register_logger cannot collide).
			if (dropped)
			{
				for (const auto& logger : old_loggers)
				{
					spdlog::register_logger(logger);
				}
				if (old_default)
				{
					spdlog::set_default_logger(old_default);
				}
			}

			commit_failed = true;
			fail_msg = ex.what();
		}
	}
	catch (const std::exception& ex)
	{
		commit_failed = true;
		fail_msg = ex.what();
	}
	catch (...)
	{
		commit_failed = true;
	}

	// Warn outside the lock: ::log_warn re-acquires state().mutex internally.
	if (commit_failed)
	{
		::log_warn("simulation", "log config reload failed during commit: %s", fail_msg.c_str());
		return false;
	}
	return true;
}

void shutdown()
{
	std::lock_guard<std::mutex> lock(state().mutex);
	if (!state().initialized)
	{
		return;
	}

	spdlog::drop_all();
	spdlog::shutdown();
	state().logger_levels.clear();
	state().disabled_loggers.clear();
	state().has_global_level_override = false;
	state().run_file_prefix.clear();
	state().initialized = false;
}

Ptr get(const std::string& name)
{
	const std::string resolved_name = normalize_name(name);
	try
	{
		std::lock_guard<std::mutex> lock(state().mutex);
		ensure_initialized_locked();
		(void)create_logger_locked(resolved_name);
	}
	catch (...)
	{
		fallback_write(resolved_name.c_str(), LOG_LEVEL_ERROR, "Logger acquisition failed");
	}

	return std::make_shared<Handle>(resolved_name);
}

Ptr app()
{
	return get("app");
}

void set_level(Level level)
{
	std::lock_guard<std::mutex> lock(state().mutex);
	ensure_initialized_locked();
	state().global_level_override = sanitize_level(level, LOG_LEVEL_INFO);
	state().has_global_level_override = true;
	spdlog::apply_all([level](const std::shared_ptr<spdlog::logger>& logger)
		{
			if (logger)
			{
				logger->set_level(to_spdlog_level(level));
			}
		});
}

void set_level(const std::string& name, Level level)
{
	const std::string resolved_name = normalize_name(name);
	std::lock_guard<std::mutex> lock(state().mutex);
	ensure_initialized_locked();
	state().logger_levels[resolved_name] = level;
	if (std::shared_ptr<spdlog::logger> logger = spdlog::get(resolved_name))
	{
		logger->set_level(to_spdlog_level(level));
	}
}

void trace(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_internal("app", LOG_LEVEL_TRACE, fmt, args);
	va_end(args);
}

void debug(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_internal("app", LOG_LEVEL_DEBUG, fmt, args);
	va_end(args);
}

void info(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_internal("app", LOG_LEVEL_INFO, fmt, args);
	va_end(args);
}

void warn(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_internal("app", LOG_LEVEL_WARN, fmt, args);
	va_end(args);
}

void error(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_internal("app", LOG_LEVEL_ERROR, fmt, args);
	va_end(args);
}

void critical(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_internal("app", LOG_LEVEL_CRITICAL, fmt, args);
	va_end(args);
}

} // namespace Logger

extern "C" {

void log_init(void)
{
	if (!Logger::init_from_file(k_default_config_path))
	{
		Logger::init(Logger::Config());
	}
}

void log_shutdown(void)
{
	Logger::shutdown();
}

void log_vwrite(const char* name, log_level_t level, const char* fmt, va_list args)
{
	va_list args_copy;
	va_copy(args_copy, args);
	log_internal(name, level, (fmt != nullptr) ? fmt : "", args_copy);
	va_end(args_copy);
}

void log_write(const char* name, log_level_t level, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_vwrite(name, level, fmt, args);
	va_end(args);
}

void log_trace(const char* name, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_vwrite(name, LOG_LEVEL_TRACE, fmt, args);
	va_end(args);
}

void log_debug(const char* name, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_vwrite(name, LOG_LEVEL_DEBUG, fmt, args);
	va_end(args);
}

void log_info(const char* name, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_vwrite(name, LOG_LEVEL_INFO, fmt, args);
	va_end(args);
}

void log_warn(const char* name, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_vwrite(name, LOG_LEVEL_WARN, fmt, args);
	va_end(args);
}

void log_error(const char* name, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_vwrite(name, LOG_LEVEL_ERROR, fmt, args);
	va_end(args);
}

void log_critical(const char* name, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_vwrite(name, LOG_LEVEL_CRITICAL, fmt, args);
	va_end(args);
}

}
