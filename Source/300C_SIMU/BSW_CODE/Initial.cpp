#include <stdio.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

/* Platform-safe wrappers for MSVC CRT functions */
#ifndef _MSC_VER
#define strcpy_s(dst, src) do { \
    strncpy((dst), (src), sizeof(dst) - 1); \
    (dst)[sizeof(dst) - 1] = '\0'; \
} while(0)
#define fopen_s(pfp, path, mode) (*(pfp) = fopen((path), (mode)), *(pfp) == NULL ? ENOENT : 0)
#endif

#include <json.hpp>

#include "logging/logger.h"

#include "ASW_300C/cvc_datatypes.h"
#include "ASW_300C/conf.h"
#include "ASW_300C/application.h"
#include "ASW_300C/asw_runtime_api.h"
#include "ASW_300C/interface_p2a.h"

#include "Initial.h"
#include "AppContext.h"
#include "bswTime.h"
#include "session/ext_session.h"
#include "session/maint_session.h"
#include "session/other_asw_session.h"
#include "session/pxi_motion_session.h"
#include "session/pxi_session.h"
#include "session/raw_session.h"
#include "session/rsspi_session.h"
#include "session/safety037_session.h"
#include "session/snmp_session.h"

static bool InitLogger(const std::string& config_path);
static bool LoadAppConfig(const std::string& config_path, nlohmann::json& root_config);
static void Asw_Init(void);
static void Asw_Plug_Init(const nlohmann::json& root_config);
static void SessionInit(AppContext* ctx, const nlohmann::json& root_config);
static void ParseBoardStatusConfig(const nlohmann::json& root_config);
static void ParsePdaStorageConfig(const nlohmann::json& root_config);

bool Initial(AppContext* p_ctx, const std::string& config_path)
{
	if (!p_ctx)
	{
		return false;
	}

	if (!InitLogger(config_path))
	{
		return false;
	}

	timer_Init();

	nlohmann::json root_config;
	if (!LoadAppConfig(config_path, root_config))
	{
		return false;
	}

	SessionInit(p_ctx, root_config);
	ParseBoardStatusConfig(root_config);
	ParsePdaStorageConfig(root_config);

	CONF_F_ConfigInit();

	BS_Init();

	Asw_Init();

	Asw_Plug_Init(root_config);

	return true;
}


static bool InitLogger(const std::string& config_path)
{
	if (!Logger::init_from_file(config_path))
	{
		return false;
	}
	return true;
}


static bool LoadAppConfig(const std::string& config_path, nlohmann::json& root_config)
{
	const Logger::Ptr log = Logger::get("simulation");

	std::ifstream json_file(config_path);
	if (!json_file.is_open())
	{
		log->error("failed to open config file: %s", config_path.c_str());
		return false;
	}

	try
	{
		json_file >> root_config;
	}
	catch (const std::exception& ex)
	{
		log->error("failed to parse config: %s", ex.what());
		return false;
	}

	return true;
}


