#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

/**
 * The chat filter's words, without the server around them (pure, unit tested; dChatFilter and the dashboard both use it).
 *
 * A word is compared lower case (ASCII only, so every platform agrees) without ! ? ; . , and a phrase is its words
 * joined by one space. Entries are stored and compared by ChatFilterWords::Hash: 64-bit FNV-1a over the entry's bytes,
 * the same on every compiler, standard library and platform.
 */
namespace ChatFilterWords {
	// ASCII lower case; other bytes (UTF-8) stay as they are
	inline std::string AsciiLower(std::string text) {
		for (auto& c : text) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
		return text;
	}

	// A word as the filter compares it: lower case, without ! ? ; . ,
	inline std::string NormalizeWord(std::string word) {
		std::erase_if(word, [](char c) { return c == '!' || c == '?' || c == ';' || c == '.' || c == ','; });
		return AsciiLower(std::move(word));
	}

	// A word or phrase as the filter stores it: each word normalized, words that end up empty dropped, joined by one space
	inline std::string NormalizeEntry(std::string_view text) {
		std::string entry;
		size_t start = 0;
		while (start < text.size()) {
			auto end = text.find_first_of(" \t\r\n", start);
			if (end == std::string_view::npos) end = text.size();
			const auto word = NormalizeWord(std::string(text.substr(start, end - start)));
			if (!word.empty()) {
				if (!entry.empty()) entry += ' ';
				entry += word;
			}
			start = end + 1;
		}
		return entry;
	}

	// How many words an entry has (1 for a word, 0 for an empty entry)
	inline uint32_t WordCount(std::string_view entry) {
		return entry.empty() ? 0 : static_cast<uint32_t>(std::count(entry.begin(), entry.end(), ' ')) + 1;
	}

	// 64-bit FNV-1a: offset basis 0xcbf29ce484222325, prime 0x100000001b3, one byte at a time
	constexpr uint64_t Hash(std::string_view entry) {
		uint64_t hash = 0xcbf29ce484222325ULL;
		for (const char c : entry) {
			hash ^= static_cast<uint8_t>(c);
			hash *= 0x100000001b3ULL;
		}
		return hash;
	}

	// One piece of a message between spaces: where it is in the message and the word the filter compares
	struct Token {
		uint32_t position{};
		uint32_t length{};
		std::string word;
	};

	// A message split at each space, the way the filter checks it: two spaces in a row give an empty piece, a trailing space none
	inline std::vector<Token> Tokenize(std::string_view message) {
		std::vector<Token> tokens;
		size_t start = 0;
		while (start < message.size()) {
			auto end = message.find(' ', start);
			if (end == std::string_view::npos) end = message.size();
			tokens.push_back({ static_cast<uint32_t>(start), static_cast<uint32_t>(end - start), NormalizeWord(std::string(message.substr(start, end - start))) });
			start = end + 1;
		}
		return tokens;
	}

	// A run of tokens [first, last] that matched a blocked entry, and the longest entry that matched where the run starts
	struct Match {
		size_t first{};
		size_t last{};
		std::string entry;
	};

	/**
	 * The blocked words and phrases in a message: at each word, the longest run of up to maxWords consecutive words
	 * (tokens with no word are skipped) whose entry isBlocked accepts. Runs that share a word are merged into one.
	 */
	inline std::vector<Match> FindBlocked(const std::vector<Token>& tokens, uint32_t maxWords, const std::function<bool(const std::string&)>& isBlocked) {
		std::vector<size_t> words;
		for (size_t i = 0; i < tokens.size(); i++) if (!tokens[i].word.empty()) words.push_back(i);

		std::vector<Match> matches;
		for (size_t i = 0; i < words.size(); i++) {
			std::string entry;
			std::string best;
			size_t bestLength = 0;
			for (size_t n = 1; n <= maxWords && i + n <= words.size(); n++) {
				if (n > 1) entry += ' ';
				entry += tokens[words[i + n - 1]].word;
				if (isBlocked(entry)) {
					best = entry;
					bestLength = n;
				}
			}
			if (bestLength == 0) continue;
			const size_t first = words[i];
			const size_t last = words[i + bestLength - 1];
			if (!matches.empty() && first <= matches.back().last) {
				matches.back().last = std::max(matches.back().last, last);
			} else {
				matches.push_back({ first, last, std::move(best) });
			}
		}
		return matches;
	}

	// Hashes of words or phrases, and the most words any of them has
	struct WordList {
		std::unordered_set<uint64_t> hashes;
		uint32_t maxWords{};

