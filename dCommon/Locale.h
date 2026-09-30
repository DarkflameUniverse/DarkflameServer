#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace Locale {
	/**
	 * Load the client's locale.xml: the phrases of `locale` (the default, which GetPhrase(id) answers from) and, with
	 * `allLocales`, those of every other locale the file lists too. Replaces what an earlier load read. Call before
	 * other threads read phrases; lookups only read afterwards.
	 */
	void LoadFromFile(const std::string& filepath, const std::string& locale = "en_US", bool allLocales = false);
	const std::string& GetPhrase(const std::string& phraseId);
	// The phrase in `locale`, else in the default locale; empty when neither has it
	const std::string& GetPhrase(const std::string& phraseId, const std::string& locale);
	// The locales loaded ("en_US", "de_DE", ...), the default first; empty before a load
	const std::vector<std::string>& GetLocales();
	bool IsLoaded();
	std::vector<std::string> GetPhraseIdsWithPrefix(const std::string& prefix);
};
