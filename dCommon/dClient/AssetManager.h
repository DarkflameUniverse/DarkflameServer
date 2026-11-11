#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <optional>
#include <string_view>

#include "Pack.h"
#include "PackIndex.h"

enum class eAssetBundleType {
	None,
	Unpacked,
	Packed
};

struct AssetMemoryBuffer : std::streambuf {
	char* m_Base;
	bool m_Success;

	AssetMemoryBuffer(char* base, std::ptrdiff_t n, bool success) {
		m_Base = base;
		m_Success = success;
		if (!m_Success) return;
		this->setg(base, base, base + n);
	}

	~AssetMemoryBuffer() {
		if (m_Success) free(m_Base);
	}

	pos_type seekpos(pos_type sp, std::ios_base::openmode which) override {
		return seekoff(sp - pos_type(off_type(0)), std::ios_base::beg, which);
	}

	pos_type seekoff(off_type off,
		std::ios_base::seekdir dir,
		std::ios_base::openmode which = std::ios_base::in) override {
		if (dir == std::ios_base::cur)
			gbump(off);
		else if (dir == std::ios_base::end)
			setg(eback(), egptr() + off, egptr());
		else if (dir == std::ios_base::beg)
			setg(eback(), eback() + off, egptr());
		return gptr() - eback();
	}

	// Every byte of the file (empty when it wasn't found)
	std::string_view Bytes() const {
		if (!m_Success) return {};
		return std::string_view(eback(), static_cast<size_t>(egptr() - eback()));
	}
};

struct AssetStream : std::istream {
	AssetStream(char* base, std::ptrdiff_t n, bool success) : std::istream(new AssetMemoryBuffer(base, n, success)) {}

	~AssetStream() {
		delete rdbuf();
	}

	operator bool() {
		return reinterpret_cast<AssetMemoryBuffer*>(rdbuf())->m_Success;
	}

	// Every byte of the file, whatever has been read of it so far
	std::string_view Bytes() const {
		return reinterpret_cast<const AssetMemoryBuffer*>(rdbuf())->Bytes();
	}
};

class AssetManager {
public:
	AssetManager(const std::filesystem::path& path);

	[[nodiscard]]
	const std::filesystem::path& GetResPath() const {
		return m_ResPath;
	}
	
	[[nodiscard]]
	eAssetBundleType GetAssetBundleType() const {
		return m_AssetBundleType;
	}

	/**
	 * A folder of edited zone files (zone_override_dir): a file under maps/ is read from <dir>/maps/... when it is
	 * there, and from the client otherwise. Nothing else is looked for in it, and the client's files are never
	 * written. Empty turns it off.
	 */
	void SetOverrideDir(const std::filesystem::path& dir);

	[[nodiscard]]
	const std::filesystem::path& GetOverrideDir() const {
		return m_OverrideDir;
	}

	// Where name is read from in the override folder: only for a name under maps/ whose file is there
	[[nodiscard]]
	std::optional<std::filesystem::path> GetOverridePath(std::string name) const;

	/**
	 * The path inside an override folder a name under maps/ is kept at (lowercase, forward slashes), whether or not
	 * a file is there. nullopt for any other name and for one with an empty, "." or ".." part.
	 */
	[[nodiscard]]
	static std::optional<std::string> OverrideName(std::string name);

	[[nodiscard]]
	bool HasFile(std::string name) const;

	[[nodiscard]]
	bool GetFile(std::string name, char** data, uint32_t* len) const;

	// Where GetFile reads name from when it is a loose file in res; nullopt when it comes from a pack or isn't there
	[[nodiscard]]
	std::optional<std::filesystem::path> GetLoosePath(std::string name) const;

	[[nodiscard]]
	AssetStream GetFile(const char* name) const;

	[[nodiscard]]
	AssetStream GetFile(const std::string& name) const { return GetFile(name.c_str()); };

private:
	void LoadPackIndex();

	// Modified crc algorithm (mpeg2)
	// Reference: https://stackoverflow.com/questions/54339800/how-to-modify-crc-32-to-crc-32-mpeg-2
	static inline uint32_t crc32b(uint32_t crc, std::string_view message);

	std::filesystem::path m_Path;
	std::filesystem::path m_RootPath;
	std::filesystem::path m_ResPath;
	std::filesystem::path m_OverrideDir;

	eAssetBundleType m_AssetBundleType = eAssetBundleType::None;

	std::optional<PackIndex> m_PackIndex;
};
