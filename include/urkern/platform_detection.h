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

// Compile-time platform detection. Defines exactly one of:
//   URKERN_PLATFORM_WINDOWS
//   URKERN_PLATFORM_MACOS
//   URKERN_PLATFORM_IOS
//   URKERN_PLATFORM_IOS_SIMULATOR
//   URKERN_PLATFORM_ANDROID
//   URKERN_PLATFORM_LINUX

#ifdef _WIN32
#ifdef _WIN64
#define URKERN_PLATFORM_WINDOWS
#else
#error "x86 Builds are not supported!"
#endif
#elif defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR == 1
#define URKERN_PLATFORM_IOS_SIMULATOR
#elif TARGET_OS_IPHONE == 1
#define URKERN_PLATFORM_IOS
#elif TARGET_OS_MAC == 1
#define URKERN_PLATFORM_MACOS
#else
#error "Unknown Apple platform!"
#endif
#elif defined(__ANDROID__)
#define URKERN_PLATFORM_ANDROID
#elif defined(__linux__)
#define URKERN_PLATFORM_LINUX
#else
#error "Unknown platform!"
#endif
