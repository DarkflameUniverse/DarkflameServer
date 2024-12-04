#pragma once

#include <cstdlib>

#include <functional>

namespace hijack {
	namespace memory {
		inline void Protect(size_t address, size_t size, std::function<void()> function);
		inline void* Read(size_t address, size_t size);
		inline void Patch(size_t address, size_t size, void* data);
		
		template<class T>
		inline void Patch(size_t address, T data) {
			hijack::memory::Patch(address, sizeof(T), &data);
		}
	}
}