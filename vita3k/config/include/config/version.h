// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

extern const char org_name[];
extern const char app_name[];
extern const char display_name[];

// Version of this build. A release build reports its tag, for example "v1.1".
// Any other build reports the release it is based on and how many commits it is
// past that tag, for example "v1.1-dev.63". Set by tools/release/version-info.sh.
extern const char app_version[];
// The release this build is based on, for example "v1.1". The same on a release
// build and on a development build.
extern const char app_base_version[];
// 1 when this build was made on the release tag itself, 0 for a development
// build. A release build carries no build date, because the tag names it.
extern const bool app_is_release;
// Commits between the release tag and this build. 0 on a release build.
extern const int app_commits_since_release;
// UTC build time of a development build, for example "2026-10-03T11:56Z".
// Empty on a release build.
extern const char app_build_date[];
// Monotone integer for this build. It rises with every release and with every
// development build inside a release, so it can order builds and it is what
// Android takes as versionCode.
extern const int app_version_code;
extern const char app_hash[];
extern const char window_title[];
extern const bool is_official_build;
