#include "FeaturedProperties.h"

#include <algorithm>
#include <ctime>
#include <map>

#include "RouteUtils.h"
#include "DashboardRoutes.h"
#include "WSRoutes.h"
#include "CDClientDatabase.h"
#include "Database.h"
#include "HotPropertySlots.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	using HotPropertySlots::eMode;
	constexpr uint32_t MAX_MATCHES = 25;
	constexpr size_t MAX_SEARCH = 64;

	// The news screen's slots and the property worlds they can show properties of, from the CDClient
	struct NewsWorlds {
		std::vector<HotPropertySlots::Slot> slots;
		std::vector<uint32_t> worlds;
	};

	const NewsWorlds& GetNewsWorlds() {
		static const auto news = [] {
			std::vector<HotPropertySlots::TemplateRow> templates;
			auto rows = CDClientDatabase::ExecuteQuery("SELECT id, mapID, spawnName FROM PropertyTemplate;");
			for (; !rows.eof(); rows.nextRow()) {
				templates.push_back({ static_cast<uint32_t>(rows.getIntField("id")), static_cast<uint32_t>(rows.getIntField("mapID")), rows.getStringField("spawnName", "") });
			}
			std::vector<HotPropertySlots::EntranceRow> entrances;
			auto entranceRows = CDClientDatabase::ExecuteQuery("SELECT mapID, propertyName FROM PropertyEntranceComponent;");
			for (; !entranceRows.eof(); entranceRows.nextRow()) {
				entrances.push_back({ static_cast<uint32_t>(entranceRows.getIntField("mapID")), entranceRows.getStringField("propertyName", "") });
			}
			return NewsWorlds{ HotPropertySlots::ResolveSlots(templates, entrances), HotPropertySlots::PropertyWorlds(templates, entrances) };
		}();
		return news;
	}

	const HotPropertySlots::Slot* FindSlot(std::optional<uint32_t> templateId) {
		const auto& slots = GetNewsWorlds().slots;
		const auto slot = std::find_if(slots.begin(), slots.end(), [&](const auto& s) { return templateId && s.templateId == *templateId; });
		return slot == slots.end() ? nullptr : &*slot;
	}

	// A location given by the dashboard: only the slot's own world (missing means that too), since the news screen
	// names the slot's world whatever property is sent for it (HotPropertySlots::Location)
	std::optional<uint32_t> ParseLocation(const nlohmann::json& value, const HotPropertySlots::Slot& slot) {
		if (value.is_null()) return slot.mapId;
		const auto mapId = value.is_string() ? GeneralUtils::TryParse<uint32_t>(value.get<std::string>())
			: value.is_number_unsigned() ? std::optional<uint32_t>(value.get<uint32_t>()) : std::nullopt;
		if (mapId != slot.mapId) return std::nullopt;
		return mapId;
	}

	std::string ZoneName(uint32_t zoneId) {
		const auto& zones = ZoneNames();
		const auto key = std::to_string(zoneId);
		return zones.contains(key) && zones[key].is_string() ? zones[key].get<std::string>() : "Zone " + key;
	}

	nlohmann::json PropertyJson(const IProperty::Info& info, const std::string& ownerName) {
		return {
			{"id", std::to_string(info.id)},
			{"name", info.name},
			{"owner_id", std::to_string(info.ownerId)},
			{"owner_name", ownerName},
			{"reputation", info.reputation},
			{"performance_cost", info.performanceCost},
			{"last_updated", info.lastUpdatedTime},
			{"zone_id", info.zoneId},
			{"zone_name", ZoneName(info.zoneId)},
		};
	}

	// The world's (0: every world's) approved public properties matching `search` (name, description or owner), most reputation first
	IProperty::ShowcaseResult Candidates(uint32_t mapId, uint32_t length, const std::string& search = "") {
		IProperty::ShowcaseQuery query;
		query.search = search;
		query.zoneId = mapId;
		query.sort = IProperty::ShowcaseSort::REPUTATION;
		query.length = length;
		return Database::Get()->GetShowcaseProperties(query);
	}

	// A picked property with its owner's name, if it may be shown in a slot at that location
	std::optional<std::pair<IProperty::Info, std::string>> Featurable(LWOOBJID propertyId, uint32_t location) {
		const auto info = Database::Get()->GetPropertyInfo(propertyId);
		if (!info || !HotPropertySlots::Featurable(info->modApproved, info->privacyOption, info->zoneId, location)) return std::nullopt;
		const auto owner = Database::Get()->GetCharacterInfo(info->ownerId);
		if (!owner) return std::nullopt;
		return std::make_pair(*info, owner->name);
	}

	// Everything the dashboard shows; what each slot shows is resolved the way the world servers do it (see
	// LoadHotProperties in GameMessages.cpp)
	nlohmann::json StateJson() {
		const auto& news = GetNewsWorlds();
		const auto settings = Database::Get()->GetFeaturedPropertiesSettings();
		std::map<uint32_t, IFeaturedProperties::FeaturedSlot> chosen;
		for (const auto& row : Database::Get()->GetFeaturedPropertySlots()) chosen[row.templateId] = row;

		std::vector<HotPropertySlots::Choice> choices;
		for (const auto& slot : news.slots) {
			const auto it = chosen.find(slot.templateId);
			HotPropertySlots::Choice choice{ eMode::AUTO, slot.mapId };
			if (it != chosen.end()) choice = { HotPropertySlots::ModeFromInt(it->second.mode), HotPropertySlots::Location(it->second.zoneId, slot, news.worlds), it->second.propertyId };
			choices.push_back(choice);
		}

		std::vector<HotPropertySlots::Candidate> candidates;
		std::map<LWOOBJID, nlohmann::json> properties;
		for (const auto world : HotPropertySlots::CandidateWorlds(choices, settings.fullAuto)) {
			for (const auto& entry : Candidates(world, HotPropertySlots::CANDIDATES_PER_WORLD).entries) {
				candidates.push_back({ entry.info.id, entry.info.zoneId, entry.info.reputation });
				properties.emplace(entry.info.id, PropertyJson(entry.info, entry.ownerName));
			}
		}
		// The picks, shown or not, if they may still be shown at their slot's location
		std::vector<nlohmann::json> picked(choices.size());
		for (size_t i = 0; i < choices.size(); i++) {
			if (choices[i].mode != eMode::PICKED) continue;
			const auto property = Featurable(choices[i].propertyId, choices[i].mapId);
			if (!property) continue;
			picked[i] = PropertyJson(property->first, property->second);
			if (!settings.fullAuto && !properties.contains(property->first.id)) {
				candidates.push_back({ property->first.id, property->first.zoneId, property->first.reputation });
				properties.emplace(property->first.id, picked[i]);
			}
		}
		const auto showing = HotPropertySlots::Resolve(choices, settings.fullAuto, candidates);

		nlohmann::json list = nlohmann::json::array();
		for (size_t i = 0; i < news.slots.size(); i++) {
			const auto& slot = news.slots[i];
			const auto& choice = choices[i];
			const auto it = chosen.find(slot.templateId);
			nlohmann::json entry{
				{"template_id", slot.templateId},
				{"map_id", slot.mapId},
				{"zone_name", ZoneName(slot.mapId)},
				{"spawn_name", slot.spawnName},
				{"location", choice.mapId},
				{"location_name", ZoneName(choice.mapId)},
				{"mode", HotPropertySlots::ModeName(choice.mode)},
				{"property_id", nullptr},
				{"picked", nullptr},
				{"picked_unavailable", false},
				{"picked_shown_elsewhere", false},
				{"showing", showing[i].propertyId ? properties[*showing[i].propertyId] : nlohmann::json(nullptr)},
				{"candidate_total", Candidates(choice.mapId, 1).total},
				{"updated_at", it == chosen.end() ? 0 : it->second.updatedAt},
				{"updated_by", it == chosen.end() ? "" : it->second.updatedBy},
			};
			if (choice.mode == eMode::PICKED) {
				const auto& property = picked[i];
				entry["property_id"] = std::to_string(choice.propertyId);
				entry["picked"] = property;
				entry["picked_unavailable"] = property.is_null();
				entry["picked_shown_elsewhere"] = !property.is_null() && showing[i].pickFellBack;
			}
			list.push_back(std::move(entry));
		}

		nlohmann::json worlds = nlohmann::json::array();
		for (const auto world : news.worlds) worlds.push_back({ {"map_id", world}, {"zone_name", ZoneName(world)} });
		return {
			{"full_auto", settings.fullAuto},
			{"settings_updated_at", settings.updatedAt},
			{"settings_updated_by", settings.updatedBy},
			{"worlds", worlds},
			{"slots", list},
		};
	}
}

