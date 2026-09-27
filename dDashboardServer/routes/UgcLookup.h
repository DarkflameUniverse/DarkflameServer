#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "tinyxml2.h"
#include "dCommonVars.h"
#include "GeneralUtils.h"
#include "IUgcLookup.h"
#include "LDFFormat.h"

/**
 * Finding players' creations (brick built models and modular cars and rockets) for the dashboard: reading a search,
 * finding which of a character's items are creations, and in which mail one is. Pure (no database), so it can be
 * unit tested; routes/UgcLinks.cpp runs it.
 *
 * A creation shows up as:
 * - a placed model (properties_contents.ugc_id): LOT 14 for a brick built model, its ugc_id the blueprint id
 * - a brick built model item (LOT 6662): its blueprint id in the item's config (blueprintid, saved as x@bp)
 * - a car or rocket item (any LOT): its subkey is the ugc_modular_build id
 * - a mail attachment: the same, with the config as LDF lines (attachment_config) and the subkey in attachment_subkey
 */
namespace UgcLookup {
	using eUgcKind = IUgcLookup::eUgcKind;
	using UgcSearch = IUgcLookup::UgcSearch;

	constexpr LOT MODEL_OBJECT_LOT = 14;
	constexpr LOT MODEL_ITEM_LOT = 6662;

	inline const char* KindName(eUgcKind kind) { return kind == eUgcKind::MODULAR ? "modular" : "model"; }

	inline std::optional<eUgcKind> ParseKind(std::string_view text) {
		if (text == "model") return eUgcKind::MODEL;
		if (text == "modular") return eUgcKind::MODULAR;
		return std::nullopt;
	}

	inline const char* StateName(IUgc::eProcessState state) {
		switch (state) {
		case IUgc::eProcessState::PENDING: return "pending";
		case IUgc::eProcessState::DONE: return "done";
		case IUgc::eProcessState::FAILED: return "failed";
		}
		return "unknown";
	}

	// Where the /ugc page shows a creation: /ugc?item=<id>&kind=model|modular (the page may read these to open it)
	inline std::string ViewerLink(eUgcKind kind, LWOOBJID id) {
		return "/ugc?item=" + std::to_string(id) + "&kind=" + KindName(kind);
	}

	inline std::string_view Trim(std::string_view text) {
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
		return text;
	}

	/**
	 * A search box's text: a number is matched against ids (and LOTs), anything else against names. "field: text"
	 * searches one field: id, owner (or character, account), property, model (or name), lot.
	 */
	inline UgcSearch ParseQuery(std::string_view input) {
		UgcSearch search;
		auto text = std::string(Trim(input));
		const auto colon = text.find(':');
		if (colon != std::string::npos) {
			auto key = text.substr(0, colon);
			std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			static const std::map<std::string, UgcSearch::eField> FIELDS = {
				{ "id", UgcSearch::eField::ID }, { "owner", UgcSearch::eField::OWNER }, { "character", UgcSearch::eField::OWNER },
				{ "account", UgcSearch::eField::OWNER }, { "property", UgcSearch::eField::PROPERTY }, { "model", UgcSearch::eField::MODEL },
				{ "name", UgcSearch::eField::MODEL }, { "lot", UgcSearch::eField::LOT },
			};
			const auto field = FIELDS.find(key);
			if (field != FIELDS.end()) {
				search.field = field->second;
				text = std::string(Trim(std::string_view(text).substr(colon + 1)));
			}
		}
		search.number = GeneralUtils::TryParse<int64_t>(text);
		if (search.number && *search.number < 0) search.number.reset();
		// Ids and LOTs are only numbers (anything else is searched as a name); a name may be all digits, so elsewhere a
		// number is also matched as text
		const bool numeric = search.field == UgcSearch::eField::ID || search.field == UgcSearch::eField::LOT;
		if (numeric && !search.number) search.field = UgcSearch::eField::ANY;
		if (!numeric || !search.number) search.text = text.substr(0, 64);
		return search;
	}

	// An object id kept in an item's saved config: "9:<id>" (LDF type and value) or just the number
	inline LWOOBJID ConfigId(const char* value) {
		if (!value) return LWOOBJID_EMPTY;
		std::string_view text(value);
		const auto colon = text.rfind(':');
		if (colon != std::string_view::npos) text = text.substr(colon + 1);
		return GeneralUtils::TryParse<LWOOBJID>(text).value_or(LWOOBJID_EMPTY);
	}

