#pragma once

#include <algorithm>
#include <cstring>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "ChatFilterCore.h"

// Words for the chat filter page, compared the way the chat filter compares them (see ModerationTools.h)
namespace ModerationTools {
	constexpr size_t MAX_FILTER_WORD = 64;

	// A word or phrase staff typed for the chat filter, as the filter compares it (lower case, no ! ? ; . ,, words joined by
	// one space); nullopt if it isn't 1-64 characters or has no word. Pure; unit tested.
	inline std::optional<std::string> FilterWord(std::string text) {
		text.erase(0, text.find_first_not_of(" \t\r\n"));
		text.erase(text.find_last_not_of(" \t\r\n") + 1);
		if (text.empty() || text.size() > MAX_FILTER_WORD) return std::nullopt;
		auto entry = ChatFilterWords::NormalizeEntry(text);
		if (entry.empty()) return std::nullopt;
		return entry;
	}

	// Whether an entry is a phrase (more than one word)
	inline bool IsPhrase(const std::string& entry) { return ChatFilterWords::WordCount(entry) > 1; }

	// The words of a plain word list (chatplus_en_us.txt) as the filter reads them: one per line, lower case; sorted, each once
	inline std::vector<std::string> FileWords(const std::string& text) {
		std::vector<std::string> words;
		size_t start = 0;
		while (start < text.size()) {
			const auto end = std::min(text.find('\n', start), text.size());
			auto line = text.substr(start, end - start);
			std::erase(line, '\r');
			line = ChatFilterWords::AsciiLower(std::move(line));
			if (!line.empty()) words.push_back(std::move(line));
			start = end + 1;
		}
		std::sort(words.begin(), words.end());
		words.erase(std::unique(words.begin(), words.end()), words.end());
		return words;
	}

	// Whether a message contains a word or phrase (FilterWord) as the chat filter reads it: whole words, in a row, skipping
	// pieces that are only punctuation
	inline bool HasFilterWord(const std::string& message, const std::string& entry) {
		const auto tokens = ChatFilterWords::Tokenize(message);
		const auto words = ChatFilterWords::WordCount(entry);
		return !ChatFilterWords::FindBlocked(tokens, words, [&entry](const std::string& run) { return run == entry; }).empty();
	}

	// What the filter decides about one word of a message, and why
	struct WordVerdict {
		std::string text;   // as typed
		std::string word;   // as the filter compares it
		bool stopped{};
		std::string reason; // blocked_here, allowed_here, allow_file, character_name, not_allowed, block_file, not_in_block_file, no_block_file
		std::string phrase; // the blocked or allowed phrase this word is part of, when it was a phrase
	};

	// Where the filter finds its words (callbacks keep this pure; the route reads the files and the database)
	struct WordSources {
		std::function<std::optional<bool>(const std::string&)> dashboard; // true allowed here, false blocked here, nullopt neither
		std::function<bool(const std::string&)> allowFile;                // chatplus_en_us.txt
		std::function<bool(const std::string&)> characterName;            // approved character names count as allowed words
		std::function<bool(const std::string&)> blockFile;                // blocklist.dcf (by hash)
		bool blockFileLoaded{};
		uint32_t maxWords{ 1 };                                           // the longest blocked phrase, in words (here or in the file)
		uint32_t maxAllowedWords{ 1 };                                    // the longest allowed phrase, in words (here or in the file)
	};

	/**
	 * Each word of a message with what dChatFilter::IsSentenceOkay decides about it for a player below GM level 2 (higher
	 * levels skip the filter). Blocked words and phrases are stopped (here always, the block file's phrases always and its
	 * single words in free chat); a phrase stops each of its words. Normal chat (allowList) needs every other word allowed,
	 * on its own or as part of an allowed phrase; best friends' free
	 * chat stops only blocked ones, or every word when there is no blocked words file. Words are split at spaces as the
	 * filter splits them (ChatFilterWords::CheckMessage). Pure; unit tested.
	 */
	inline std::vector<WordVerdict> ExplainMessage(const std::string& message, bool allowList, const WordSources& sources) {
		const auto tokens = ChatFilterWords::Tokenize(message);
		std::vector<WordVerdict> verdicts;
		for (const auto& token : tokens) verdicts.push_back({ message.substr(token.position, token.length), token.word });
		if (!allowList && !sources.blockFileLoaded) {
			for (auto& verdict : verdicts) {
				verdict.stopped = true;
				verdict.reason = "no_block_file";
			}
			return verdicts;
		}

		const auto blockedHere = [&sources](const std::string& entry) { const auto here = sources.dashboard(entry); return here && !*here; };
		const auto matches = ChatFilterWords::FindBlocked(tokens, std::max(sources.maxWords, 1u), [&](const std::string& entry) {
			return blockedHere(entry) || ((!allowList || ChatFilterWords::IsPhrase(entry)) && sources.blockFile(entry));
		});
		for (const auto& match : matches) {
			const auto reason = blockedHere(match.entry) ? "blocked_here" : "block_file";
			for (size_t i = match.first; i <= match.last; i++) {
				verdicts[i].stopped = true;
				verdicts[i].reason = reason;
				if (ChatFilterWords::WordCount(match.entry) > 1) verdicts[i].phrase = match.entry;
			}
		}

		// Allowed phrases in normal chat: their words pass together (a stopped word breaks a phrase)
		if (allowList && sources.maxAllowedWords > 1) {
			auto open = tokens;
			for (size_t i = 0; i < open.size(); i++) if (verdicts[i].stopped && !open[i].word.empty()) open[i].word = "\x01";
			const auto allowedHere = [&sources](const std::string& entry) { const auto here = sources.dashboard(entry); return here && *here; };
			const auto allowed = ChatFilterWords::FindBlocked(open, sources.maxAllowedWords, [&](const std::string& entry) {
				return ChatFilterWords::IsPhrase(entry) && (sources.allowFile(entry) || allowedHere(entry));
			});
			for (const auto& match : allowed) {
				const auto reason = sources.allowFile(match.entry) ? "allow_file" : "allowed_here";
				for (size_t i = match.first; i <= match.last; i++) {
					verdicts[i].reason = reason;
					verdicts[i].phrase = match.entry;
				}
			}
		}

		for (auto& verdict : verdicts) {
			if (verdict.stopped || !verdict.phrase.empty()) continue;
			const auto here = sources.dashboard(verdict.word);
			if (!allowList) {
				verdict.reason = "not_in_block_file";
			} else if (sources.allowFile(verdict.word)) {
				verdict.reason = "allow_file";
			} else if (here && *here) {
				verdict.reason = "allowed_here";
			} else if (sources.characterName(verdict.word)) {
				verdict.reason = "character_name";
			} else {
				verdict.stopped = true;
				verdict.reason = "not_allowed";
			}
		}
		return verdicts;
	}
}
