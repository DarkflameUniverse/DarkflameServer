#include "ContrabandRoutes.h"

#include <ctime>

#include "APIRoutes.h"
#include "CDClientDatabase.h"
#include "ClientAssets.h"
#include "Database.h"
#include "DashboardRoutes.h"
#include "GeneralUtils.h"
#include "master/PlayerAction.h"
#include "PlayerActions.h"
#include "RouteUtils.h"
#include "WSRoutes.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	using eContrabandAction = IContraband::eContrabandAction;
	constexpr size_t MAX_REASON = 300;

	const char* ActionName(eContrabandAction action) {
		return action == eContrabandAction::REMOVE ? "remove" : "flag";
	}

	std::optional<eContrabandAction> ParseAction(const std::string& text) {
		if (text == "flag") return eContrabandAction::FLAG;
		if (text == "remove") return eContrabandAction::REMOVE;
		return std::nullopt;
	}

	bool IsItem(LOT lot) {
		auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT 1 FROM Objects WHERE id = ? LIMIT 1;");
		stmt.bind(1, static_cast<int32_t>(lot));
		return !stmt.execQuery().eof();
	}

	std::string Trimmed(std::string text, size_t max) {
		text.erase(0, text.find_first_not_of(" \t\r\n"));
		text.erase(text.find_last_not_of(" \t\r\n") + 1);
		return text.substr(0, max);
	}

	// Tell every running world to load the list again
	uint32_t ReloadWorlds(uint32_t requester) {
		PlayerActionRequest request;
		request.action = ePlayerAction::RELOAD_CONTRABAND;
		return PlayerActions::Request(request, requester, [](const PlayerActionResult& result) {
			return PlayerActions::Outcome{ true, result.affected ? "The list was updated in " + std::to_string(result.affected) + " world(s)"
				: "No world is running; they'll use the list when they start" };
		});
	}
}

namespace ContrabandRoutes {
	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/contraband", Perm("reports_view"), "Items players aren't allowed to have",
			[](HTTPReply& reply, const HTTPContext& context) { RenderPage(reply, context, "contraband.jinja2", "contraband"); });

		Route(eHTTPMethod::GET, "/api/contraband", Perm("reports_view"),
			"The contraband list: {items: [{lot, name, reason, action: flag|remove, added_by, added_at}], canManage}",
			[](HTTPReply& reply, const HTTPContext& context) {
				nlohmann::json items = nlohmann::json::array();
				for (const auto& item : Database::Get()->GetContrabandItems()) {
					items.push_back({ {"lot", item.lot}, {"name", ClientAssets::ItemName(item.lot)}, {"reason", item.reason}, {"action", ActionName(item.action)},
						{"added_by", item.addedBy}, {"added_at", item.addedAt} });
				}
				JsonSuccess(reply, { {"items", items}, {"canManage", Can(context, "contraband_manage")} });
			});

		Route(eHTTPMethod::GET, "/api/contraband/items", Perm("contraband_manage"), "Search items to add by name or LOT: [{lot, name}]. Query: ?q=",
			[](HTTPReply& reply, const HTTPContext& context) {
				JsonReply(reply, eHTTPStatusCode::OK, SearchItems(Trimmed(QueryValue(context.queryString, "q"), 64)));
			});

		Route(eHTTPMethod::POST, "/api/contraband", Perm("contraband_manage"),
			"Add an item to the list or change its entry; running worlds pick it up at once. Body: {lot, reason, action: flag|remove}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto lot = body->contains("lot") && (*body)["lot"].is_number_integer() ? std::optional<LOT>((*body)["lot"].get<LOT>())
					: GeneralUtils::TryParse<LOT>(body->value("lot", ""));
				if (!lot || *lot <= 0 || !IsItem(*lot)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Pick an item from the list or enter its LOT");
				const auto action = ParseAction(body->value("action", "flag"));
				if (!action) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "action must be flag or remove");
				const auto reason = Trimmed(body->value("reason", ""), MAX_REASON);

				bool existed = false;
				for (const auto& item : Database::Get()->GetContrabandItems()) existed = existed || item.lot == *lot;
				Database::Get()->SetContrabandItem({ *lot, reason, *action, context.authenticatedUser, static_cast<int64_t>(std::time(nullptr)) });
				const auto name = ClientAssets::ItemName(*lot);
				Audit(context, existed ? "contraband_change" : "contraband_add", std::string(existed ? "Changed" : "Added") + " contraband item " + name + " (" +
					std::to_string(*lot) + "): " + (*action == eContrabandAction::REMOVE ? "flag and remove" : "flag only") + (reason.empty() ? "" : ", \"" + reason + "\""));
				BroadcastTableChanged("contraband");
				JsonSuccess(reply, { {"message", name + (existed ? " updated" : " added")}, {"requestId", ReloadWorlds(context.accountId)} });
			});

		Route(eHTTPMethod::POST, "/api/contraband/delete", Perm("contraband_manage"), "Take an item off the list. Body: {lot}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				const auto lot = body && body->contains("lot") && (*body)["lot"].is_number_integer() ? std::optional<LOT>((*body)["lot"].get<LOT>()) : std::nullopt;
				if (!lot) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid lot");
				if (!Database::Get()->DeleteContrabandItem(*lot)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "That item isn't on the list");
				const auto name = ClientAssets::ItemName(*lot);
				Audit(context, "contraband_remove", "Took " + name + " (" + std::to_string(*lot) + ") off the contraband list");
				BroadcastTableChanged("contraband");
				JsonSuccess(reply, { {"message", name + " is no longer contraband"}, {"requestId", ReloadWorlds(context.accountId)} });
			});
	}
}
