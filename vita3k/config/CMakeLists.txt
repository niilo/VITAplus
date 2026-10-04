add_library(
	config
	STATIC
	include/config/config.h
	include/config/functions.h
	include/config/settings.h
	include/config/state.h
	include/config/version.h
	src/config.cpp
	src/settings.cpp
	version.cpp
)

# The version comes from tools/release/version-info.sh, so the generated file
# has to be rewritten on every configure, not only when this template changes.
# configure_file without FORCE skips the write when the generated result would
# be identical, and it does not know the version is a variable, so a build after
# a new commit kept the first build's version. FORCE guarantees the write.
configure_file(src/version.cpp.in version.cpp FORCE)

target_include_directories(config PUBLIC include)
target_link_libraries(config PUBLIC input util)
target_link_libraries(config PRIVATE CLI11 pugixml::pugixml yaml-cpp)
if(TRACY_ENABLE_ON_CORE_COMPONENTS)
	target_link_libraries(config PRIVATE tracy)
endif()
