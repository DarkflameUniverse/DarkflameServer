#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "ChatFilterCore.h"
#include "dCommonVars.h"

enum class eGameMasterLevel : uint8_t;

class dChatFilter
{
public:
	/**
	 * Loads the allow list (filepath + ".txt", cached as filepath + ".dcf") and the block list (blocklist.dcf next to the
	 * servers, rebuilt from blocklist.txt there when that file is newer). dontGenerateDCF: read the plain lists only and
	 * write no .dcf files.
	 */
	dChatFilter(const std::string& filepath, bool dontGenerateDCF);
	~dChatFilter() = default;

	std::set<std::pair<uint8_t, uint8_t>> IsSentenceOkay(const std::string& message, eGameMasterLevel gmLevel, bool allowList = true);
	// Whether a deny list is loaded (without one, IsSentenceOkay(..., false) refuses every message)
	bool HasDenyList() const { return !m_Lists.denied.Empty(); }

	/**
	 * Load the words staff added on the dashboard (chat_filter_words) again, replacing the ones loaded before.
	 * Allowed words are accepted in whitelisted chat; blocked words and phrases are always stopped, even when a file allows them.
	 */
	void ReloadCustomWords();

	// A word as the filter compares it: lower case, without ! ? ; . ,
	static std::string NormalizeWord(std::string word) { return ChatFilterWords::NormalizeWord(std::move(word)); }

	// A message split into words (at spaces) the way the filter checks it
	static std::vector<std::string> Words(const std::string& message) {
		std::vector<std::string> words;
		for (auto& token : ChatFilterWords::Tokenize(message)) words.push_back(std::move(token.word));
		return words;
	}

private:
	void LoadAllowList(const std::string& filepath);
	void LoadBlockList();

	bool m_DontGenerateDCF;
	ChatFilterWords::Lists m_Lists;
};
