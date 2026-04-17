//
//    Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <filesystem>
#include <string>

namespace urkern {

// Directory of the currently-running executable.
std::filesystem::path GetExecutableDirectory();

// Per-user app data dir. Created if missing.
//   Windows: %LOCALAPPDATA%/<app_name>
//   macOS:   ~/Library/Application Support/<app_name>
//   Linux:   $XDG_DATA_HOME/<app_name> or ~/.local/share/<app_name>
std::filesystem::path GetUserDataDirectory(const std::string& app_name);

// Per-user cache dir (temp, thumbnails, etc.). Created if missing.
//   Windows: %LOCALAPPDATA%/<app_name>/Cache
//   macOS:   ~/Library/Caches/<app_name>
//   Linux:   $XDG_CACHE_HOME/<app_name> or ~/.cache/<app_name>
std::filesystem::path GetUserCacheDirectory(const std::string& app_name);

// Open a file or directory with the OS default handler.
void OpenFileInDefaultEditor(const std::filesystem::path& path);

// Windows: enable ANSI color escape codes in the current console.
// No-op on other platforms.
void EnableAnsiColors();

// Windows: allocate a console window and redirect stdio to it.
// No-op on other platforms.
void AllocateConsole();

}  // namespace urkern
