#pragma once

#if defined(_WIN32)
#define DARKFLAME_PLATFORM_WIN32
#elif defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#define DARKFLAME_PLATFORM_IOS
#elif TARGET_OS_MAC
#define DARKFLAME_PLATFORM_MACOS
#else
#error unknown Apple operating system
#endif
#elif defined(__unix__)
#define DARKFLAME_PLATFORM_UNIX
#if defined(__ANDROID__)
#define DARKFLAME_PLATFORM_ANDROID
#elif defined(__linux__)
#define DARKFLAME_PLATFORM_LINUX
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#define DARKFLAME_PLATFORM_FREEBSD
#elif defined(__CYGWIN__)
#define DARKFLAME_PLATFORM_CYGWIN
#else
#error unknown unix operating system
#endif
#else
#error unknown operating system
#endif
