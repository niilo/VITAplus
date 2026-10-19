add_library(
	util
	STATIC
	src/adpf.cpp
	src/android_driver.cpp
	src/arm.cpp
	src/float_to_half.cpp
	src/fs_utils.cpp
	src/hash.cpp
	src/hotpath.cpp
	src/instrset_detect.cpp
	src/logging.cpp
	src/mem_snapshot.cpp
	src/net_utils.cpp
	src/perf_log.cpp
	src/string_utils.cpp
	src/thread_priority.cpp
	src/tracy.cpp
	src/vita_theme_utils.cpp
)

# vc_runtime_checker.cpp is directly added from the main CMakeList (for some reason adding it here doesn't work)

target_include_directories(util PUBLIC include)

# Plus reads util/fork_build.h, which its author makes with a local tool and does not commit.
# Generate a default one so that a clean clone builds. A file in include/util takes priority.
set(VITA3K_FORK_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(VITA3K_BUILD_LABEL "" CACHE STRING "Optional label for a local test build")
if(NOT VITA3K_BUILD_LABEL MATCHES "^[a-zA-Z0-9 ._-]*$")
	message(FATAL_ERROR "VITA3K_BUILD_LABEL may contain letters, digits, spaces, dots, underscores and hyphens.")
endif()
set(VITA3K_FORK_BUILD_DESCRIPTION "niilo/VITAplus build")
if(NOT VITA3K_BUILD_LABEL STREQUAL "")
	string(APPEND VITA3K_FORK_BUILD_DESCRIPTION ": ${VITA3K_BUILD_LABEL}")
endif()
file(WRITE "${VITA3K_FORK_BUILD_DIR}/util/fork_build.h.tmp"
	"#pragma once\n#define FORK_BUILD_SEQ 0\n#define FORK_BUILD_CHANGES \"${VITA3K_FORK_BUILD_DESCRIPTION}\"\n")
file(COPY_FILE "${VITA3K_FORK_BUILD_DIR}/util/fork_build.h.tmp" "${VITA3K_FORK_BUILD_DIR}/util/fork_build.h" ONLY_IF_DIFFERENT)
target_include_directories(util PRIVATE "${VITA3K_FORK_BUILD_DIR}")
target_link_libraries(util PUBLIC ${Boost_LIBRARIES} fmt spdlog http mem nids)
target_link_libraries(util PRIVATE libcurl crypto SDL3::SDL3)
if(ANDROID)
	target_link_libraries(util PUBLIC emuenv android xxHash::xxhash)
	target_link_libraries(util PRIVATE adrenotools)
endif()
target_compile_definitions(util PRIVATE $<$<CONFIG:Debug,RelWithDebInfo>:TRACY_ENABLE>)
