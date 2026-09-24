cmake_minimum_required(VERSION 3.16)
project(cvc-noir LANGUAGES C CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── Source files ──────────────────────────────────────────────
# 300C_SIMU (C++ simulator runtime + C adaptation layer)
file(GLOB_RECURSE SIMU_SOURCES
    "Source/300C_SIMU/*.c"
    "Source/300C_SIMU/*.cpp"
)

# Domain logic (pure C) — configurable path
# Override with: cmake -DDOMAIN_CODE_DIR=Source/YourFolder
# Auto-detects common folder names if not set
set(DOMAIN_CODE_DIR "" CACHE STRING "Path to domain logic folder (relative to source dir)")
if(NOT DOMAIN_CODE_DIR)
    foreach(_candidate ATP_CODE ATO_CODE application Sandbox)
        if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Source/${_candidate}")
            set(DOMAIN_CODE_DIR "Source/${_candidate}" CACHE STRING "" FORCE)
            message(STATUS "Auto-detected domain logic at: ${DOMAIN_CODE_DIR}")
            break()
        endif()
    endforeach()
endif()

set(DOMAIN_SOURCES "")
if(DOMAIN_CODE_DIR AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${DOMAIN_CODE_DIR}")
    file(GLOB_RECURSE DOMAIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${DOMAIN_CODE_DIR}/*.c")
    list(LENGTH DOMAIN_SOURCES _domain_count)
    message(STATUS "Domain logic sources: ${DOMAIN_CODE_DIR} (${_domain_count} files)")
elseif(DOMAIN_CODE_DIR)
    message(WARNING "Domain logic path set but not found: ${DOMAIN_CODE_DIR}")
else()
    message(STATUS "No domain logic folder found — building simulator only")
endif()

set(ALL_SOURCES ${SIMU_SOURCES} ${DOMAIN_SOURCES})

# ── Executable ────────────────────────────────────────────────
add_executable(cvc-noir ${ALL_SOURCES})

# ── Include directories ──────────────────────────────────────
target_include_directories(cvc-noir PRIVATE
    "Source/300C_SIMU"
    "Source/300C_SIMU/EXTERNAL"
)
if(DOMAIN_CODE_DIR AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${DOMAIN_CODE_DIR}")
    target_include_directories(cvc-noir PRIVATE "${DOMAIN_CODE_DIR}")
endif()

# ── Preprocessor defines ─────────────────────────────────────
target_compile_definitions(cvc-noir PRIVATE
    API_PRINTF_IS_PRINTF

    # Windows-specific
    $<$<PLATFORM_ID:Windows>:
        _CRT_SECURE_NO_WARNINGS
    >

    # Debug
    $<$<CONFIG:Debug>:
        _DEBUG
    >

    # Release
    $<$<CONFIG:Release>:
        NDEBUG
    >
)

# On Windows, also define WIN32
if(WIN32)
    target_compile_definitions(cvc-noir PRIVATE WIN32)
endif()

# On Linux, define _LINUX for code that needs a Linux branch
if(UNIX AND NOT APPLE)
    target_compile_definitions(cvc-noir PRIVATE __linux__ _LINUX)
endif()

# ── Compiler options ──────────────────────────────────────────
if(MSVC)
    # Match the VS project settings
    target_compile_options(cvc-noir PRIVATE
        /utf-8
        /MP   # multi-processor compilation
        /wd4828  # disable "illegal character" warning for GBK comments
    )
    # Disable specific MSVC warnings (mirrors #pragma in code)
    target_compile_options(cvc-noir PRIVATE
        /wd4057 /wd4100 /wd4127 /wd4189 /wd4244 /wd4293 /wd4389
    )
    # Release optimizations
    target_compile_options(cvc-noir PRIVATE
        $<$<CONFIG:Release>:/O2>
    )
else()
    # GCC / Clang
    target_compile_options(cvc-noir PRIVATE
        -Wall -Wextra
    )
    # Suppress warnings the ATP_CODE generates (lots of sign-compare, unused-parameter etc.)
    target_compile_options(cvc-noir PRIVATE
        -Wno-sign-compare
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-but-set-variable
        -Wno-missing-field-initializers
        -Wno-format
        -Wno-format-security
        -Wno-unknown-pragmas
        -Wno-pointer-compare
        -Wno-type-limits
        -Wno-implicit-function-declaration
        -Wno-comment
        -Wno-char-subscripts
    )
    # Debug / Release
    target_compile_options(cvc-noir PRIVATE
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O2 -DNDEBUG>
    )
endif()

# ── Link libraries ────────────────────────────────────────────
if(WIN32)
    target_link_libraries(cvc-noir PRIVATE ws2_32)
else()
    # POSIX: need pthread for spdlog async logger; librt for clock_gettime (glibc < 2.17)
    target_link_libraries(cvc-noir PRIVATE pthread)

    # Check if clock_gettime needs -lrt (older glibc)
    include(CheckLibraryExists)
    check_library_exists(rt clock_gettime "" HAVE_CLOCK_GETTIME_IN_RT)
    if(HAVE_CLOCK_GETTIME_IN_RT)
        target_link_libraries(cvc-noir PRIVATE rt)
    endif()
endif()

# ── Tests ────────────────────────────────────────────────────
option(BUILD_TESTS "Build test executables" ON)
if(BUILD_TESTS)
	enable_testing()
	add_executable(test_tcp_framing "tests/test_tcp_framing.cpp")
	target_include_directories(test_tcp_framing PRIVATE
		"Source/300C_SIMU/BSW_CODE/Pub_Comm")
	if(MSVC)
		target_compile_options(test_tcp_framing PRIVATE /utf-8)
	else()
		target_compile_options(test_tcp_framing PRIVATE
			-Wall -Wextra
			-Wno-sign-compare -Wno-unused-parameter -Wno-unused-variable
		)
	endif()
	add_test(NAME test_tcp_framing COMMAND test_tcp_framing)

	add_executable(test_a_train_session
		"tests/test_a_train_session.cpp"
		"Source/300C_SIMU/BSW_CODE/session/a_train_session.cpp"
		"Source/300C_SIMU/BSW_CODE/session/sdmu_packet.cpp"
		"Source/300C_SIMU/BSW_CODE/session/session_crc.cpp"
		"Source/300C_SIMU/BSW_CODE/Pub_Comm/connection_factory.cpp"
		"Source/300C_SIMU/BSW_CODE/Pub_Comm/dummy_connection.cpp"
		"Source/300C_SIMU/BSW_CODE/Pub_Comm/serial_connection.cpp"
		"Source/300C_SIMU/BSW_CODE/Pub_Comm/tcp_connection_client.cpp"
		"Source/300C_SIMU/BSW_CODE/Pub_Comm/tcp_connection_server.cpp"
		"Source/300C_SIMU/BSW_CODE/Pub_Comm/udp_connection.cpp"
		"Source/300C_SIMU/logging/logger.cpp"
	)
	target_include_directories(test_a_train_session PRIVATE
		"Source/300C_SIMU"
		"Source/300C_SIMU/BSW_CODE/session"
		"Source/300C_SIMU/BSW_CODE/Pub_Comm"
		"Source/300C_SIMU/EXTERNAL"
	)
	if(MSVC)
		target_compile_options(test_a_train_session PRIVATE /utf-8)
	else()
		target_compile_options(test_a_train_session PRIVATE
			-Wall -Wextra
			-Wno-sign-compare -Wno-unused-parameter -Wno-unused-variable
		)
	endif()
	if(WIN32)
		target_link_libraries(test_a_train_session PRIVATE ws2_32)
	else()
		target_link_libraries(test_a_train_session PRIVATE pthread)
	endif()
	add_test(NAME test_a_train_session COMMAND test_a_train_session)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Sandbox/sandbox_domain.c")
        add_executable(test_sandbox_domain
            "tests/test_sandbox_domain.c"
            "Source/Sandbox/sandbox_domain.c"
        )
        target_include_directories(test_sandbox_domain PRIVATE "Source/Sandbox")
        add_test(NAME test_sandbox_domain COMMAND test_sandbox_domain)
    endif()
endif()

# ── IDE source grouping (optional, matches VS filters) ───────
# Group sources by their directory to make IDE navigation easier
source_group(TREE "${CMAKE_SOURCE_DIR}" FILES ${ALL_SOURCES})
