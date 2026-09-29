#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

/**
 * Small pure decisions about UGC shared by the worlds, the UGC server and the dashboard (docs/UgcServer.md).
 */
namespace UgcModularKey {
	// The module LOTs of a ugc_modular_build.ldf_config ("1:4713+1:4714+1:4715", also with ';' or ',' between)
	inline std::vector<uint32_t> Lots(std::string_view ldf) {
		std::vector<uint32_t> lots;
		size_t start = 0;
		while (start <= ldf.size()) {
			size_t end = ldf.find_first_of("+;,", start);
			if (end == std::string_view::npos) end = ldf.size();
			auto item = ldf.substr(start, end - start);
			if (const auto colon = item.find(':'); colon != std::string_view::npos) item = item.substr(colon + 1);
			while (!item.empty() && item.front() == ' ') item.remove_prefix(1);
			while (!item.empty() && item.back() == ' ') item.remove_suffix(1);
			uint32_t value = 0;
			bool digits = !item.empty() && item.size() <= 9;
			for (const char c : item) {
				if (c < '0' || c > '9') {
					digits = false;
					break;
				}
				value = value * 10 + static_cast<uint32_t>(c - '0');
			}
			if (digits && value != 0) lots.push_back(value);
			start = end + 1;
		}
		return lots;
	}

	/**
	 * The combination of modules a build is made of, the same however its ldf_config is written: its LOTs sorted
	 * and joined by '-' ("4713-4714-4715"). Each module LOT belongs to one slot of one build type, so the set of LOTs
	 * says everything about how the build looks. Empty when there are no modules.
	 */
	inline std::string Normalize(std::string_view ldf) {
		auto lots = Lots(ldf);
		std::sort(lots.begin(), lots.end());
		lots.erase(std::unique(lots.begin(), lots.end()), lots.end());
		std::string key;
		for (const auto lot : lots) key += (key.empty() ? "" : "-") + std::to_string(lot);
		return key;
	}

	// The id a combination's files are stored under (FNV-1a of the key, positive, never 0)
	inline int64_t StorageId(std::string_view key) {
		uint64_t hash = 14695981039346656037ull;
		for (const char c : key) hash = (hash ^ static_cast<uint8_t>(c)) * 1099511628211ull;
		hash &= 0x3FFFFFFFFFFFFFFFull;
		return static_cast<int64_t>(hash == 0 ? 1 : hash);
	}
}

namespace UgcDebounce {
	// When a model saved at `now` may be made: after `quietSeconds` (0 or less: right away, stored as 0)
	inline int64_t ProcessAfter(int64_t now, int64_t quietSeconds) {
		return quietSeconds > 0 ? now + quietSeconds : 0;
	}

	// Whether a waiting model is due: its quiet period is over, or someone asked for its files
	inline bool Due(int64_t processAfter, int64_t now, bool requested) {
		return requested || processAfter <= now;
	}
}

/**
 * How a model is made, where there is more than one way to try (docs/UgcServer.md, "Processing options"): what traces
 * the rays (ray_backend) and whether icons are denoised (denoise). The UGC settings give the defaults; staff may pick
 * others for one make (the dashboard, /reprocessproperty), and each made model records what made it. Written as the
 * chosen names in this order, separated by spaces ("embree oidn"); a choice left out is the setting's.
 */
namespace UgcProcessOptions {
	inline constexpr std::string_view RAYS[] = { "embree", "hiprt" };
	inline constexpr std::string_view DENOISE[] = { "off", "oidn" };
	// Hidden-face methods earlier versions' options named (stored options still have them): read and ignored
	inline constexpr std::string_view RETIRED[] = { "toolbox", "fast" };

	struct Choice {
		std::string rays;    // one of RAYS, or empty for the setting
		std::string denoise; // one of DENOISE, or empty

		bool Empty() const { return rays.empty() && denoise.empty(); }
	};

	template<size_t N>
	inline bool Contains(const std::string_view (&names)[N], std::string_view word) {
		return std::find(std::begin(names), std::end(names), word) != std::end(names);
	}

	// The words, in any order ("oidn embree"); "default" or "-" leave a choice to the setting, retired words are
	// skipped and builtin is embree. False on an unknown word or two words for one choice.
	inline bool Parse(std::string_view text, Choice& choice) {
		choice = {};
		size_t start = 0;
		while (start < text.size()) {
			while (start < text.size() && (text[start] == ' ' || text[start] == ',')) start++;
			size_t end = start;
			while (end < text.size() && text[end] != ' ' && text[end] != ',') end++;
			auto word = text.substr(start, end - start);
			start = end;
			if (word.empty() || word == "default" || word == "-" || Contains(RETIRED, word)) continue;
			// builtin, the ray backend Embree replaced, is embree
			if (word == "builtin") word = "embree";
			std::string* slot = Contains(RAYS, word) ? &choice.rays : Contains(DENOISE, word) ? &choice.denoise : nullptr;
			if (!slot || !slot->empty()) return false;
			*slot = std::string(word);
		}
		return true;
	}

	// The choices made, in order ("embree oidn"); empty when all are the settings'
	inline std::string ToString(const Choice& choice) {
		std::string text;
		for (const auto* part : { &choice.rays, &choice.denoise }) {
			if (part->empty()) continue;
			if (!text.empty()) text += ' ';
			text += *part;
		}
		return text;
	}
}
