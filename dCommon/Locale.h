#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace Locale {
	void LoadFromFile(const std::string& filepath, const std::string& locale = "en_US");
	const std::string& GetPhrase(const std::string& phraseId);
	bool IsLoaded();
	std::vector<std::string> GetPhraseIdsWithPrefix(const std::string& prefix);
};
