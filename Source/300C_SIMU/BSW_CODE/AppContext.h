#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#include <cstdint>
#include <memory>
#include <vector>

#include "session/ext_session.h"
#include "session/maint_session.h"
#include "session/other_asw_session.h"
#include "session/pxi_motion_session.h"
#include "session/pxi_session.h"
#include "session/rsspi_session.h"
#include "session/safety037_session.h"
#include "session/snmp_session.h"
#include "session/session.h"

typedef struct AppContext {

    std::vector<std::unique_ptr<session>> ext_sessions;
    std::vector<std::unique_ptr<session>> maint_sessions;
    std::vector<std::unique_ptr<session>> other_asw_sessions;
    std::vector<std::unique_ptr<session>> raw_sessions;
    std::vector<std::unique_ptr<session>> rsspi_sessions;
    std::vector<std::unique_ptr<session>> safety037_sessions;
    std::vector<std::unique_ptr<session>> snmp_sessions;
    std::int32_t train_pos_cm = 0;
    std::unique_ptr<pxi_motion_session> pxi_motion_session_instance;
    std::unique_ptr<pxi_session> pxi_session_instance;
} AppContext;

#endif // APP_CONTEXT_H