		void AddEntry(std::string_view entry) {
			if (entry.empty()) return;
			hashes.insert(Hash(entry));
			maxWords = std::max(maxWords, WordCount(entry));
		}
		bool Contains(std::string_view entry) const { return hashes.contains(Hash(entry)); }
		bool Empty() const { return hashes.empty(); }
		size_t Size() const { return hashes.size(); }
	};

	// Everything the filter checks a message against
	struct Lists {
		WordList approved;      // chatplus_en_us.txt and approved character names: whitelist chat, one word at a time
		WordList denied;        // blocklist.dcf: best friends' free chat
		WordList customAllowed; // allowed on the dashboard
		WordList customBlocked; // blocked on the dashboard: stopped in every kind of chat
	};

	/**
	 * The pieces of a message the filter stops, as (position, length) in the message. Blocked words and phrases (the
	 * dashboard's always, blocklist.dcf's in free chat) are stopped as one span each. In whitelist chat (allowList) every
	 * other piece must be an allowed word, one at a time, as the client checks words. In free chat without a block list
	 * the whole message is stopped.
	 */
	inline std::set<std::pair<uint8_t, uint8_t>> CheckMessage(std::string_view message, bool allowList, const Lists& lists) {
		if (message.empty()) return {};
		if (!allowList && lists.denied.Empty()) return { { 0, static_cast<uint8_t>(message.length()) } };

		const auto tokens = Tokenize(message);
		const uint32_t maxWords = std::max(lists.customBlocked.maxWords, allowList ? 0u : lists.denied.maxWords);
		const auto matches = FindBlocked(tokens, maxWords, [&](const std::string& entry) {
			return lists.customBlocked.Contains(entry) || (!allowList && lists.denied.Contains(entry));
		});

		std::set<std::pair<uint8_t, uint8_t>> bad;
		std::vector<bool> covered(tokens.size(), false);
		for (const auto& match : matches) {
			const auto& first = tokens[match.first];
			const auto& last = tokens[match.last];
			bad.emplace(static_cast<uint8_t>(first.position), static_cast<uint8_t>(last.position + last.length - first.position));
			for (size_t i = match.first; i <= match.last; i++) covered[i] = true;
		}

		if (allowList) {
			for (size_t i = 0; i < tokens.size(); i++) {
				if (covered[i]) continue;
				const auto hash = Hash(tokens[i].word);
				if (!lists.approved.hashes.contains(hash) && !lists.customAllowed.hashes.contains(hash)) {
					bad.emplace(static_cast<uint8_t>(tokens[i].position), static_cast<uint8_t>(tokens[i].length));
				}
			}
		}
		return bad;
	}
}

/**
 * The chat filter's word list files (.dcf). These are DLU's own files: the client reads no .dcf and hashes no chat
 * words (it keeps its lists as plain text). Layout, little-endian:
 *   uint32 magic 'DCFB' | uint32 version (3) | uint32 most words in one entry | uint64 count | count x uint64 ChatFilterWords::Hash
 * Version 2 (older DLU) stored std::hash values, which differ between compilers and platforms; those can't be read.
 */
namespace dChatFilterDCF {
	constexpr uint32_t header = ('D' + ('C' << 8) + ('F' << 16) + ('B' << 24));
	constexpr uint32_t formatVersion = 3;
	constexpr uint32_t oldFormatVersion = 2;
	constexpr size_t headerSize = 4 + 4 + 4 + 8;

	// The block list's plain source (one word or phrase per line) and the .dcf built from it, next to the servers
	constexpr const char* BLOCK_LIST_TEXT = "blocklist.txt";
	constexpr const char* BLOCK_LIST_FILE = "blocklist.dcf";

	enum class eStatus : uint8_t {
		OK,
		MISSING,     // no file
		NOT_DCF,     // not a .dcf file
		OLD_FORMAT,  // version 2: platform-dependent hashes, rebuild it from the plain word list
		UNKNOWN,     // a version this server doesn't know
		TRUNCATED,   // shorter than its count says
	};

	inline const char* StatusText(eStatus status) {
		switch (status) {
		case eStatus::OK: return "ok";
		case eStatus::MISSING: return "missing";
		case eStatus::NOT_DCF: return "not a .dcf file";
		case eStatus::OLD_FORMAT: return "old format (version 2, platform-dependent hashes)";
		case eStatus::UNKNOWN: return "unknown version";
		case eStatus::TRUNCATED: return "truncated";
		}
		return "unknown";
	}

