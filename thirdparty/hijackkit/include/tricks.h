#pragma once

#include <Windows.h>
#include <winnt.h>
#include <ntifs.h>

#include <vector>

namespace hijack {
	namespace tricks {
		struct ModuleInfo {
			wchar_t* m_ModuleName;
			size_t m_ModuleBase;
		};
	
		template<class T>
		inline T* GetVFunc(void* instance, size_t index) {
			return (T*)*(size_t*)((size_t)instance + index * sizeof(size_t));
		}

		inline std::vector<ModuleInfo> LookupDLL() {
			size_t pedAddr = __readgsqword(0x60);

			size_t ldrData = *(size_t*)(pedAddr + 0x18);
			size_t firstEntry = *(size_t*)(ldrData + 0x10);
			size_t currentEntry = firstEntry;

			std::vector<ModuleInfo> modules;

			while (*(DWORD*)(currentEntry + 0x60) != NULL) {
				wchar_t* dllName = (wchar_t*)(currentEntry + 0x60);
				size_t dllBase = *(size_t*)(currentEntry + 0x30);

				ModuleInfo info;
				info.m_ModuleName = dllName;
				info.m_ModuleBase = dllBase;
				
				modules.push_back(info);
				
				currentEntry = *(size_t*)currentEntry;
			}
			
			return modules;
		}
	}
}