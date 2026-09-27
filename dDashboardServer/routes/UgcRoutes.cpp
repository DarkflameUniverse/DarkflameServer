#include "UgcRoutes.h"

#include <memory>
#include <mutex>
#include <set>

#include <curl/curl.h>

#include "Database.h"
#include "NifFile.h"
#include "TtlCache.h"
#include "Workers.h"
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

	/**
	 * The dashboard's own way to the UGC server (ugc_internal_url, normally the same machine): its status and the files
	 * it made are fetched here and handed to the browser, which may not be able to reach the UGC server at all. Fetches
	 * run on worker threads (Workers::Reply) with the URL worked out on the web thread; small answers are kept briefly.
	 */
	struct Fetched {
		long status{};      // HTTP status, 0 when the UGC server didn't answer
		std::string body;
		std::string error;
	};

	std::mutex g_CacheMutex;
	TtlCache<std::string, std::shared_ptr<const Fetched>> g_Cache(std::chrono::seconds(15), 32 * 1024 * 1024);
	TtlCache<std::string, std::shared_ptr<const Fetched>> g_StatusCache(std::chrono::seconds(2), 1024 * 1024);

	constexpr size_t MAX_FETCH_BYTES = 128 * 1024 * 1024;

	size_t Collect(char* data, size_t size, size_t count, void* userData) {
		auto* out = static_cast<std::string*>(userData);
		if (out->size() + size * count > MAX_FETCH_BYTES) return 0;
		out->append(data, size * count);
		return size * count;
	}

	// Web thread: where the dashboard reaches the UGC server
	std::string InternalUrl() {
		auto url = Game::config->GetValue("ugc_internal_url");
		if (url.empty()) url = "http://127.0.0.1:2008";
		while (url.ends_with('/')) url.pop_back();
		return url;
	}

	// Any thread: a GET to the UGC server
	std::shared_ptr<const Fetched> Get(const std::string& url) {
		auto out = std::make_shared<Fetched>();
		CURL* curl = curl_easy_init();
		if (!curl) {
			out->error = "could not start a request";
			return out;
		}
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "DarkflameServer-Dashboard");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Collect);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out->body);
		const auto code = curl_easy_perform(curl);
		if (code == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out->status);
		else out->error = curl_easy_strerror(code);
		curl_easy_cleanup(curl);
		return out;
	}

	// Any thread: Get, through the cache (answers that aren't a file being made are kept)
	std::shared_ptr<const Fetched> CachedGet(const std::string& url, bool status) {
		{
			std::lock_guard lock(g_CacheMutex);
			if (auto hit = (status ? g_StatusCache : g_Cache).Get(url)) return *hit;
		}
		auto fetched = Get(url);
		if (fetched->status == 200 || fetched->status == 404) {
			std::lock_guard lock(g_CacheMutex);
			(status ? g_StatusCache : g_Cache).Put(url, fetched, fetched->body.size() + 64);
		}
		return fetched;
	}

	void FetchError(HTTPReply& reply, const Fetched& fetched, const std::string& url) {
		if (fetched.status == 408) return JsonError(reply, eHTTPStatusCode::REQUEST_TIMEOUT, "The UGC server is making it; try again in a moment");
		if (fetched.status == 404) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "The UGC server has no such file");
		if (fetched.status == 0) return JsonError(reply, eHTTPStatusCode::BAD_GATEWAY, "The UGC server doesn't answer at " + url + " (" + fetched.error + ")");
		JsonError(reply, eHTTPStatusCode::BAD_GATEWAY, "The UGC server answered " + std::to_string(fetched.status));
	}

	const std::set<std::string> FILES = { "icon.png", "model.nif", "model.noao.nif", "stats.json",
		"previous.icon.png", "previous.model.nif", "previous.model.noao.nif", "previous.stats.json" };

	std::optional<std::string> KindOf(const std::string& text) {
		if (text == "model" || text == "modular") return text;
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
			"error, bakeAo, modules}], ugcPublicUrl, canManage}. Query: ?kind=model|modular&state=pending|done|failed&search=(id or owner name)&page=&size=",
			[](HTTPReply& reply, const HTTPContext& context) {
				const bool modular = QueryValue(context.queryString, "kind") == "modular";
				const auto state = ParseState(QueryValue(context.queryString, "state"));
				const auto page = GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "page")).value_or(0);
				const auto search = QueryValue(context.queryString, "search").substr(0, 64);
				const auto size = std::clamp(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "size")).value_or(PAGE_SIZE), 1u, 200u);
				const auto list = modular ? Database::Get()->GetModularBuildProcessList(state, search, page * size, size + 1)
					: Database::Get()->GetUgcProcessList(state, search, page * size, size + 1);
				nlohmann::json items = nlohmann::json::array();
				for (size_t i = 0; i < list.size() && i < size; i++) {
					const auto& info = list[i];
					items.push_back({ { "id", std::to_string(info.id) }, { "characterId", std::to_string(info.characterId) }, { "characterName", info.characterName },
						{ "state", StateName(info.state) }, { "attempts", info.attempts }, { "processedAt", info.processedAt }, { "error", info.error },
						{ "bakeAo", info.bakeAo }, { "modules", info.details } });
				}
				JsonSuccess(reply, { { "counts", { { "model", Counts(Database::Get()->GetUgcProcessCounts()) }, { "modular", Counts(Database::Get()->GetModularBuildProcessCounts()) } } },
					{ "items", items }, { "more", list.size() > size }, { "ugcPublicUrl", Game::config->GetValue("ugc_public_url") }, { "canManage", Can(context, "ugc_manage") } });
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

		Route(eHTTPMethod::GET, "/api/ugc/server/status", Perm("properties_view"),
			"The UGC server's live status (its /status, fetched by the dashboard from ugc_internal_url): queue, workers, CPU and memory use, "
			"limits and throttling. {success: false, error, url} when it doesn't answer",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto base = InternalUrl();
				Workers::Reply(reply, context, false, [base](HTTPReply& out) {
					const auto fetched = CachedGet(base + "/status", true);
					if (fetched->status != 200) {
						JsonReply(out, eHTTPStatusCode::OK, { { "success", false }, { "url", base }, { "error", fetched->status == 0 ? fetched->error : "answered " + std::to_string(fetched->status) } });
						return;
					}
					out.status = eHTTPStatusCode::OK;
					out.contentType = eContentType::APPLICATION_JSON;
					out.message = R"({"success":true,"status":)" + fetched->body + "}";
					out.headers.push_back("Cache-Control: no-store");
				}, WorkerPool::ePriority::URGENT);
			});

		Route(eHTTPMethod::GET, "/api/ugc/files/:kind/:id/:file", Perm("properties_view"),
			"A file the UGC server made (fetched from ugc_internal_url): icon.png, model.nif, model.noao.nif (before the lighting bake), "
			"stats.json, or previous.<one of those> (the version before it was made again). 408 while it's being made",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto kind = KindOf(std::string(PathSegment(context.path, 3)));
				const auto id = PathId<LWOOBJID>(context.path, 4);
				const auto file = std::string(PathSegment(context.path, 5));
				if (!kind || !id || !FILES.contains(file)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid kind, id or file");
				const auto url = InternalUrl() + "/files/" + *kind + "/" + std::to_string(*id) + "/" + file;
				Workers::Reply(reply, context, false, [url, file](HTTPReply& out) {
					const auto fetched = CachedGet(url, false);
					if (fetched->status != 200) return FetchError(out, *fetched, url);
					out.status = eHTTPStatusCode::OK;
					out.contentType = file.ends_with(".png") ? eContentType::IMAGE_PNG : file.ends_with(".json") ? eContentType::APPLICATION_JSON : eContentType::APPLICATION_OCTET_STREAM;
					out.message = fetched->body;
					out.headers.push_back("Cache-Control: private, no-cache");
				}, file.ends_with(".png") ? WorkerPool::ePriority::URGENT : WorkerPool::ePriority::NORMAL);
			});

		Route(eHTTPMethod::GET, "/api/ugc/mesh/:id", Perm("properties_view"),
			"A player model's generated .nif converted for the 3D view (NifFile::Encode, as the scenery meshes). Query: ?lod=0 (most detailed) "
			"to 3, &version=current|previous, &ao=0 for the mesh before the lighting bake. The header adds triangles and vertices",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = PathId<LWOOBJID>(context.path, 3);
				if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid id");
				const auto lod = std::min(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "lod")).value_or(0), 3u);
				const bool previous = QueryValue(context.queryString, "version") == "previous";
				const bool baked = QueryValue(context.queryString, "ao") != "0";
				const std::string file = std::string(previous ? "previous." : "") + (baked ? "model.nif" : "model.noao.nif");
				const auto url = InternalUrl() + "/files/model/" + std::to_string(*id) + "/" + file;
				Workers::Reply(reply, context, false, [url, lod](HTTPReply& out) {
					const auto fetched = CachedGet(url, false);
					if (fetched->status != 200) return FetchError(out, *fetched, url);
					std::string error;
					const auto model = NifFile::Parse(fetched->body, lod, error);
					if (!model) return JsonError(out, eHTTPStatusCode::UNPROCESSABLE_ENTITY, "The .nif can't be read: " + error);
					out.status = eHTTPStatusCode::OK;
					out.contentType = eContentType::APPLICATION_OCTET_STREAM;
					out.message = NifFile::Encode(*model, std::vector<std::string>(model->meshes.size()));
					out.headers.push_back("Cache-Control: private, no-cache");
				});
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