	struct ParseResult {
		eStatus status{ eStatus::NOT_DCF };
		uint32_t version{};
		ChatFilterWords::WordList list;
	};

	namespace detail {
		inline uint64_t ReadLE(std::string_view bytes, size_t offset, size_t size) {
			uint64_t value = 0;
			for (size_t i = 0; i < size; i++) value |= static_cast<uint64_t>(static_cast<uint8_t>(bytes[offset + i])) << (8 * i);
			return value;
		}
		inline void WriteLE(std::string& out, uint64_t value, size_t size) {
			for (size_t i = 0; i < size; i++) out.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
		}
	}

	inline ParseResult Parse(std::string_view bytes) {
		ParseResult result;
		if (bytes.size() < 8 || detail::ReadLE(bytes, 0, 4) != header) return result;
		result.version = static_cast<uint32_t>(detail::ReadLE(bytes, 4, 4));
		if (result.version == oldFormatVersion) {
			result.status = eStatus::OLD_FORMAT;
			return result;
		}
		if (result.version != formatVersion) {
			result.status = eStatus::UNKNOWN;
			return result;
		}
		result.status = eStatus::TRUNCATED;
		if (bytes.size() < headerSize) return result;
		const auto maxWords = static_cast<uint32_t>(detail::ReadLE(bytes, 8, 4));
		const auto count = detail::ReadLE(bytes, 12, 8);
		if (count > (bytes.size() - headerSize) / 8) return result;
		result.list.maxWords = maxWords;
		result.list.hashes.reserve(count);
		for (uint64_t i = 0; i < count; i++) result.list.hashes.insert(detail::ReadLE(bytes, headerSize + i * 8, 8));
		result.status = eStatus::OK;
		return result;
	}

	// A list as a .dcf file; hashes sorted, so the same words always give the same bytes
	inline std::string Serialize(const ChatFilterWords::WordList& list) {
		std::vector<uint64_t> hashes(list.hashes.begin(), list.hashes.end());
		std::sort(hashes.begin(), hashes.end());
		std::string out;
		out.reserve(headerSize + hashes.size() * 8);
		detail::WriteLE(out, header, 4);
		detail::WriteLE(out, formatVersion, 4);
		detail::WriteLE(out, list.maxWords, 4);
		detail::WriteLE(out, hashes.size(), 8);
		for (const auto hash : hashes) detail::WriteLE(out, hash, 8);
		return out;
	}

	// A plain block list: one word or phrase per line, normalized (ChatFilterWords::NormalizeEntry); empty lines skipped
	inline ChatFilterWords::WordList BlockListFromText(std::string_view text) {
		ChatFilterWords::WordList list;
		size_t start = 0;
		while (start < text.size()) {
			auto end = text.find('\n', start);
			if (end == std::string_view::npos) end = text.size();
			list.AddEntry(ChatFilterWords::NormalizeEntry(text.substr(start, end - start)));
			start = end + 1;
		}
		return list;
	}

	// A plain allow list (chatplus_en_us.txt): one word per line, lower case, compared whole (as the filter always has)
	inline ChatFilterWords::WordList AllowListFromText(std::string_view text) {
		ChatFilterWords::WordList list;
		size_t start = 0;
		while (start < text.size()) {
			auto end = text.find('\n', start);
			if (end == std::string_view::npos) end = text.size();
			std::string line(text.substr(start, end - start));
			std::erase(line, '\r');
			line = ChatFilterWords::AsciiLower(std::move(line));
			list.hashes.insert(ChatFilterWords::Hash(line));
			list.maxWords = std::max(list.maxWords, 1u);
			start = end + 1;
		}
		return list;
	}

	inline std::optional<std::string> ReadBytes(const std::filesystem::path& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return std::nullopt;
		return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}

	inline ParseResult ReadFile(const std::filesystem::path& path) {
		const auto bytes = ReadBytes(path);
		if (!bytes) return { eStatus::MISSING };
		return Parse(*bytes);
	}

	// Writes the file whole or not at all (a temporary file renamed over it), so servers starting together don't read half a file
	inline bool WriteFile(const std::filesystem::path& path, const ChatFilterWords::WordList& list) {
		auto temp = path;
		temp += "." + std::to_string(std::random_device{}()) + ".tmp";
		{
			std::ofstream out(temp, std::ios::binary | std::ios::trunc);
			if (!out) return false;
			const auto bytes = Serialize(list);
			out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
			if (!out) return false;
		}
		std::error_code error;
		std::filesystem::rename(temp, path, error);
		if (!error) return true;
		std::filesystem::remove(temp, error);
		return false;
	}
}
