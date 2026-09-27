#include "PropertyRentRoutes.h"

#include <ctime>

#include "CDClientDatabase.h"
#include "Database.h"
#include "DashboardRoutes.h"
#include "dConfig.h"
#include "EconomyPlaces.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "HotPropertySlots.h"
#include "PropertyRentRules.h"
#include "RouteUtils.h"
#include "WSRoutes.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	struct Templates {
		std::vector<HotPropertySlots::TemplateRow> rows;
		std::vector<HotPropertySlots::EntranceRow> entrances;
		std::vector<uint32_t> worlds;
	};

	const Templates& GetTemplates() {
		static const auto templates = [] {
			Templates t;
			auto rows = CDClientDatabase::ExecuteQuery("SELECT id, mapID, spawnName, minimumPrice, rentDuration, durationType, reputationPerMinute FROM PropertyTemplate;");
			for (; !rows.eof(); rows.nextRow()) {
				t.rows.push_back({ static_cast<uint32_t>(rows.getIntField("id")), static_cast<uint32_t>(rows.getIntField("mapID")), rows.getStringField("spawnName", ""),
					rows.getIntField("minimumPrice", 0), rows.getIntField("rentDuration", 0), rows.getIntField("durationType", 0), rows.getIntField("reputationPerMinute", 0) });
			}
			auto entrances = CDClientDatabase::ExecuteQuery("SELECT mapID, propertyName FROM PropertyEntranceComponent;");
			for (; !entrances.eof(); entrances.nextRow()) {
				t.entrances.push_back({ static_cast<uint32_t>(entrances.getIntField("mapID")), entrances.getStringField("propertyName", "") });
			}
			t.worlds = HotPropertySlots::PropertyWorlds(t.rows, t.entrances);
			return t;
		}();
		return templates;
	}

	nlohmann::json RateJson(const std::optional<PropertyRentRules::Rate>& rate) {
		if (!rate) return nullptr;
		return { {"price", rate->price}, {"periodDays", rate->periodSeconds / PropertyRentRules::DAY} };
	}
}

namespace PropertyRentRoutes {
	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/property_rent", Perm("properties_view"), "What owners pay for each property world",
			[](HTTPReply& reply, const HTTPContext& context) { RenderPage(reply, context, "property_rent.jinja2", "property_rent"); });

		Route(eHTTPMethod::GET, "/api/property_rent", Perm("properties_view"),
			"Each property world's rent: {enabled, graceDays, canManage, worlds: [{mapId, name, template: {price, periodDays}|null, "
			"override: {price, periodDays, updated_by, updated_at}|null, rent: {price, periodDays}|null}]} (rent null: free)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto& templates = GetTemplates();
				const auto overrides = Database::Get()->GetPropertyRentRates();
				nlohmann::json worlds = nlohmann::json::array();
				for (const auto mapId : templates.worlds) {
					const auto* row = HotPropertySlots::WorldTemplate(templates.rows, templates.entrances, mapId);
					const auto fromTemplate = row ? PropertyRentRules::TemplateRate(row->minimumPrice, row->rentDuration, row->durationType) : std::nullopt;
					nlohmann::json world{ {"mapId", mapId}, {"name", EconomyPlaces::ZoneName(mapId)}, {"template", RateJson(fromTemplate)}, {"override", nullptr},
						{"rent", RateJson(fromTemplate)} };
					for (const auto& rate : overrides) {
						if (rate.mapId != mapId) continue;
						world["override"] = { {"price", rate.price}, {"periodDays", rate.periodDays}, {"updated_by", rate.updatedBy}, {"updated_at", rate.updatedAt} };
						world["rent"] = RateJson(PropertyRentRules::Resolve(fromTemplate, rate.price, rate.periodDays));
					}
					worlds.push_back(world);
				}
				JsonSuccess(reply, { {"enabled", Game::config->GetValue("property_rent_enabled") == "1"},
					{"graceDays", GeneralUtils::TryParse<int64_t>(Game::config->GetValue("property_rent_grace_days")).value_or(3)},
					{"canManage", Can(context, "property_rent_manage")}, {"worlds", worlds} });
			});

		Route(eHTTPMethod::POST, "/api/property_rent", Perm("property_rent_manage"),
			"Set a property world's rent instead of its template's. Body: {mapId, price (coins, 0: free), periodDays (0: the template's)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto mapId = body->value("mapId", 0u);
				const auto& worlds = GetTemplates().worlds;
				if (std::find(worlds.begin(), worlds.end(), mapId) == worlds.end()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Not a property world");
				const auto price = body->value("price", int64_t{ -1 });
				const auto periodDays = body->value("periodDays", -1);
				if (price < 0 || price > 1000000000) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The price must be 0 to 1000000000 coins");
				if (periodDays < 0 || periodDays > 3650) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The period must be 0 to 3650 days");
				Database::Get()->SetPropertyRentRate({ mapId, price, periodDays, context.authenticatedUser, static_cast<int64_t>(std::time(nullptr)) });
				const auto name = EconomyPlaces::ZoneName(mapId);
				Audit(context, "property_rent_set", name + " (" + std::to_string(mapId) + "): rent " + (price == 0 ? std::string("free") :
					std::to_string(price) + " coins every " + (periodDays ? std::to_string(periodDays) + " days" : std::string("template period"))));
				BroadcastTableChanged("property_rent");
				JsonSuccess(reply, { {"message", "Rent for " + name + " saved"} });
			});

		Route(eHTTPMethod::GET, "/api/properties/:id/reputation", Perm("properties_view"),
			"Reputation a property got from visitors in the last 30 days: {reputation, days: [{day, visitors, points, seconds}]} (days since the Unix epoch, newest first)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto propertyId = PathId<LWOOBJID>(context.path, 2);
				const auto info = propertyId ? Database::Get()->GetPropertyInfo(*propertyId) : std::nullopt;
				if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Property not found");
				const auto today = static_cast<uint32_t>(std::time(nullptr) / PropertyRentRules::DAY);
				JsonSuccess(reply, { {"reputation", info->reputation}, {"days", Database::Get()->GetPropertyReputationDays(*propertyId, today >= 30 ? today - 30 : 0)} });
			});

		Route(eHTTPMethod::POST, "/api/property_rent/delete", Perm("property_rent_manage"), "Go back to the template's rent for a property world. Body: {mapId}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				const auto mapId = body ? body->value("mapId", 0u) : 0u;
				if (!Database::Get()->DeletePropertyRentRate(mapId)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "That world uses its template's rent already");
				const auto name = EconomyPlaces::ZoneName(mapId);
				Audit(context, "property_rent_reset", name + " (" + std::to_string(mapId) + "): rent back to its template's");
				BroadcastTableChanged("property_rent");
				JsonSuccess(reply, { {"message", name + " uses its template's rent again"} });
			});
	}
}
