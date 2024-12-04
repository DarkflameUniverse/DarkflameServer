#include "utils.h"

#include <Windows.h>
#include <TlHelp32.h>

#include <string>

#include <cstdlib>

uintptr_t hjiack::utils::GetModuleBaseAddress(const wchar_t* moduleName) {
	uintptr_t modBaseAddr = 0;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
	
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 modEntry;
		modEntry.dwSize = sizeof(modEntry);
		if (Module32First(hSnap, &modEntry))
		{
			do
			{
				auto a = std::wstring((wchar_t*)modEntry.szModule);
				if (!_wcsicmp(a.c_str(), moduleName))
				{
					modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
					break;
				}
			} while (Module32Next(hSnap, &modEntry));
		}
	}
	
	CloseHandle(hSnap);
	
	return modBaseAddr;
}

void hjiack::utils::AllocateConsole() {
	AllocConsole();


	freopen_s((FILE**)__acrt_iob_func(1), "CONOUT$", "w", __acrt_iob_func(1));
	freopen_s((FILE**)__acrt_iob_func(2), "CONOUT$", "w", __acrt_iob_func(2));
	freopen_s((FILE**)__acrt_iob_func(0), "CONIN$", "r", __acrt_iob_func(0));
}

