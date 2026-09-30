#include "GameText.h"

#include <algorithm>
#include <cctype>
#include <map>

#include "HTTPContext.h"
#include "Locale.h"
#include "LocaleText.h"

namespace {
	thread_local std::string t_Language = GameText::DEFAULT_LANGUAGE;

	// Zone names per language, built by Init (read-only afterwards)
	std::map<std::string, nlohmann::json> g_ZoneNames;
	const nlohmann::json g_NoZones = nlohmann::json{ { "0", "Character Select" } };

	// The part of a tag before the region: "de_DE" -> "de"
	std::string_view Primary(std::string_view tag) {
		return tag.substr(0, tag.find('_'));
	}

	bool EqualNoCase(std::string_view a, std::string_view b) {
		return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
			return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
		});
	}

	std::string_view Trim(std::string_view text) {
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
		return text;
	}

	nlohmann::json BuildZoneNames(const std::string& language) {
		nlohmann::json names = g_NoZones;
		constexpr std::string_view PREFIX = "ZoneTable_", SUFFIX = "_DisplayDescription";
		for (const auto& key : Locale::GetPhraseIdsWithPrefix(std::string(PREFIX))) {
			if (key.size() <= PREFIX.size() + SUFFIX.size() || !key.ends_with(SUFFIX)) continue;
			const auto& name = Locale::GetPhrase(key, language);
			if (!name.empty()) names[key.substr(PREFIX.size(), key.size() - PREFIX.size() - SUFFIX.size())] = name;
		}
		return names;
	}
}

namespace GameText {
	std::string NormalizeTag(std::string_view tag) {
		std::string out(Trim(tag));
		std::replace(out.begin(), out.end(), '-', '_');
		const auto split = out.find('_');
		for (size_t i = 0; i < out.size(); i++) {
			const auto c = static_cast<unsigned char>(out[i]);
			out[i] = static_cast<char>(split != std::string::npos && i > split ? std::toupper(c) : std::tolower(c));
		}
		return out;
	}

	std::optional<std::string> MatchAcceptLanguage(std::string_view header, const std::vector<std::string>& available) {
		struct Wanted { std::string tag; double weight; size_t order; };
		std::vector<Wanted> wanted;
		size_t order = 0;
		while (!header.empty()) {
			const auto comma = header.find(',');
			auto part = header.substr(0, comma);
			header = comma == std::string_view::npos ? std::string_view{} : header.substr(comma + 1);
			double weight = 1.0;
			if (const auto semi = part.find(';'); semi != std::string_view::npos) {
				const auto param = Trim(part.substr(semi + 1));
				if (param.starts_with("q=")) {
					try { weight = std::stod(std::string(param.substr(2))); } catch (...) { weight = 0.0; }
				}
				part = part.substr(0, semi);
			}
			const auto tag = NormalizeTag(part);
			if (tag.empty() || tag == "*" || weight <= 0.0) continue;
			wanted.push_back({ tag, weight, order++ });
		}
		std::stable_sort(wanted.begin(), wanted.end(), [](const Wanted& a, const Wanted& b) { return a.weight > b.weight; });
		for (const auto& want : wanted) {
			for (const auto& language : available) if (EqualNoCase(NormalizeTag(language), want.tag)) return language;
			for (const auto& language : available) if (EqualNoCase(Primary(NormalizeTag(language)), Primary(want.tag))) return language;
		}
		return std::nullopt;
	}

	std::string CookieValue(std::string_view cookieHeader, std::string_view name) {
		while (!cookieHeader.empty()) {
			const auto semi = cookieHeader.find(';');
			const auto part = Trim(cookieHeader.substr(0, semi));
			cookieHeader = semi == std::string_view::npos ? std::string_view{} : cookieHeader.substr(semi + 1);
			const auto equals = part.find('=');
			if (equals != std::string_view::npos && Trim(part.substr(0, equals)) == name) return std::string(Trim(part.substr(equals + 1)));
		}
		return "";
	}

	std::string ChooseLanguage(std::string_view cookie, std::string_view acceptLanguage, const std::vector<std::string>& available) {
		if (!cookie.empty()) {
			for (const auto& language : available) if (language == cookie) return language;
		}
		if (const auto match = MatchAcceptLanguage(acceptLanguage, available)) return *match;
		return available.empty() ? DEFAULT_LANGUAGE : available.front();
	}

	std::string Key(std::string_view table, int64_t id, std::string_view column) {
		std::string key(table);
		key += '_';
		key += std::to_string(id);
		key += '_';
		key += column;
		return key;
	}

	const std::vector<std::string>& Languages() {
		static const std::vector<std::string> fallback{ DEFAULT_LANGUAGE };
		const auto& loaded = Locale::GetLocales();
		return loaded.empty() ? fallback : loaded;
	}

	const std::string& Language() {
		return t_Language;
	}

	LanguageScope::LanguageScope(std::string language) : m_Previous(std::move(t_Language)) {
		t_Language = std::move(language);
	}

	LanguageScope::LanguageScope(const HTTPContext& context)
		: LanguageScope(ChooseLanguage(CookieValue(context.GetHeader("cookie"), COOKIE), context.GetHeader("accept-language"), Languages())) {}

	LanguageScope::~LanguageScope() {
		t_Language = std::move(m_Previous);
	}

	const std::string& Phrase(const std::string& key) {
		return Locale::GetPhrase(key, t_Language);
	}

	std::string Text(const std::string& key, const std::string& fallback) {
		const auto& phrase = Phrase(key);
		return phrase.empty() ? fallback : phrase;
	}

	std::string TextOrKey(const std::string& key) {
		return Text(key, key);
	}

	std::string Name(std::string_view table, int64_t id, std::string_view column) {
		return Text(Key(table, id, column), std::string(table) + " " + std::to_string(id));
	}

	std::string ZoneName(uint32_t zoneId) {
		const auto& names = ZoneNames();
		const auto key = std::to_string(zoneId);
		if (const auto it = names.find(key); it != names.end() && it->is_string()) return it->get<std::string>();
		return Text("ZoneTable_" + key + "_DisplayDescription", "Zone " + key);
	}

	const nlohmann::json& ZoneNames() {
		if (const auto it = g_ZoneNames.find(t_Language); it != g_ZoneNames.end()) return it->second;
		if (const auto it = g_ZoneNames.find(Languages().front()); it != g_ZoneNames.end()) return it->second;
		return g_NoZones;
	}

	std::string Expand(const std::string& text) {
		return LocaleText::Expand(text, [](const std::string& key) -> const std::string& { return Phrase(key); });
	}

	const std::vector<std::pair<std::string, std::string>>& TermKeys() {
		static const std::vector<std::pair<std::string, std::string>> keys{
			{ "coins", "UI_COINS" },
			{ "uscore", "UI_UNIVERSE_SCORE" },
			{ "imagination", "UI_IMAGINATION" },
			{ "reputation", "UI_REPUTATION" },
			{ "life", "UI_HEALTH" },
		};
		return keys;
	}

	nlohmann::json Terms() {
		nlohmann::json terms = nlohmann::json::object();
		for (const auto& [name, key] : TermKeys()) terms[name] = TextOrKey(key);
		return terms;
	}

	nlohmann::json PageJson() {
		return { { "language", t_Language }, { "languages", Languages() }, { "terms", Terms() }, { "zones", ZoneNames() } };
	}

	void Init() {
		g_ZoneNames.clear();
		for (const auto& language : Languages()) g_ZoneNames[language] = BuildZoneNames(language);
	}
}
