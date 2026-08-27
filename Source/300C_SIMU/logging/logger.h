#ifndef SIM_LOGGING_LOGGER_H
#define SIM_LOGGING_LOGGER_H

#include <stdarg.h>

typedef enum log_level_t
{
	LOG_LEVEL_TRACE = 0,
	LOG_LEVEL_DEBUG = 1,
	LOG_LEVEL_INFO = 2,
	LOG_LEVEL_WARN = 3,
	LOG_LEVEL_ERROR = 4,
	LOG_LEVEL_CRITICAL = 5,
	LOG_LEVEL_OFF = 6
} log_level_t;

#ifdef __cplusplus

#include <memory>
#include <string>

namespace Logger
{

using Level = log_level_t;

struct Config
{
	std::string log_dir;
	int max_file_size;
	int max_files;
	Level level;
	int async_queue_size;
	int async_thread_count;
	bool enable_console;

	Config();
};

class Handle
{
public:
	explicit Handle(std::string name);

	const std::string& name() const;

	void trace(const char* fmt, ...) const;
	void debug(const char* fmt, ...) const;
	void info(const char* fmt, ...) const;
	void warn(const char* fmt, ...) const;
	void error(const char* fmt, ...) const;
	void critical(const char* fmt, ...) const;

private:
	std::string name_;
};

using Ptr = std::shared_ptr<Handle>;

void init(const Config& config);
bool init_from_file(const std::string& config_path);
bool reload_from_file(const std::string& config_path);
void shutdown();

Ptr get(const std::string& name);
Ptr app();

void set_level(Level level);
void set_level(const std::string& name, Level level);

void trace(const char* fmt, ...);
void debug(const char* fmt, ...);
void info(const char* fmt, ...);
void warn(const char* fmt, ...);
void error(const char* fmt, ...);
void critical(const char* fmt, ...);

} // namespace Logger

#endif

#ifdef __cplusplus
extern "C" {
#endif

void log_init(void);
void log_shutdown(void);
void log_vwrite(const char* name, log_level_t level, const char* fmt, va_list args);
void log_write(const char* name, log_level_t level, const char* fmt, ...);
void log_trace(const char* name, const char* fmt, ...);
void log_debug(const char* name, const char* fmt, ...);
void log_info(const char* name, const char* fmt, ...);
void log_warn(const char* name, const char* fmt, ...);
void log_error(const char* name, const char* fmt, ...);
void log_critical(const char* name, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
