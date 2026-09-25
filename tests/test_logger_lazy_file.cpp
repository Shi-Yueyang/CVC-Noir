#include "logging/logger.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <json.hpp>

int main()
{
	const std::filesystem::path test_directory = std::filesystem::temp_directory_path() /
		("cvc-noir-logger-test-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()));
	const std::filesystem::path log_directory = test_directory / "logs";
	const std::filesystem::path config_path = test_directory / "simu_config.json";
	std::filesystem::create_directories(test_directory);

	nlohmann::json config = {
		{"log", {
			{"default_console", {{"enabled", false}}},
			{"default_file", {
				{"enabled", true},
				{"directory", log_directory.string()},
				{"file_name", "{module}.log"},
				{"level", "trace"}
			}}
		}}
	};
	{
		std::ofstream config_file(config_path);
		config_file << config.dump();
	}

	bool passed = true;
	if (!Logger::init_from_file(config_path.string()))
	{
		std::cerr << "FAIL: logger config initializes\n";
		passed = false;
	}
	else
	{
		Logger::Ptr logger = Logger::get("lazy_file_test");
		if (std::filesystem::exists(log_directory))
		{
			std::cerr << "FAIL: acquiring a logger does not create the log directory\n";
			passed = false;
		}

		logger->info("first write");
		Logger::shutdown();

		bool file_created = false;
		if (std::filesystem::exists(log_directory))
		{
			for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(log_directory))
			{
				if (entry.is_regular_file())
				{
					file_created = true;
					break;
				}
			}
		}
		if (!file_created)
		{
			std::cerr << "FAIL: first log write creates a log file\n";
			passed = false;
		}
	}

	Logger::shutdown();
	std::filesystem::remove_all(test_directory);
	return passed ? 0 : 1;
}