static void SessionInit(AppContext* ctx, const nlohmann::json& root_config)
{
	const Logger::Ptr log = Logger::get("simulation");

	if (!ctx)
	{
		return;
	}

	const nlohmann::json& session_config =
		(root_config.contains("sessions") && root_config["sessions"].is_object()) ? root_config["sessions"] : root_config;

	ctx->raw_sessions.clear();
	ctx->ext_sessions.clear();
	ctx->maint_sessions.clear();
	if (session_config.contains("ext_sessions") && session_config["ext_sessions"].is_array())
	{
		for (const auto& session_entry : session_config["ext_sessions"])
		{
			std::unique_ptr<session> session_ptr(new ext_session(session_entry));
			if (session_ptr->connection())
			{
				const connection_result open_result = session_ptr->connection()->open();
				if (open_result != connection_result::ok)
				{
					log->warn("failed to open ext session connection: name=%s, id=%d",
						session_ptr->name().c_str(),
						session_ptr->id());
				}
			}
			ctx->ext_sessions.push_back(std::move(session_ptr));
		}
	}
	else
	{
		log->warn("ext_sessions array missing");
	}

	if (session_config.contains("maint_sessions") && session_config["maint_sessions"].is_array())
	{
		for (const auto& session_entry : session_config["maint_sessions"])
		{
			std::unique_ptr<session> session_ptr(new maint_session(session_entry));
			if (session_ptr->connection())
			{
				const connection_result open_result = session_ptr->connection()->open();
				if (open_result != connection_result::ok)
				{
					log->warn("failed to open maint session connection: name=%s, id=%d",
						session_ptr->name().c_str(),
						session_ptr->id());
				}
			}
			ctx->maint_sessions.push_back(std::move(session_ptr));
		}
	}
	else
	{
		log->warn("maint_sessions array missing");
	}

	ctx->other_asw_sessions.clear();
	if (session_config.contains("other_asw_sessions") && session_config["other_asw_sessions"].is_array())
	{
		for (const auto& session_entry : session_config["other_asw_sessions"])
		{
			std::unique_ptr<session> session_ptr(new other_asw_session(session_entry));
			if (session_ptr->connection())
			{
				const connection_result open_result = session_ptr->connection()->open();
				if (open_result != connection_result::ok)
				{
					log->warn("failed to open other_asw session connection: name=%s, id=%d",
						session_ptr->name().c_str(),
						session_ptr->id());
				}
			}
			ctx->other_asw_sessions.push_back(std::move(session_ptr));
		}
	}
	else
	{
		log->warn("other_asw_sessions array missing");
	}

	if (session_config.contains("raw_sessions") && session_config["raw_sessions"].is_array())
	{
		for (const auto& session_entry : session_config["raw_sessions"])
		{
			std::unique_ptr<session> session_ptr(new raw_session(session_entry));
			if (session_ptr->connection())
			{
				const connection_result open_result = session_ptr->connection()->open();
				if (open_result != connection_result::ok)
				{
					log->warn("failed to open raw session connection: name=%s, id=%d",
						session_ptr->name().c_str(),
						session_ptr->id());
				}
			}
			ctx->raw_sessions.push_back(std::move(session_ptr));
		}
	}
	else
	{
		log->warn("raw_sessions array missing");
	}

	ctx->rsspi_sessions.clear();
	const auto& rssp1_config = session_config.contains("rssp1_sessions")
		? session_config["rssp1_sessions"]
		: session_config["rsspi_sessions"];
	if (rssp1_config.is_array())
	{
		for (const auto& session_entry : rssp1_config)
		{
			std::unique_ptr<session> session_ptr(new rsspi_session(session_entry));
			if (session_ptr->connection())
			{
				const connection_result open_result = session_ptr->connection()->open();
				if (open_result != connection_result::ok)
				{
					log->warn("failed to open rsspi session connection: name=%s, id=%d",
						session_ptr->name().c_str(),
						session_ptr->id());
				}
			}
			ctx->rsspi_sessions.push_back(std::move(session_ptr));
		}
	}
	else
	{
		log->warn("rsspi_sessions array missing");
	}

	ctx->safety037_sessions.clear();
	if (session_config.contains("safety037_sessions") && session_config["safety037_sessions"].is_array())
	{
		for (const auto& session_entry : session_config["safety037_sessions"])
		{
			std::unique_ptr<session> session_ptr(new safety037_session(session_entry));
			if (session_ptr->connection())
			{
				const connection_result open_result = session_ptr->connection()->open();
				if (open_result != connection_result::ok)
				{
					log->warn("failed to open safety037 session connection: name=%s, id=%d",
						session_ptr->name().c_str(),
						session_ptr->id());
				}
			}
			ctx->safety037_sessions.push_back(std::move(session_ptr));
		}
	}
	else
	{
		log->warn("safety037_sessions array missing");
	}

	ctx->snmp_sessions.clear();
	if (session_config.contains("snmp_sessions") && session_config["snmp_sessions"].is_array())
	{
		for (const auto& session_entry : session_config["snmp_sessions"])
		{
			std::unique_ptr<session> session_ptr(new snmp_session(session_entry));
			if (session_ptr->connection())
			{
				const connection_result open_result = session_ptr->connection()->open();
				if (open_result != connection_result::ok)
				{
					log->warn("failed to open snmp session connection: name=%s, id=%d",
						session_ptr->name().c_str(),
						session_ptr->id());
				}
			}
			ctx->snmp_sessions.push_back(std::move(session_ptr));
		}
	}
	else
	{
		log->warn("snmp_sessions array missing");
	}

	ctx->pxi_session_instance.reset();
	if (session_config.contains("pxi_session") && session_config["pxi_session"].is_object())
	{
		std::unique_ptr<pxi_session> pxi_ptr(new pxi_session(session_config["pxi_session"]));
		if (pxi_ptr->connection())
		{
			const connection_result open_result = pxi_ptr->connection()->open();
			if (open_result != connection_result::ok)
			{
				log->warn("failed to open pxi session connection: name=%s, id=%d",
					pxi_ptr->name().c_str(),
					pxi_ptr->id());
			}
		}
		ctx->pxi_session_instance = std::move(pxi_ptr);
	}

	ctx->pxi_motion_session_instance.reset();
	if (session_config.contains("pxi_motion_session") && session_config["pxi_motion_session"].is_object())
	{
		std::unique_ptr<pxi_motion_session> pxi_motion_ptr(new pxi_motion_session(session_config["pxi_motion_session"]));
		if (pxi_motion_ptr->connection())
		{
			const connection_result open_result = pxi_motion_ptr->connection()->open();
			if (open_result != connection_result::ok)
			{
				log->warn("failed to open pxi motion session connection: name=%s, id=%d",
					pxi_motion_ptr->name().c_str(),
					pxi_motion_ptr->id());
			}
		}
		ctx->pxi_motion_session_instance = std::move(pxi_motion_ptr);
	}
}

