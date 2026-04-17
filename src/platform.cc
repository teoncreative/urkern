//
//    Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "urkern/platform.h"

#include <stdexcept>

// clang-format off
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cstdio>
#elif __APPLE__
#include <mach-o/dyld.h>
#include <unistd.h>
#include <pwd.h>
#include <climits>
#include <cstdlib>
#else
#include <unistd.h>
#include <pwd.h>
#include <climits>
#include <cstdlib>
#endif
// clang-format on

namespace urkern {

std::filesystem::path GetExecutableDirectory() {
#ifdef _WIN32
  wchar_t path[MAX_PATH];
  DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (length == 0) {
    throw std::runtime_error("Failed to get executable path");
  }
  std::filesystem::path exe_path(path);
  return exe_path.parent_path();
#elif __APPLE__
  char path[1024];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) != 0) {
    throw std::runtime_error("Failed to get executable path");
  }
  char real_path[PATH_MAX];
  if (realpath(path, real_path) == nullptr) {
    throw std::runtime_error("Failed to resolve executable path");
  }
  return std::filesystem::path(real_path).parent_path();
#else
  char path[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
  if (count == -1) {
    throw std::runtime_error("Failed to get executable path");
  }
  path[count] = '\0';
  return std::filesystem::path(path).parent_path();
#endif
}

std::filesystem::path GetUserDataDirectory(const std::string& app_name) {
#ifdef _WIN32
  PWSTR path_ptr = nullptr;
  HRESULT hr =
      SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path_ptr);
  if (FAILED(hr)) {
    CoTaskMemFree(path_ptr);
    throw std::runtime_error("Failed to get AppData path");
  }
  std::filesystem::path user_data(path_ptr);
  CoTaskMemFree(path_ptr);
  user_data /= app_name;
#elif __APPLE__
  const char* home = getenv("HOME");
  if (!home) {
    struct passwd* pw = getpwuid(getuid());
    home = pw->pw_dir;
  }
  std::filesystem::path user_data(home);
  user_data /= "Library/Application Support";
  user_data /= app_name;
#else
  const char* xdg_data = getenv("XDG_DATA_HOME");
  std::filesystem::path user_data;
  if (xdg_data && xdg_data[0] != '\0') {
    user_data = xdg_data;
  } else {
    const char* home = getenv("HOME");
    if (!home) {
      struct passwd* pw = getpwuid(getuid());
      home = pw->pw_dir;
    }
    user_data = home;
    user_data /= ".local/share";
  }
  user_data /= app_name;
#endif

  if (!std::filesystem::exists(user_data)) {
    std::filesystem::create_directories(user_data);
  }
  return user_data;
}

std::filesystem::path GetUserCacheDirectory(const std::string& app_name) {
#ifdef _WIN32
  return GetUserDataDirectory(app_name) / "Cache";
#elif __APPLE__
  const char* home = getenv("HOME");
  if (!home) {
    struct passwd* pw = getpwuid(getuid());
    home = pw->pw_dir;
  }
  std::filesystem::path cache(home);
  cache /= "Library/Caches";
  cache /= app_name;
  if (!std::filesystem::exists(cache)) {
    std::filesystem::create_directories(cache);
  }
  return cache;
#else
  const char* xdg_cache = getenv("XDG_CACHE_HOME");
  std::filesystem::path cache;
  if (xdg_cache && xdg_cache[0] != '\0') {
    cache = xdg_cache;
  } else {
    const char* home = getenv("HOME");
    if (!home) {
      struct passwd* pw = getpwuid(getuid());
      home = pw->pw_dir;
    }
    cache = home;
    cache /= ".cache";
  }
  cache /= app_name;
  if (!std::filesystem::exists(cache)) {
    std::filesystem::create_directories(cache);
  }
  return cache;
#endif
}

void OpenFileInDefaultEditor(const std::filesystem::path& path) {
#ifdef _WIN32
  ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr,
                SW_SHOWNORMAL);
#elif __APPLE__
  std::string cmd = "open \"" + path.string() + "\"";
  system(cmd.c_str());
#else
  std::string cmd = "xdg-open \"" + path.string() + "\"";
  system(cmd.c_str());
#endif
}

void EnableAnsiColors() {
#ifdef _WIN32
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut != INVALID_HANDLE_VALUE) {
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
  }
#endif
}

void AllocateConsole() {
#ifdef _WIN32
  ::AllocConsole();
  FILE* f = nullptr;
  freopen_s(&f, "CONOUT$", "w", stdout);
  freopen_s(&f, "CONOUT$", "w", stderr);
  freopen_s(&f, "CONIN$", "r", stdin);
#endif
}

}  // namespace urkern
