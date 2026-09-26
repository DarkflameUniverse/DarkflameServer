#pragma once

#include <algorithm>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "dChatFilter.h"

// Words for the chat filter page, compared the way the chat filter compares them (see ModerationTools.h)
namespace ModerationTools {
	constexpr size_t MAX_FILTER_WORD = 64;

	// A word staff typed for the chat filter, as the filter compares it (lower case, no ! ? ; . ,); nullopt if it isn't
	// one word of 1-64 characters. Pure; unit tested.
	inline std::optional<std::string> FilterWord(std::string text) {
		text.erase(0, text.find_first_not_of(" \t\r\n"));
		text.erase(text.find_last_not_of(" \t\r\n") + 1);
		if (text.empty() || text.size() > MAX_FILTER_WORD || text.find_first_of(" \t\r\n") != std::string::npos) return std::nullopt;
		auto word = dChatFilter::NormalizeWord(text);
		if (word.empty()) return std::nullopt;
		return word;
	}

	// The words of a plain word list (chatplus_en_us.txt) as the filter reads them: one per line, lower case; sorted, each once
	inline std::vector<std::string> FileWords(const std::string& text) {
		std::vector<std::string> words;
		size_t start = 0;
		while (start < text.size()) {
			const auto end = std::min(text.find('\n', start), text.size());
			auto line = text.substr(start, end - start);
			std::erase(line, '\r');
			std::transform(line.begin(), line.end(), line.begin(), ::tolower);
			if (!line.empty()) words.push_back(std::move(line));
			start = end + 1;
		}
		std::sort(words.begin(), words.end());
		words.erase(std::unique(words.begin(), words.end()), words.end());
		return words;
	}

	// The hashes of a .dcf word list (blocklist.dcf), as dChatFilter::ReadWordlistDCF reads them; nullopt if it isn't one
	inline std::optional<std::vector<size_t>> DcfHashes(const std::string& bytes) {
		dChatFilterDCF::fileHeader header{};
		size_t count = 0;
		if (bytes.size() < sizeof(header) + sizeof(count)) return std::nullopt;
		std::memcpy(&header, bytes.data(), sizeof(header));
		if (header.header != dChatFilterDCF::header || header.formatVersion != dChatFilterDCF::formatVersion) return std::nullopt;
		std::memcpy(&count, bytes.data() + sizeof(header), sizeof(count));
		const size_t offset = sizeof(header) + sizeof(count);
		if (count > (bytes.size() - offset) / sizeof(size_t)) return std::nullopt;
		std::vector<size_t> hashes(count);
		if (count) std::memcpy(hashes.data(), bytes.data() + offset, count * sizeof(size_t));
		return hashes;
	}

	// A word's hash as the filter stores it (dChatFilter::CalculateHash)
	inline size_t WordHash(const std::string& word) { return std::hash<std::string>{}(word); }

	// Whether a message contains `word` as one of the words the chat filter checks
	inline bool HasFilterWord(const std::string& message, const std::string& word) {
		const auto words = dChatFilter::Words(message);
		return std::find(words.begin(), words.end(), word) != words.end();
	}
}