static void ParseBoardStatusConfig(const nlohmann::json& root_config)
{
	const Logger::Ptr log = Logger::get("simulation");

	if (!root_config.contains("board_status") || !root_config["board_status"].is_array())
	{
		log->info("board_status config missing, using defaults");
		return;
	}

	const auto& board_status_array = root_config["board_status"];
	if (board_status_array.size() == 0 || board_status_array.size() > BOARD_STATUS_CONFIG_TYPE_MAX)
	{
		log->error("board_status array size invalid: %zu", board_status_array.size());
		return;
	}

	std::memset(&g_board_status_config, 0, sizeof(g_board_status_config));

	std::uint8_t type_idx = 0;
	for (const auto& type_entry : board_status_array)
	{
		if (!type_entry.contains("type") || !type_entry["type"].is_string())
		{
			log->error("board_status entry missing type string");
			continue;
		}

		if (!type_entry.contains("boards") || !type_entry["boards"].is_array())
		{
			log->error("board_status entry missing boards array");
			continue;
		}

		const std::string type_str = type_entry["type"].get<std::string>();
		std::uint8_t type_id = 0xFF;
		if (type_str == "MPB") type_id = BOARD_STATUS_TYPE_MPB;
		else if (type_str == "GWB") type_id = BOARD_STATUS_TYPE_GWB;
		else if (type_str == "VVB") type_id = BOARD_STATUS_TYPE_VVB;
		else if (type_str == "VIB") type_id = BOARD_STATUS_TYPE_VIB;
		else if (type_str == "VOB") type_id = BOARD_STATUS_TYPE_VOB;
		else if (type_str == "BTM") type_id = BOARD_STATUS_TYPE_BTM;
		else if (type_str == "AIOB") type_id = BOARD_STATUS_TYPE_AIOB; 
		else
		{
			log->error("unknown board type: %s", type_str.c_str());
			continue;
		}

		const auto& boards_array = type_entry["boards"];
		if (boards_array.size() == 0 || boards_array.size() > BOARD_STATUS_CONFIG_BOARD_MAX)
		{
			log->error("boards array size invalid for type %s: %zu", type_str.c_str(), boards_array.size());
			continue;
		}

		BoardStatus_ConfigType_t& config_type = g_board_status_config.types[type_idx];
		config_type.type_id = type_id;

		std::uint8_t board_idx = 0;
		for (const auto& board_entry : boards_array)
		{
			if (!board_entry.contains("id") || !board_entry["id"].is_number())
			{
				log->error("board entry missing id");
				continue;
			}
			if (!board_entry.contains("status") || !board_entry["status"].is_string())
			{
				log->error("board entry missing status");
				continue;
			}

			BoardStatus_ConfigEntry_t& config_entry = config_type.boards[board_idx];
			config_entry.id = static_cast<std::uint8_t>(board_entry["id"].get<int>());

			if (board_entry.contains("friend_id") && board_entry["friend_id"].is_number())
			{
				config_entry.friend_id = static_cast<std::uint8_t>(board_entry["friend_id"].get<int>());
			}
			else
			{
				config_entry.friend_id = 0;
			}

			const std::string status_str = board_entry["status"].get<std::string>();
			if (status_str == "good")
			{
				config_entry.is_good = 1;
			}
			else if (status_str == "bad")
			{
				config_entry.is_good = 0;
			}
			else
			{
				log->error("unknown status '%s' for board id %d, defaulting to bad", status_str.c_str(), config_entry.id);
				config_entry.is_good = 0;
			}

			++board_idx;
		}
		config_type.board_num = board_idx;

		++type_idx;
	}

	g_board_status_config.type_num = type_idx;
	log->info("loaded board_status config: %d types", type_idx);
}