	// An item in a character's saved inventories that may be a creation
	struct ItemRef {
		LWOOBJID itemId{};
		LOT lot{};
		uint32_t inventory{};
		LWOOBJID subkey{};    // a modular build's id
		LWOOBJID blueprint{}; // a brick built model's blueprint (ugc) id
	};

	// Every item of a character's saved XML with a subkey or a blueprint
	inline std::vector<ItemRef> InventoryRefs(const std::string& xml) {
		std::vector<ItemRef> refs;
		tinyxml2::XMLDocument doc;
		if (xml.empty() || doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) return refs;
		const auto* obj = doc.FirstChildElement("obj");
		const auto* inv = obj ? obj->FirstChildElement("inv") : nullptr;
		const auto* items = inv ? inv->FirstChildElement("items") : nullptr;
		for (const auto* in = items ? items->FirstChildElement("in") : nullptr; in; in = in->NextSiblingElement("in")) {
			for (const auto* i = in->FirstChildElement("i"); i; i = i->NextSiblingElement("i")) {
				ItemRef ref;
				ref.itemId = i->Int64Attribute("id", 0);
				ref.lot = i->IntAttribute("l", 0);
				ref.inventory = static_cast<uint32_t>(in->IntAttribute("t", 0));
				ref.subkey = i->Int64Attribute("sk", 0);
				if (const auto* x = i->FirstChildElement("x")) {
					ref.blueprint = ConfigId(x->Attribute("bp"));
					if (ref.blueprint == LWOOBJID_EMPTY) ref.blueprint = ConfigId(x->Attribute("b"));
				}
				if (ref.subkey != LWOOBJID_EMPTY || ref.blueprint != LWOOBJID_EMPTY) refs.push_back(ref);
			}
		}
		return refs;
	}

	// The ids that InventoryRefs' items may be creations by
	inline std::vector<LWOOBJID> Candidates(const std::vector<ItemRef>& refs) {
		std::set<LWOOBJID> ids;
		for (const auto& ref : refs) {
			if (ref.blueprint != LWOOBJID_EMPTY) ids.insert(ref.blueprint);
			if (ref.subkey != LWOOBJID_EMPTY) ids.insert(ref.subkey);
		}
		return { ids.begin(), ids.end() };
	}

	struct ItemLink {
		ItemRef item;
		eUgcKind kind{};
		LWOOBJID ugcId{};
	};

	// Which items are creations, given the ids that are models and those that are modular builds. A model item goes by
	// its blueprint (its subkey is the model's own id); anything else by its subkey.
	inline std::vector<ItemLink> LinkItems(const std::vector<ItemRef>& refs, const std::set<LWOOBJID>& models, const std::set<LWOOBJID>& modular) {
		std::vector<ItemLink> links;
		for (const auto& ref : refs) {
			if (ref.blueprint != LWOOBJID_EMPTY && models.contains(ref.blueprint)) links.push_back({ ref, eUgcKind::MODEL, ref.blueprint });
			else if (ref.subkey != LWOOBJID_EMPTY && modular.contains(ref.subkey)) links.push_back({ ref, eUgcKind::MODULAR, ref.subkey });
		}
		return links;
	}

	// The creation a mail attachment is: a model item by the blueprint in its config (LDF lines), else by its subkey
	inline std::optional<std::pair<eUgcKind, LWOOBJID>> MailCreation(const IUgcLookup::UgcMail& mail, const std::set<LWOOBJID>& models, const std::set<LWOOBJID>& modular) {
		if (mail.lot == MODEL_ITEM_LOT && !mail.config.empty()) {
			LwoNameValue config;
			config.InsertLines(mail.config);
			const auto it = config.find(u"blueprintid");
			if (it != config.end() && it->second) {
				const auto blueprint = GeneralUtils::TryParse<LWOOBJID>(it->second->GetValueAsString()).value_or(LWOOBJID_EMPTY);
				if (models.contains(blueprint)) return std::make_pair(eUgcKind::MODEL, blueprint);
			}
		}
		if (mail.subkey != LWOOBJID_EMPTY && modular.contains(mail.subkey)) return std::make_pair(eUgcKind::MODULAR, mail.subkey);
		return std::nullopt;
	}
}
