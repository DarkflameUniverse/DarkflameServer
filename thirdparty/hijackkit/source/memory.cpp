#include "memory.h"

#include <Windows.h>

void hijack::memory::Protect(size_t address, size_t size, std::function<void()> function) {
	DWORD oldProtect;
	VirtualProtect((void*)address, size, PAGE_EXECUTE_READWRITE, &oldProtect);

	function();

	VirtualProtect((void*)address, size, oldProtect, &oldProtect);
}

void* hijack::memory::Read(size_t address, size_t size) {
	void* returnData = malloc(size);

	Protect(address, size, [&]() {
		memcpy(returnData, (void*)address, size);
	});

	return returnData;
}

void hijack::memory::Patch(size_t address, size_t size, void* data) {
	Protect(address, size, [&]() {
		memcpy((void*)address, data, size);
	});
}
