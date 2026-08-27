#ifndef SIM_RUNTIME_APP_LAUNCHER_H
#define SIM_RUNTIME_APP_LAUNCHER_H

#include <string>

class AppLauncher final
{
public:
	static void launch_from_file(const std::string& config_path);
	static void shutdown_auto_close_apps();
};

#endif