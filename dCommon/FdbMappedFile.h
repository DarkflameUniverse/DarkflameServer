#ifndef FDBMAPPEDFILE_H
#define FDBMAPPEDFILE_H

#include <cstdint>
#include <filesystem>
#include <vector>

/**
 * A read-only view of a whole file. The file is memory-mapped when the platform allows it
 * (CreateFileMapping/MapViewOfFile on Windows, mmap elsewhere), so every process that maps the
 * same file shares one copy through the OS page cache. If mapping fails the file is read into
 * memory instead, and the view works the same way.
 */
class FdbMappedFile {
public:
	FdbMappedFile() = default;
	~FdbMappedFile();

	FdbMappedFile(const FdbMappedFile&) = delete;
	FdbMappedFile& operator=(const FdbMappedFile&) = delete;
	FdbMappedFile(FdbMappedFile&& other) noexcept;
	FdbMappedFile& operator=(FdbMappedFile&& other) noexcept;

	/**
	 * Opens a file. Closes whatever was open before.
	 *
	 * @param path The file to open
	 * @param allowMapping false skips the mapping and reads the file into memory
	 * @return true if the file is open and not empty
	 */
	bool Open(const std::filesystem::path& path, bool allowMapping = true);

	void Close();

	[[nodiscard]] bool IsOpen() const { return m_Data != nullptr; }

	// true if the view is a mapping, false if the file was read into memory
	[[nodiscard]] bool IsMapped() const { return m_Mapped; }

	[[nodiscard]] const uint8_t* GetData() const { return m_Data; }

	[[nodiscard]] uint64_t GetSize() const { return m_Size; }

private:
	bool Map(const std::filesystem::path& path);
	bool Read(const std::filesystem::path& path);

	const uint8_t* m_Data = nullptr;
	uint64_t m_Size = 0;
	bool m_Mapped = false;
	std::vector<uint8_t> m_Buffer;
#ifdef _WIN32
	void* m_FileHandle = nullptr;
	void* m_MappingHandle = nullptr;
#endif
};

#endif // FDBMAPPEDFILE_H
