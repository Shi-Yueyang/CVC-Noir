#ifndef CONNECTION_FACTORY_INCLUDE
#define CONNECTION_FACTORY_INCLUDE

#include <memory>

#include "IConnection.h"
#include "../../EXTERNAL/json.hpp"

class connection_factory
{
public:
	static std::unique_ptr<IConnection> create(const nlohmann::json& config);
};

#endif