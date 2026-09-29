#ifndef BUILDINFO_H
#define BUILDINFO_H

#include <cstdint>
#include <string_view>

// What this binary was built from. The values live in BuildInfo.cpp, which cmake/BuildInfo.cmake regenerates on
// every build (only rewriting it when something changed), so a new commit recompiles that one file and relinks.
namespace BuildInfo {
	// Stored in 2 bits of Flags().
	enum class eBuildKind : uint8_t {
		UNKNOWN = 0,
		LOCAL = 1,
		CI = 2,
		RELEASE = 3,
	};

	constexpr uint8_t BUILD_KIND_MASK = 0b011;
	constexpr uint8_t DIRTY_FLAG = 0b100;

	extern const uint8_t versionMajor;
	extern const uint8_t versionMinor;
	extern const uint8_t versionPatch;
	extern const eBuildKind buildKind;
	extern const bool dirty; // The build had uncommitted changes to tracked files
	extern const std::string_view commit; // Full hex commit hash, empty without git
	extern const std::string_view branch; // Empty when detached or without git
	extern const std::string_view buildString; // e.g. 3.0.0-experimental+g1a2b3c4d-dirty

	// The build kind in bits 0-1 and the dirty flag in bit 2.
	inline uint8_t Flags() {
		return (static_cast<uint8_t>(buildKind) & BUILD_KIND_MASK) | (dirty ? DIRTY_FLAG : 0);
	}

	// The first 32 bits (8 hex digits) of the commit hash, 0 without git.
	inline uint32_t CommitPrefix() {
		uint32_t value = 0;
		for (size_t i = 0; i < 8 && i < commit.size(); i++) {
			const char c = commit[i];
			uint32_t digit = 0;
			if (c >= '0' && c <= '9') digit = c - '0';
			else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
			else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
			else return 0;
			value = (value << 4) | digit;
		}
		return commit.size() >= 8 ? value : 0;
	}
}

#endif // BUILDINFO_H
