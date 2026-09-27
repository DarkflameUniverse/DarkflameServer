#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * The client's brick data the UGC server builds models from: LDD geometry (res/brickprimitives/lod<n>/<design>.g, .g1,
 * ...) and the material colors (Materials.xml in res/brickdb.zip). The parsers are pure; BrickLibrary loads and caches
 * files and may be used from any thread. Files are read through a FileReader (the server's reads packed and unpacked
 * clients alike), falling back to loose files under the res folder.
 */
namespace UgcBricks {
	// One LDD .g file: a triangle mesh with a normal per vertex
	struct Geometry {
		std::vector<float> positions; // x, y, z per vertex
		std::vector<float> normals;   // x, y, z per vertex
		std::vector<uint32_t> indices;
	};

	// An LDD material: sRGB color and opacity, 0-255
	struct Material {
		uint8_t r{ 160 };
		uint8_t g{ 160 };
		uint8_t b{ 160 };
		uint8_t a{ 255 };
		bool Transparent() const { return a < 255; }
	};

	// A .g file ("10GB" magic, counts, positions, normals, texture coordinates for decorated parts, indices)
	std::optional<Geometry> ParseGeometry(std::string_view data);

	// Materials.xml: MatID -> color
	std::map<uint32_t, Material> ParseMaterials(std::string_view xml);

	// A file from a zip archive (stored or deflated), matched without regard to case; nullopt when it isn't there
	std::optional<std::string> ReadZipEntry(std::string_view zip, std::string_view name);

	// `relative` (either slash, any case) under `root`, matching each part without regard to case as the client's
	// files are named on Windows; nullopt when there is no such file
	std::optional<std::filesystem::path> ResolvePath(const std::filesystem::path& root, std::string_view relative);

	// Reads a whole file; nullopt when it can't
	std::optional<std::string> ReadFile(const std::filesystem::path& path);

	// Reads a client file by its path under res/ ("brickdb.zip", "brickprimitives/lod0/3001.g"); nullopt when there is
	// none. Called from any thread.
	using FileReader = std::function<std::optional<std::string>(const std::string& relative)>;

	class BrickLibrary {
	public:
		// `res` is the client's res folder; `lod` the brickprimitives level used (0 is the most detailed); `reader`
		// the files' source, tried before loose files under `res`
		BrickLibrary(std::filesystem::path res, uint32_t lod, FileReader reader = {});

		// Loads the material colors; false when brickdb.zip or its Materials.xml can't be read
		bool LoadMaterials();

		// For tests and callers that have the colors already
		void SetMaterials(std::map<uint32_t, Material> materials);

		// A material's color (a grey when the id is unknown)
		Material GetMaterial(uint32_t id) const;

		// Whether the client's Materials.xml has the id
		bool HasMaterial(uint32_t id) const;

		// Every geometry file of a design, in order (.g, .g1, ...); empty when the design has none. Loaded once.
		// `lod`: the brickprimitives level, the library's own when omitted; a design without that level uses the
		// next more detailed one.
		std::shared_ptr<const std::vector<Geometry>> GetDesign(uint32_t design, std::optional<uint32_t> lod = std::nullopt);

		// Designs loaded (for the memory report)
		size_t CachedDesigns() const;

		const std::filesystem::path& GetResPath() const { return m_Res; }

	private:
		// A file from the reader, else loose under m_Res
		std::optional<std::string> Read(const std::string& relative) const;

		std::filesystem::path m_Res;
		uint32_t m_Lod;
		FileReader m_Reader;
		std::map<uint32_t, Material> m_Materials;
		mutable std::mutex m_Mutex;
		std::map<uint64_t, std::shared_ptr<const std::vector<Geometry>>> m_Designs; // lod << 32 | design
	};
}