static void ParsePdaStorageConfig(const nlohmann::json& root_config)
{
	const Logger::Ptr log = Logger::get("simulation");

	std::memset(&g_pda_storage_config, 0, sizeof(g_pda_storage_config));

	if (!root_config.contains("data") || !root_config["data"].is_object())
	{
		log->info("data config missing, using defaults");
		// Set default values
		strcpy_s(g_pda_storage_config.nrnw.file, "Plug/nvram.bin");
		strcpy_s(g_pda_storage_config.araw.file, "Plug/dataplug.bin");
		strcpy_s(g_pda_storage_config.arnw.file, "Plug/arnw.bin");
		strcpy_s(g_pda_storage_config.flash[0].file, "Plug/db.dat");
		g_pda_storage_config.flash[0].id = 0;
		strcpy_s(g_pda_storage_config.flash[1].file, "Plug/db2.dat");
		g_pda_storage_config.flash[1].id = 1;
		g_pda_storage_config.flash_count = 2;
	}

	auto check_file_exists = [&](const char* file_path, const char* pda_type) {
		if (!std::filesystem::exists(file_path)) {
			log->warn("PDA file does not exist: %s (%s)", file_path, pda_type);
		}
	};

	if (!root_config.contains("data") || !root_config["data"].is_object())
	{
		// Check default file paths
		check_file_exists(g_pda_storage_config.nrnw.file, "pda1/nrnw");
		check_file_exists(g_pda_storage_config.araw.file, "pda2/araw");
		check_file_exists(g_pda_storage_config.arnw.file, "pda4/arnw");
		for (uint8_t i = 0; i < g_pda_storage_config.flash_count; i++) {
			check_file_exists(g_pda_storage_config.flash[i].file, "pda3/flash");
		}
		return;
	}

	const auto& data_config = root_config["data"];
	auto get_data_object = [&](const char* preferred_key, const char* legacy_key) -> const nlohmann::json*
	{
		if (data_config.contains(preferred_key) && data_config[preferred_key].is_object())
		{
			return &data_config[preferred_key];
		}
		if (data_config.contains(legacy_key) && data_config[legacy_key].is_object())
		{
			return &data_config[legacy_key];
		}
		return nullptr;
	};

	// Parse first type PDA data (new key: pda1, legacy key: nrnw)
	if (const nlohmann::json* pda1_config = get_data_object("pda1", "nrnw"))
	{
		if (pda1_config->contains("file") && (*pda1_config)["file"].is_string())
		{
			std::string file = (*pda1_config)["file"].get<std::string>();
			strcpy_s(g_pda_storage_config.nrnw.file, file.c_str());
		}
		check_file_exists(g_pda_storage_config.nrnw.file, "pda1/nrnw");
	}

	// Parse second type PDA data (new key: pda2, legacy key: araw)
	if (const nlohmann::json* pda2_config = get_data_object("pda2", "araw"))
	{
		if (pda2_config->contains("file") && (*pda2_config)["file"].is_string())
		{
			std::string file = (*pda2_config)["file"].get<std::string>();
			strcpy_s(g_pda_storage_config.araw.file, file.c_str());
		}
		check_file_exists(g_pda_storage_config.araw.file, "pda2/araw");
	}

	// Parse fourth type PDA data (new key: pda4, legacy key: arnw)
	if (const nlohmann::json* pda4_config = get_data_object("pda4", "arnw"))
	{
		if (pda4_config->contains("file") && (*pda4_config)["file"].is_string())
		{
			std::string file = (*pda4_config)["file"].get<std::string>();
			strcpy_s(g_pda_storage_config.arnw.file, file.c_str());
		}
		check_file_exists(g_pda_storage_config.arnw.file, "pda4/arnw");
	}

	const nlohmann::json* pda3_config = nullptr;
	if (data_config.contains("pda3") && data_config["pda3"].is_array())
	{
		pda3_config = &data_config["pda3"];
	}
	else if (data_config.contains("flash") && data_config["flash"].is_array())
	{
		pda3_config = &data_config["flash"];
	}

	// Parse third type PDA data (new key: pda3, legacy key: flash)
	if (pda3_config != nullptr)
	{
		const auto& flash_array = *pda3_config;
		uint8_t count = 0;
		for (const auto& flash_entry : flash_array)
		{
			if (count >= PDA_CONFIG_MAX_FLASH_FILES)
			{
				log->warn("flash array exceeds max %d, ignoring remaining entries", PDA_CONFIG_MAX_FLASH_FILES);
				break;
			}

			if (flash_entry.contains("id") && flash_entry["id"].is_number())
			{
				g_pda_storage_config.flash[count].id = flash_entry["id"].get<uint8_t>();
			}
			if (flash_entry.contains("file") && flash_entry["file"].is_string())
			{
				std::string file = flash_entry["file"].get<std::string>();
				strcpy_s(g_pda_storage_config.flash[count].file, file.c_str());
			}
			check_file_exists(g_pda_storage_config.flash[count].file, "pda3/flash");
			count++;
		}
		g_pda_storage_config.flash_count = count;
	}

	log->info("loaded data config: pda1=%s, pda2=%s, pda3_count=%d, pda4=%s",
		g_pda_storage_config.nrnw.file, 
		g_pda_storage_config.araw.file, 
		g_pda_storage_config.flash_count,
		g_pda_storage_config.arnw.file);
}

