#include <updater/functions.h>

#include <config/version.h>
#include <fmt/format.h>

namespace updater {

std::string release_api_url() {
    return "https://api.github.com/repos/nckstwrt/Vita3K-Plus/releases/latest";
}

std::string release_page_url() {
    return "https://github.com/nckstwrt/Vita3K-Plus/releases/latest";
}

std::string display_version(const UpdateInfo &info) {
    if (!info.version.empty())
        return fmt::format("{} ({})", info.version, info.build_number);

    return fmt::format("Build {}", info.build_number);
}

std::string current_display_version() {
    if (app_is_release)
        return app_version;

    // A development build is named by the release it is based on, so say which
    // one. The commit count is already inside app_version.
    return fmt::format("{} (based on {})", app_version, app_base_version);
}

bool is_official_build() {
    return ::is_official_build;
}

} // namespace updater
