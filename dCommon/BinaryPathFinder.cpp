#include <filesystem>
#include "BinaryPathFinder.h"
#include "dPlatforms.h"

#if DARKFLAME_PLATFORM_WIN32
#include <Windows.h>
#elif DARKFLAME_PLATFORM_MACOS || DARKFLAME_PLATFORM_IOS
#include <mach-o/dyld.h>
#elif DARKFLAME_PLATFORM_FREEBSD
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdlib.h>
#endif

std::filesystem::path BinaryPathFinder::binaryDir;

std::filesystem::path BinaryPathFinder::GetBinaryDir() {
	if (!binaryDir.empty()) {
		return binaryDir;
	}

	std::string pathStr;

	// Derived from boost::dll::program_location, licensed under the Boost Software License: http://www.boost.org/LICENSE_1_0.txt
#if DARKFLAME_PLATFORM_WIN32
	char path[MAX_PATH];
	GetModuleFileName(NULL, path, MAX_PATH);
	pathStr = std::string(path);
#elif DARKFLAME_PLATFORM_MACOS || DARKFLAME_PLATFORM_IOS
	char path[1024];
	uint32_t size = sizeof(path);
	if (_NSGetExecutablePath(path, &size) == 0) {
		pathStr = std::string(path);
	} else {
		// The filepath size is greater than our initial buffer size, so try again with the size
		// that _NSGetExecutablePath told us it actually is
		char *p = new char[size];
		if (_NSGetExecutablePath(p, &size) != 0) {
			throw std::runtime_error("Failed to get binary path from _NSGetExecutablePath");
		}

		pathStr = std::string(p);
		delete[] p;
	}
#elif DARKFLAME_PLATFORM_FREEBSD
	int mib[4];
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PATHNAME;
	mib[3] = -1;
	char buf[10240];
	size_t cb = sizeof(buf);
	sysctl(mib, 4, buf, &cb, NULL, 0);
	pathStr = std::string(buf);
#else // DARKFLAME_PLATFORM_LINUX || DARKFLAME_PLATFORM_UNIX || DARKFLAME_PLATFORM_ANDROID
	pathStr = std::filesystem::read_symlink("/proc/self/exe");
#endif

	// Some methods like _NSGetExecutablePath could return a symlink
	// Either way, we need to get the parent path because we want the directory, not the binary itself
	// We also ensure that it is an absolute path so that it is valid if we need to construct a path
	// to exucute on unix systems (eg sudo BinaryPathFinder::GetBinaryDir() / WorldServer)
	if (std::filesystem::is_symlink(pathStr)) {
		binaryDir = std::filesystem::absolute(std::filesystem::read_symlink(pathStr).parent_path());
	} else {
		binaryDir = std::filesystem::absolute(std::filesystem::path(pathStr).parent_path());
	}

	return binaryDir;
}
