#include "FdbMappedFile.h"

#include <fstream>
#include <limits>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

FdbMappedFile::~FdbMappedFile() {
	Close();
}

FdbMappedFile::FdbMappedFile(FdbMappedFile&& other) noexcept {
	*this = std::move(other);
}

FdbMappedFile& FdbMappedFile::operator=(FdbMappedFile&& other) noexcept {
	if (this == &other) return *this;
	Close();
	m_Mapped = std::exchange(other.m_Mapped, false);
	m_Size = std::exchange(other.m_Size, 0);
	m_Buffer = std::move(other.m_Buffer);
	other.m_Buffer.clear();
	const uint8_t* data = std::exchange(other.m_Data, nullptr);
	// A moved vector keeps its heap block, so the pointer stays valid; set it again anyway for clarity
	m_Data = m_Mapped ? data : (m_Buffer.empty() ? nullptr : m_Buffer.data());
#ifdef _WIN32
	m_FileHandle = std::exchange(other.m_FileHandle, nullptr);
	m_MappingHandle = std::exchange(other.m_MappingHandle, nullptr);
#endif
	return *this;
}

bool FdbMappedFile::Open(const std::filesystem::path& path, bool allowMapping) {
	Close();
	if (allowMapping && Map(path)) return true;
	return Read(path);
}

void FdbMappedFile::Close() {
	if (m_Mapped && m_Data) {
#ifdef _WIN32
		UnmapViewOfFile(m_Data);
#else
		munmap(const_cast<uint8_t*>(m_Data), static_cast<std::size_t>(m_Size));
#endif
	}
#ifdef _WIN32
	if (m_MappingHandle) CloseHandle(static_cast<HANDLE>(m_MappingHandle));
	if (m_FileHandle) CloseHandle(static_cast<HANDLE>(m_FileHandle));
	m_MappingHandle = nullptr;
	m_FileHandle = nullptr;
#endif
	m_Data = nullptr;
	m_Size = 0;
	m_Mapped = false;
	m_Buffer.clear();
	m_Buffer.shrink_to_fit();
}

bool FdbMappedFile::Map(const std::filesystem::path& path) {
#ifdef _WIN32
	HANDLE file = CreateFileW(path.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;

	LARGE_INTEGER size{};
	if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0) {
		CloseHandle(file);
		return false;
	}

	// Size 0/0 maps the whole file
	HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (!mapping) {
		CloseHandle(file);
		return false;
	}

	const void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
	if (!view) {
		CloseHandle(mapping);
		CloseHandle(file);
		return false;
	}

	m_FileHandle = file;
	m_MappingHandle = mapping;
	m_Data = static_cast<const uint8_t*>(view);
	m_Size = static_cast<uint64_t>(size.QuadPart);
	m_Mapped = true;
	return true;
#else
	const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0) return false;

	struct stat info {};
	if (fstat(fd, &info) != 0 || info.st_size <= 0 ||
		static_cast<uint64_t>(info.st_size) > static_cast<uint64_t>(std::numeric_limits<std::size_t>::max())) {
		close(fd);
		return false;
	}

	const auto length = static_cast<std::size_t>(info.st_size);
	void* view = mmap(nullptr, length, PROT_READ, MAP_SHARED, fd, 0);
	// The mapping keeps its own reference to the file
	close(fd);
	if (view == MAP_FAILED) return false;

	m_Data = static_cast<const uint8_t*>(view);
	m_Size = static_cast<uint64_t>(info.st_size);
	m_Mapped = true;
	return true;
#endif
}

bool FdbMappedFile::Read(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) return false;

	const std::streamoff size = file.tellg();
	if (size <= 0) return false;

	std::vector<uint8_t> buffer(static_cast<std::size_t>(size));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return false;

	m_Buffer = std::move(buffer);
	m_Data = m_Buffer.data();
	m_Size = static_cast<uint64_t>(size);
	m_Mapped = false;
	return true;
}
