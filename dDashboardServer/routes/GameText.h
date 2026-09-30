#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "json.hpp"

struct HTTPContext;

/**
 * The game's own text (zone, object, mission, activity names, currencies, ...) as the client's locale has it, in the
 * viewer's language. The dashboard never keeps its own copies of game text: C++ routes call these, templates get them
 * through the `game` data and the phrase()/zone_name() callbacks, and scripts read them with GameText.* in common.js.
 *
 * The phrases are loaded once at startup (Locale::LoadFromFile with every locale, then Init) and only read afterwards,
 * so any thread may look them up. The viewer's language is per thread: RouteUtils sets it for each request
 * (LanguageScope) and Workers::Reply carries it to the worker thread.
 */
namespace GameText {
	constexpr const char* DEFAULT_LANGUAGE = "en_US";
	constexpr const char* COOKIE = "game_lang"; // the viewer's pick (common.js keeps it in step with the account's preference)

	// ---- Pure helpers (unit tested) ----

	// "de-DE" / "de_de" -> "de_DE"; "de" -> "de"
	std::string NormalizeTag(std::string_view tag);

	/**
	 * The best of `available` (locale names like "de_DE") for an Accept-Language header ("de-CH,de;q=0.9,en;q=0.5"):
	 * by the header's weights, an exact match first, then the same language in another region (de-CH -> de_DE, en ->
	 * en_US when listed first). nullopt when nothing matches.
	 */
	std::optional<std::string> MatchAcceptLanguage(std::string_view header, const std::vector<std::string>& available);

	// A cookie's value from a Cookie header ("a=1; game_lang=de_DE"); empty when absent
	std::string CookieValue(std::string_view cookieHeader, std::string_view name);

	/**
	 * The language to show: the cookie's pick when it's one of `available`, else the best Accept-Language match,
	 * else `available`'s first (the default), else DEFAULT_LANGUAGE
	 */
	std::string ChooseLanguage(std::string_view cookie, std::string_view acceptLanguage, const std::vector<std::string>& available);

	// "<table>_<id>_<column>": Key("Objects", 1727) is "Objects_1727_name"
	std::string Key(std::string_view table, int64_t id, std::string_view column = "name");

	// ---- The viewer's language ----

	// The languages the client's locale has, the default first
	const std::vector<std::string>& Languages();
	// This thread's viewer language (DEFAULT_LANGUAGE outside a request)
	const std::string& Language();

	// Sets this thread's language for its lifetime
	class LanguageScope {
	public:
		explicit LanguageScope(std::string language);
		// The request's language (cookie, then Accept-Language)
		explicit LanguageScope(const HTTPContext& context);
		~LanguageScope();
		LanguageScope(const LanguageScope&) = delete;
		LanguageScope& operator=(const LanguageScope&) = delete;
	private:
		std::string m_Previous;
	};

	// ---- Lookups, in the viewer's language, then the default; the fallbacks are the raw key or id ----

	// The phrase for `key`; empty when the locale lacks it
	const std::string& Phrase(const std::string& key);
	// The phrase for `key`, else `fallback`
	std::string Text(const std::string& key, const std::string& fallback);
	// The phrase for `key`, else the key itself
	std::string TextOrKey(const std::string& key);
	// A row's localized column ("Missions", 5, "name"), else "<table> <id>"
	std::string Name(std::string_view table, int64_t id, std::string_view column = "name");
	// A zone's display name (ZoneTable_<id>_DisplayDescription), else "Zone <id>"; 0 is character select
	std::string ZoneName(uint32_t zoneId);
	// {"<zone id>": name} for every zone the locale names (and "0"), in the viewer's language
	const nlohmann::json& ZoneNames();
	// Every %[KEY] in `text` replaced by its phrase (mail and bug reports store text this way)
	std::string Expand(const std::string& text);

	/**
	 * The game's words the pages show as labels ({coins: "Coins", uscore: "Universe Score", imagination, reputation,
	 * life}), in the viewer's language; the key when the locale lacks one
	 */
	nlohmann::json Terms();
	// The key -> locale phrase id table behind Terms()
	const std::vector<std::pair<std::string, std::string>>& TermKeys();

	// What every page gets: {language, languages, terms, zones}
	nlohmann::json PageJson();

	// Build the per-language caches (zone names). Call on the main thread after the locale loads, before workers start.
	void Init();
}