static void Asw_Init(void)
{

	std::uint32_t appVerCRC = 0U;
	std::uint8_t ret = 0U;
	FILE *fp = NULL;
	int file_size = 0;
	std::array<std::uint8_t, 40960> plug_file = {};
	std::uint64_t sf_raw_addr = 0U;
	const char *plug_path = "plug\\data.ctcs";
	CVC_BSW_ITF_Init();
	if (NULL != CONF_V_ASW1ConfigPtr)
	{
		auto *config_data = static_cast<std::uint8_t *>(CONF_V_ASW1ConfigPtr);
		const auto config_size = *reinterpret_cast<std::uint32_t *>(CONF_V_ASW1ConfigPtr);
		ret = static_cast<std::uint8_t>(SRV_Initialize(reinterpret_cast<INT8U *const>(config_data + 4U),
			config_size,
			reinterpret_cast<INT32U *>(&appVerCRC),
			sf_raw_addr));
	}

	fopen_s(&fp, plug_path, "rb");
	if (fp != NULL)
	{
		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		fread(plug_file.data(), 1, file_size, fp);
		fclose(fp);
		ret = static_cast<std::uint8_t>(SRV_Initialize(reinterpret_cast<INT8U *>(plug_file.data()),
			static_cast<std::uint32_t>(file_size),
			reinterpret_cast<INT32U *>(&appVerCRC),
			sf_raw_addr));
	}

	return;
}

