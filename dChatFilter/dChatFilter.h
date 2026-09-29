#pragma once
#include <algorithm>
#include <cctype>
#include <set>
#include <unordered_set>
#include <vector>
#include <string>

#include "dCommonVars.h"

enum class eGameMasterLevel : uint8_t;
namespace dChatFilterDCF {
	static const uint32_t header = ('D' + ('C' << 8) + ('F' << 16) + ('B' << 24));
	static const uint32_t formatVersion = 2;

	struct fileHeader {
		uint32_t header;
		uint32_t formatVersion;
	};
};

class dChatFilter
{
public:
	dChatFilter(const std::string& filepath, bool dontGenerateDCF);
	~dChatFilter();

	void ReadWordlistPlaintext(const std::string& filepath, bool allowList);
	bool ReadWordlistDCF(const std::string& filepath, bool allowList);
	void ExportWordlistToDCF(const std::string& filepath, bool allowList);
	std::set<std::pair<uint8_t, uint8_t>> IsSentenceOkay(const std::string& message, eGameMasterLevel gmLevel, bool allowList = true);
	// Whether a deny list is loaded (without one, IsSentenceOkay(..., false) refuses every message)
	bool HasDenyList() const { return !m_DeniedWords.empty(); }

	/**
	 * Load the words staff added on the dashboard (chat_filter_words) again, replacing the ones loaded before.
	 * Allowed words are accepted in whitelisted chat; blocked words are always stopped, even when a file allows them.
	 */
	void ReloadCustomWords();

	// A word as the filter compares it: lower case, without ! ? ; . ,
	static std::string NormalizeWord(std::string word) {
		std::erase_if(word, [](char c) { return c == '!' || c == '?' || c == ';' || c == '.' || c == ','; });
		std::transform(word.begin(), word.end(), word.begin(), ::tolower); //Transform to lowercase
		return word;
	}

	// A message split into words (at spaces) the way the filter checks it
	static std::vector<std::string> Words(const std::string& message) {
		std::vector<std::string> words;
		size_t start = 0;
		while (start <= message.size()) {
			const auto end = std::min(message.find(' ', start), message.size());
			words.push_back(NormalizeWord(message.substr(start, end - start)));
			start = end + 1;
		}
		return words;
	}

private:
	bool m_DontGenerateDCF;
	std::vector<size_t> m_DeniedWords;
	std::vector<size_t> m_ApprovedWords;
	std::vector<size_t> m_UserUnapprovedWordCache;
	std::unordered_set<size_t> m_CustomAllowedWords;
	std::unordered_set<size_t> m_CustomBlockedWords;

	//Private functions:
	size_t CalculateHash(const std::string& word);
};
