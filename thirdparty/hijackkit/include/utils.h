#pragma once

#include <cstdint>

namespace hjiack {
	namespace utils {
		uintptr_t GetModuleBaseAddress(const wchar_t* moduleName);
		void AllocateConsole();
	}
}