void FeaturedProperties::RegisterRoutes() {
	Route(eHTTPMethod::GET, "/api/featured_properties", Perm("feature_properties"),
		"The news screen's \"Today's Top Properties\": full auto or per slot, the property worlds, and for each slot its location, what was chosen and what it shows",
		[](HTTPReply& reply, const HTTPContext& context) {
			auto state = StateJson();
			state["success"] = true;
			JsonReply(reply, eHTTPStatusCode::OK, state);
		});

	Route(eHTTPMethod::POST, "/api/featured_properties", Perm("feature_properties"),
		"Switch \"Today's Top Properties\" between full auto (the top four approved public properties across every property world) and per slot. Body: {full_auto: bool}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			if (!body->contains("full_auto") || !(*body)["full_auto"].is_boolean()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "full_auto must be true or false");

			IFeaturedProperties::FeaturedSettings settings;
			settings.fullAuto = (*body)["full_auto"].get<bool>();
			settings.updatedAt = static_cast<int64_t>(std::time(nullptr));
			settings.updatedBy = context.authenticatedUser;
			Database::Get()->SetFeaturedPropertiesSettings(settings);
			Audit(context, "feature_property", std::string("Top properties: ") + (settings.fullAuto ? "full auto (the top properties across every property world)" : "per slot"));
			BroadcastTableChanged("featured_properties");
			JsonSuccess(reply, StateJson());
		});

	Route(eHTTPMethod::GET, "/api/featured_properties/:template/candidates", Perm("feature_properties"),
		"Approved public properties a \"Today's Top Properties\" slot can show, most reputation first (at most 25). Only the slot's own world (the news screen names it). Query: ?search= (name, description or owner)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto* slot = FindSlot(PathId<uint32_t>(context.path, 2));
			if (!slot) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such slot");
			const auto locationValue = QueryValue(context.queryString, "location");
			const auto location = ParseLocation(locationValue.empty() ? nlohmann::json(nullptr) : nlohmann::json(locationValue), *slot);
			if (!location) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "A slot can only show a property of its own world (the news screen names that world)");

			const auto candidates = Candidates(*location, MAX_MATCHES, QueryValue(context.queryString, "search").substr(0, MAX_SEARCH));
			nlohmann::json list = nlohmann::json::array();
			for (const auto& candidate : candidates.entries) list.push_back(PropertyJson(candidate.info, candidate.ownerName));
			JsonSuccess(reply, { {"total", candidates.total}, {"properties", list} });
		});

	Route(eHTTPMethod::POST, "/api/featured_properties/:template", Perm("feature_properties"),
		"Choose what a \"Today's Top Properties\" slot shows. Body: {mode: auto|picked|empty, property_id (for picked: an approved public property of the slot's own world that no other slot picked)}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto templateId = PathId<uint32_t>(context.path, 2);
			if (!templateId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid slot");
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");

			const auto* slot = FindSlot(templateId);
			if (!slot) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such slot");

			const auto mode = HotPropertySlots::ParseMode(body->value("mode", ""));
			if (!mode) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "mode must be auto, picked or empty");
			const auto location = ParseLocation(body->contains("location") ? (*body)["location"] : nlohmann::json(nullptr), *slot);
			if (!location) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "A slot can only show a property of its own world (the news screen names that world)");

			IFeaturedProperties::FeaturedSlot row;
			row.templateId = slot->templateId;
			row.mode = static_cast<uint8_t>(*mode);
			row.zoneId = *location == slot->mapId ? 0 : *location; // 0 keeps following the slot's own world
			row.updatedAt = static_cast<int64_t>(std::time(nullptr));
			row.updatedBy = context.authenticatedUser;

			std::string what = std::string(HotPropertySlots::ModeName(*mode));
			AuditTarget target;
			if (*mode == eMode::PICKED) {
				const auto& idValue = (*body)["property_id"];
				const auto propertyId = idValue.is_string() ? GeneralUtils::TryParse<LWOOBJID>(idValue.get<std::string>())
					: idValue.is_number_integer() ? std::optional<LWOOBJID>(idValue.get<LWOOBJID>()) : std::nullopt;
				if (!propertyId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "property_id is required to pick a property");
				const auto picked = Featurable(*propertyId, *location);
				if (!picked) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Only an approved public property in " + ZoneName(*location) + " can be picked there");
				for (const auto& other : Database::Get()->GetFeaturedPropertySlots()) {
					if (other.templateId == slot->templateId || HotPropertySlots::ModeFromInt(other.mode) != eMode::PICKED || other.propertyId != *propertyId) continue;
					const auto* otherSlot = FindSlot(other.templateId);
					if (!otherSlot) continue;
					return JsonError(reply, eHTTPStatusCode::CONFLICT, "That property is already picked for the " + ZoneName(otherSlot->mapId) + " slot");
				}
				const auto& [info, ownerName] = *picked;
				row.propertyId = *propertyId;
				what = "property " + info.name + " (ID " + std::to_string(*propertyId) + ") by " + ownerName;
				target = AuditTarget::Character(info.ownerId);
			}

			Database::Get()->SetFeaturedPropertySlot(row);
			Audit(context, "feature_property", "Top properties slot " + ZoneName(slot->mapId) + " (template " + std::to_string(slot->templateId) + "): " + ZoneName(*location) + ", " + what, target);
			BroadcastTableChanged("featured_properties", std::to_string(slot->templateId));
			JsonSuccess(reply, StateJson());
		});
}
