#include "UgcRoutes.h"

#include "Database.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "RouteUtils.h"
#include "Sd0.h"
#include "WSRoutes.h"
#include "dConfig.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	constexpr uint32_t PAGE_SIZE = 50;

	const char* StateName(IUgc::eProcessState state) {
		switch (state) {
		case IUgc::eProcessState::PENDING: return "pending";
		case IUgc::eProcessState::DONE: return "done";
		case IUgc::eProcessState::FAILED: return "failed";
		}
		return "unknown";
	}

	std::optional<IUgc::eProcessState> ParseState(const std::string& text) {
		if (text == "pending") return IUgc::eProcessState::PENDING;
		if (text == "done") return IUgc::eProcessState::DONE;
		if (text == "failed") return IUgc::eProcessState::FAILED;
		return std::nullopt;
	}

	nlohmann::json Counts(const std::vector<std::pair<IUgc::eProcessState, uint64_t>>& counts) {
		nlohmann::json out = { { "pending", 0 }, { "done", 0 }, { "failed", 0 } };
		for (const auto& [state, count] : counts) out[StateName(state)] = count;
		return out;
	}
}

namespace UgcRoutes {
	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/ugc", Perm("properties_view"), "What the UGC server made of players' models",
			[](HTTPReply& reply, const HTTPContext& context) { RenderPage(reply, context, "ugc.jinja2", "ugc"); });

		Route(eHTTPMethod::GET, "/api/ugc", Perm("properties_view"),
			"Processing state: {counts: {model, modular: {pending, done, failed}}, items: [{id, characterId, characterName, state, attempts, processedAt, "
			"error, bakeAo, modules}], ugcUrl, canManage}. Query: ?kind=model|modular&state=pending|done|failed&page=",
			[](HTTPReply& reply, const HTTPContext& context) {
				const bool modular = QueryValue(context.queryString, "kind") == "modular";
				const auto state = ParseState(QueryValue(context.queryString, "state"));
				const auto page = GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "page")).value_or(0);
				const auto list = modular ? Database::Get()->GetModularBuildProcessList(state, page * PAGE_SIZE, PAGE_SIZE + 1)
					: Database::Get()->GetUgcProcessList(state, page * PAGE_SIZE, PAGE_SIZE + 1);
				nlohmann::json items = nlohmann::json::array();
				for (size_t i = 0; i < list.size() && i < PAGE_SIZE; i++) {
					const auto& info = list[i];
					items.push_back({ { "id", std::to_string(info.id) }, { "characterId", std::to_string(info.characterId) }, { "characterName", info.characterName },
						{ "state", StateName(info.state) }, { "attempts", info.attempts }, { "processedAt", info.processedAt }, { "error", info.error },
						{ "bakeAo", info.bakeAo }, { "modules", info.details } });
				}
				JsonSuccess(reply, { { "counts", { { "model", Counts(Database::Get()->GetUgcProcessCounts()) }, { "modular", Counts(Database::Get()->GetModularBuildProcessCounts()) } } },
					{ "items", items }, { "more", list.size() > PAGE_SIZE }, { "ugcUrl", Game::config->GetValue("ugc_public_url") }, { "canManage", Can(context, "ugc_manage") } });
			});

		Route(eHTTPMethod::GET, "/api/ugc/:id/lxfml", Perm("properties_view"), "A player model's LXFML",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = PathId<LWOOBJID>(context.path, 2);
				if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid id");
				auto model = Database::Get()->GetUgcModel(*id);
				if (!model) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such model");
				Sd0 sd0(model->lxfmlData);
				reply.status = eHTTPStatusCode::OK;
				reply.contentType = eContentType::TEXT_PLAIN;
				reply.message = sd0.GetAsStringUncompressed();
			});

		Route(eHTTPMethod::POST, "/api/ugc/reprocess", Perm("ugc_manage"),
			"Have the UGC server make items again. Body: {kind: model|modular, id} for one, {kind, failedOnly: true} for the failed ones, {kind} for all",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const bool modular = body->value("kind", "model") == "modular";
				std::optional<LWOOBJID> id;
				if (body->contains("id")) {
					id = GeneralUtils::TryParse<LWOOBJID>((*body)["id"].is_string() ? (*body)["id"].get<std::string>() : (*body)["id"].dump());
					if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid id");
				}
				const bool failedOnly = body->value("failedOnly", false);
				const auto changed = modular ? Database::Get()->ResetModularBuildProcessing(id, failedOnly) : Database::Get()->ResetUgcModelProcessing(id, failedOnly);
				const std::string what = modular ? "modular build" : "model";
				Audit(context, "ugc_reprocess", "Queued " + std::to_string(changed) + " " + what + "(s) to be made again" +
					(id ? " (" + std::to_string(*id) + ")" : failedOnly ? " (the failed ones)" : " (all)"));
				BroadcastTableChanged("ugc");
				JsonSuccess(reply, { { "message", std::to_string(changed) + " " + what + (changed == 1 ? "" : "s") + " will be made again" } });
			});
	}
}