static void Asw_Plug_Init(const nlohmann::json& root_config)
{
	const Logger::Ptr log = Logger::get("simulation");
	std::array<std::uint8_t, 102400> plug_file = {};
	std::size_t total_file_size = 0U;
	std::uint32_t asw_ver = 0U;
	const auto append_binary_file = [&plug_file, &total_file_size, &log](const std::string &file_path, const char *display_name) -> bool
	{
		std::ifstream input(file_path, std::ios::binary | std::ios::ate);
		if (!input.is_open())
		{
			log->error("open %s error: %s", display_name, file_path.c_str());
			return false;
		}

		const std::streamoff file_size = input.tellg();
		if (file_size < 0)
		{
			log->error("get %s size error", display_name);
			return false;
		}

		const std::size_t file_size_value = static_cast<std::size_t>(file_size);
		if (total_file_size + file_size_value > plug_file.size())
		{
			log->error("plug buffer out of range: %s", display_name);
			return false;
		}

		input.seekg(0, std::ios::beg);
		if ((file_size_value > 0U) && !input.read(reinterpret_cast<char *>(plug_file.data() + total_file_size), static_cast<std::streamsize>(file_size_value)))
		{
			log->error("read %s error", display_name);
			return false;
		}

		total_file_size += file_size_value;
		return true;
	};

	const nlohmann::json application_data = root_config.value("data", nlohmann::json::object()).value("application_data", nlohmann::json::object());
	const nlohmann::json files = application_data.value("files", nlohmann::json::array());
	const nlohmann::json version_pad = application_data.value("version_pad", nlohmann::json::array());
	if (!files.is_array() || files.empty())
	{
		log->error("data.application_data.files is missing or empty");
		return;
	}

	for (const auto& file : files)
	{
		if (!file.is_string())
		{
			log->error("data.application_data.files contains non-string item");
			return;
		}

		const std::string file_path = file.get<std::string>();
		if (!append_binary_file(file_path, file_path.c_str()))
		{
			return;
		}
	}

	if (!version_pad.is_array())
	{
		log->error("data.application_data.version_pad is missing or invalid");
		return;
	}

	if (total_file_size + version_pad.size() > plug_file.size())
	{
		log->error("plug buffer out of range: version bytes");
		return;
	}

	for (const auto& byte : version_pad)
	{
		if (!byte.is_number_unsigned())
		{
			log->error("data.application_data.version_pad contains non-byte item");
			return;
		}

		const unsigned int value = byte.get<unsigned int>();
		if (value > 0xFFU)
		{
			log->error("data.application_data.version_pad contains out-of-range item: %u", value);
			return;
		}

		plug_file[total_file_size] = static_cast<std::uint8_t>(value);
		++total_file_size;
	}

	SRV_Initialize(reinterpret_cast<INT8U *>(plug_file.data()),
		static_cast<std::uint32_t>(total_file_size),
		reinterpret_cast<UINT_32 *>(&asw_ver),
		0U);


	return;
}
