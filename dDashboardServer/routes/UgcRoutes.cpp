#include "UgcRoutes.h"

#include <memory>
#include <mutex>
#include <set>

#include <curl/curl.h>

#include "CDClientDatabase.h"
#include "Database.h"
#include "UgcIconParams.h"
#include "NifFile.h"
#include "TtlCache.h"
#include "SettingsCatalog.h"
#include "SettingsHistory.h"
#include "UgcKeys.h"
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

	std::string StateName(IUgc::eProcessState state) { return IUgc::ProcessStateName(state); }

	std::optional<IUgc::eProcessState> ParseState(const std::string& text) { return IUgc::ParseProcessState(text); }


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

	// Any thread: a POST of JSON to one of the UGC server's /admin routes, with the key it checks (the master password)
	std::shared_ptr<const Fetched> AdminPost(const std::string& url, const std::string& key, const std::string& body) {
		auto out = std::make_shared<Fetched>();
		CURL* curl = curl_easy_init();
		if (!curl) {
			out->error = "could not start a request";
			return out;
		}
		curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
		headers = curl_slist_append(headers, ("X-Ugc-Admin-Key: " + key).c_str());
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Collect);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out->body);
		const auto code = curl_easy_perform(curl);
		if (code == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out->status);
		else out->error = curl_easy_strerror(code);
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
		return out;
	}

	// Web thread: the key the UGC server's /admin routes want
	std::string AdminKey() {
		const auto master = Database::Get()->GetMasterInfo();
		return master ? master->password : std::string();
	}

	// Sends an admin request from a worker and answers with the UGC server's JSON (or why it couldn't)
	void ProxyAdmin(HTTPReply& reply, const HTTPContext& context, const std::string& route, const nlohmann::json& body) {
		const auto url = InternalUrl() + route;
		Workers::Reply(reply, context, false, [url, key = AdminKey(), text = body.dump()](HTTPReply& out) {
			const auto fetched = AdminPost(url, key, text);
			if (fetched->status == 0) return JsonError(out, eHTTPStatusCode::BAD_GATEWAY, "The UGC server doesn't answer at " + url + " (" + fetched->error + ")");
			out.status = static_cast<eHTTPStatusCode>(fetched->status);
			const bool png = fetched->body.starts_with("\x89PNG");
			out.contentType = png ? eContentType::IMAGE_PNG : eContentType::APPLICATION_JSON;
			out.message = fetched->body;
			if (!png && fetched->status != 200 && !fetched->body.starts_with("{")) {
				JsonError(out, static_cast<eHTTPStatusCode>(fetched->status), fetched->body.empty() ? "The UGC server answered " + std::to_string(fetched->status) : fetched->body);
			}
			out.headers.push_back("Cache-Control: no-store");
		}, WorkerPool::ePriority::URGENT);
	}

	// Web thread: a ugcconfig.ini setting's value as the UGC server sees it (dashboard value, file value, default)
	std::optional<std::string> UgcSetting(const std::string& name) {
		if (const auto current = SettingsHistory::Current("ugcconfig.ini", name)) {
			if (current->webValue && (current->webWins || !current->fileValue)) return current->webValue;
			if (current->fileValue) return current->fileValue;
		}
		if (const auto* info = SettingsCatalog::Find("ugcconfig.ini", name)) return info->defaultValue;
		return std::nullopt;
	}

	// Web thread: the icon kinds: player models, and each car or rocket build type in the client's data
	// (ModularBuildComponent), named after the object its Assembly LOT is
	nlohmann::json IconKinds() {
		nlohmann::json kinds = nlohmann::json::array({ { { "kind", UgcIconParams::ModelKind() }, { "label", "Player models" } } });
		auto result = CDClientDatabase::ExecuteQuery("SELECT buildType, xml FROM ModularBuildComponent GROUP BY buildType ORDER BY buildType;");
		while (!result.eof()) {
			const auto buildType = result.getIntField("buildType", 0);
			std::string label = "Build type " + std::to_string(buildType);
			const std::string xml = result.getStringField("xml", "");
			if (const auto at = xml.find("LOT=\""); at != std::string::npos) {
				const auto lot = GeneralUtils::TryParse<int32_t>(xml.substr(at + 5, xml.find('"', at + 5) - at - 5));
				if (lot) {
					auto name = CDClientDatabase::CreatePreppedStmt("SELECT displayName FROM Objects WHERE id = ? LIMIT 1;");
					name.bind(1, *lot);
					auto row = name.execQuery();
					if (!row.eof() && row.getStringField("displayName", "")[0] != '\0') label = row.getStringField("displayName", "");
				}
			}
			kinds.push_back({ { "kind", UgcIconParams::BuildKind(buildType) }, { "label", label }, { "buildType", buildType } });
			result.nextRow();
		}
		return kinds;
	}

	// Web thread: a car or rocket's kind, from its first module's build type (ModuleComponent)
	std::optional<std::string> ModularKind(const std::string& modules) {
		const auto lots = UgcModularKey::Lots(modules);
		if (lots.empty()) return std::nullopt;
		auto query = CDClientDatabase::CreatePreppedStmt("SELECT m.buildType FROM ComponentsRegistry cr JOIN ModuleComponent m ON m.id = cr.component_id "
			"WHERE cr.id = ? AND cr.component_type = 28 LIMIT 1;");
		query.bind(1, static_cast<int32_t>(lots.front()));
		auto row = query.execQuery();
		if (row.eof()) return std::nullopt;
		return UgcIconParams::BuildKind(row.getIntField("buildType", 0));
	}

	// Web thread: the values stored for a target, or null
	nlohmann::json StoredValues(const std::string& target) {
		const auto stored = Database::Get()->GetUgcIconSettings(target);
		if (!stored) return nullptr;
		return nlohmann::json::parse(UgcIconParams::ToJson(UgcIconParams::Parse(*stored)));
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

	const std::set<std::string> FILES = { "icon.png", "model.nif", "model.noao.nif", "stats.json", "combo.json",
		"previous.icon.png", "previous.model.nif", "previous.model.noao.nif", "previous.stats.json" };

	std::optional<std::string> KindOf(const std::string& text) {
		if (text == "model" || text == "modular") return text;
		return std::nullopt;
	}

	nlohmann::json Counts(const std::vector<std::pair<IUgc::eProcessState, uint64_t>>& counts) {
		nlohmann::json out = nlohmann::json::object();
		for (const auto state : magic_enum::enum_values<IUgc::eProcessState>()) out[IUgc::ProcessStateName(state)] = 0;
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
				// How many builds share each combination of modules (they share one icon)
				std::map<std::string, uint64_t> reuse;
				if (modular) {
					for (const auto& [ldf, count] : Database::Get()->GetModularBuildConfigCounts()) reuse[UgcModularKey::Normalize(ldf)] += count;
				}
				nlohmann::json items = nlohmann::json::array();
				for (size_t i = 0; i < list.size() && i < size; i++) {
					const auto& info = list[i];
					items.push_back({ { "id", std::to_string(info.id) }, { "characterId", std::to_string(info.characterId) }, { "characterName", info.characterName },
						{ "state", StateName(info.state) }, { "attempts", info.attempts }, { "processedAt", info.processedAt }, { "error", info.error },
						{ "bakeAo", info.bakeAo }, { "modules", info.details }, { "processAfter", info.processAfter } });
					if (modular) {
						const auto key = UgcModularKey::Normalize(info.details);
						items.back()["combination"] = key;
						items.back()["sharedBy"] = reuse[key];
					}
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

		Route(eHTTPMethod::POST, "/api/ugc/cache/delete", Perm("ugc_manage"),
			"Delete one item's generated files on the UGC server. Body: {kind: model|modular, id, after: on_demand|now|gone} (made again when asked "
			"for, queued now, or left deleted). A car or rocket's files are shared by every build of the same modules",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto kind = body->value("kind", std::string("model")) == "modular" ? std::string("modular") : std::string("model");
				const auto id = GeneralUtils::TryParse<LWOOBJID>((*body)["id"].is_string() ? (*body)["id"].get<std::string>() : (*body)["id"].dump());
				if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid id");
				const auto after = body->value("after", std::string("now"));
				Audit(context, "ugc_delete_files", "Deleted the UGC files of " + kind + " " + std::to_string(*id) + " (" + after + ")");
				ProxyAdmin(reply, context, "/admin/delete", { { "kind", kind }, { "ids", { std::to_string(*id) } }, { "after", after } });
			});

		Route(eHTTPMethod::POST, "/api/ugc/cache/purge", Perm("ugc_manage"),
			"Delete generated files by filter. Body: {kind, state?, owner? (name), olderThanDays?, unusedDays?, all?, confirm (\"PURGE ALL\" with all and no "
			"other filter), after: on_demand|now|gone}. State and owner pick the rows here; the ages are the files' own",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const bool modular = body->value("kind", std::string("model")) == "modular";
				const auto state = ParseState(body->value("state", std::string()));
				const auto owner = body->value("owner", std::string()).substr(0, 64);
				const auto older = body->value("olderThanDays", int64_t{ 0 });
				const auto unused = body->value("unusedDays", int64_t{ 0 });
				nlohmann::json request = { { "kind", modular ? "modular" : "model" }, { "olderThanDays", older }, { "unusedDays", unused },
					{ "after", body->value("after", std::string("on_demand")) } };
				std::string what = std::string(modular ? "cars and rockets" : "models");
				if (state || !owner.empty()) {
					// The rows the filter picks, from the database
					nlohmann::json ids = nlohmann::json::array();
					for (uint32_t page = 0;; page++) {
						const auto list = modular ? Database::Get()->GetModularBuildProcessList(state, owner, page * 1000, 1000) : Database::Get()->GetUgcProcessList(state, owner, page * 1000, 1000);
						for (const auto& info : list) ids.push_back(std::to_string(info.id));
						if (list.size() < 1000 || ids.size() >= 100000) break;
					}
					if (ids.empty()) return JsonSuccess(reply, { { "deleted", 0 }, { "bytes", 0 }, { "notes", { "Nothing matches the filter." } } });
					request["ids"] = ids;
					what += (state ? " " + body->value("state", std::string()) : "") + (owner.empty() ? "" : " of " + owner);
				} else {
					if (body->value("all", false) && older == 0 && unused == 0 && body->value("confirm", std::string()) != "PURGE ALL") {
						return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Type PURGE ALL to delete every generated file");
					}
					if (!body->value("all", false) && older == 0 && unused == 0) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Pick a filter, or all");
					request["all"] = true;
				}
				if (older > 0) what += ", made more than " + std::to_string(older) + " days ago";
				if (unused > 0) what += ", not asked for in " + std::to_string(unused) + " days";
				Audit(context, "ugc_purge", "Purged the UGC files of " + what + " (" + request["after"].get<std::string>() + ")");
				ProxyAdmin(reply, context, "/admin/delete", request);
			});

		Route(eHTTPMethod::GET, "/api/ugc/icon/params", Perm("properties_view"),
			"What an icon's framing and light can be set to: {params: [{key, setting, label, unit, min, max, step, default, description}], kinds: [{kind, label}]} "
			"(player models, and each car or rocket build type in the client's data)",
			[](HTTPReply& reply, const HTTPContext&) {
				nlohmann::json params = nlohmann::json::array();
				for (const auto& param : UgcIconParams::List()) {
					params.push_back({ { "key", param.key }, { "setting", param.setting }, { "label", param.label }, { "unit", param.unit }, { "min", param.min },
						{ "max", param.max }, { "step", param.step }, { "default", param.defaultValue }, { "description", param.description } });
				}
				JsonSuccess(reply, { { "params", params }, { "kinds", IconKinds() } });
			});

		Route(eHTTPMethod::GET, "/api/ugc/icon/settings", Perm("properties_view"),
			"An item's icon values in their layers: {kind, target, settings (icon_* as the UGC server reads them), preset (the kind's, or null), own (the "
			"item's or combination's, or null)}. Query: ?kind=model&id= or ?kind=modular&modules=, or ?preset=<kind> for a kind alone",
			[](HTTPReply& reply, const HTTPContext& context) {
				std::string kind, target;
				if (const auto preset = QueryValue(context.queryString, "preset"); !preset.empty()) {
					kind = preset;
				} else if (QueryValue(context.queryString, "kind") == "modular") {
					const auto modules = QueryValue(context.queryString, "modules");
					const auto found = ModularKind(modules);
					if (!found) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Those aren't modules");
					kind = *found;
					target = UgcIconParams::CombinationTarget(UgcModularKey::Normalize(modules));
				} else {
					const auto id = GeneralUtils::TryParse<LWOOBJID>(QueryValue(context.queryString, "id"));
					if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid id");
					kind = UgcIconParams::ModelKind();
					target = UgcIconParams::ModelTarget(*id);
				}
				nlohmann::json settings = nlohmann::json::object();
				for (const auto& param : UgcIconParams::List()) {
					const auto value = UgcSetting(param.setting);
					settings[param.key] = std::clamp(value ? GeneralUtils::TryParse<float>(*value).value_or(param.defaultValue) : param.defaultValue, param.min, param.max);
				}
				JsonSuccess(reply, { { "kind", kind }, { "target", target }, { "settings", settings }, { "preset", StoredValues(UgcIconParams::KindTarget(kind)) },
					{ "own", target.empty() ? nlohmann::json(nullptr) : StoredValues(target) } });
			});

		Route(eHTTPMethod::POST, "/api/ugc/icon/preview", Perm("ugc_manage"),
			"An icon drawn by the UGC server with the given values, not stored (PNG). Body: {kind: model, id, values} or {kind: modular, modules, values}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const bool model = body->value("kind", std::string()) == "model";
				ProxyAdmin(reply, context, "/admin/preview", { { "kind", model ? "model" : "modular" }, { "id", (*body)["id"].is_string() ? (*body)["id"].get<std::string>() : std::string("0") },
					{ "modules", body->value("modules", std::string()) }, { "values", nlohmann::json::parse(UgcIconParams::ToJson(UgcIconParams::Parse(body->value("values", nlohmann::json::object()).dump()))) } });
			});

		Route(eHTTPMethod::POST, "/api/ugc/icon/save", Perm("ugc_manage"),
			"Save icon values as a kind's preset ({scope: kind, kind, values}) or as one item's own ({scope: item, kind: model, id, values} or {scope: item, "
			"kind: modular, modules, values}); values null removes it. The icons already made keep theirs until they are drawn again",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				std::string target, what;
				if (body->value("scope", std::string()) == "kind") {
					const auto kind = body->value("kind", std::string());
					bool known = false;
					for (const auto& entry : IconKinds()) known = known || entry["kind"] == kind;
					if (!known) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Unknown kind");
					target = UgcIconParams::KindTarget(kind);
					what = "the " + kind + " icon preset";
				} else if (body->value("kind", std::string()) == "modular") {
					const auto key = UgcModularKey::Normalize(body->value("modules", std::string()));
					if (key.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "No modules");
					target = UgcIconParams::CombinationTarget(key);
					what = "the icon of the module combination " + key;
				} else {
					const auto id = GeneralUtils::TryParse<LWOOBJID>((*body)["id"].is_string() ? (*body)["id"].get<std::string>() : (*body)["id"].dump());
					if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid id");
					target = UgcIconParams::ModelTarget(*id);
					what = "the icon of model " + std::to_string(*id);
				}
				const auto values = body->value("values", nlohmann::json());
				if (values.is_null()) {
					Database::Get()->DeleteUgcIconSettings(target);
					Audit(context, "ugc_icon_settings", "Removed the settings of " + what);
					return JsonSuccess(reply, { { "message", "Removed; draw the icons again to see it" } });
				}
				const auto clean = UgcIconParams::ToJson(UgcIconParams::Parse(values.dump()));
				Database::Get()->SetUgcIconSettings(target, clean);
				Audit(context, "ugc_icon_settings", "Set " + what + " to " + clean);
				JsonSuccess(reply, { { "message", "Saved; draw the icons again to see it in game" } });
			});

		Route(eHTTPMethod::POST, "/api/ugc/icon/regenerate", Perm("ugc_manage"),
			"Have the UGC server draw every stored icon of a kind again (only icons: player models' from their stored .nif). Body: {kind}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				const auto kind = body ? body->value("kind", std::string()) : std::string();
				bool known = false;
				for (const auto& entry : IconKinds()) known = known || entry["kind"] == kind;
				if (!known) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Unknown kind");
				Audit(context, "ugc_regenerate_icons", "Queued every " + kind + " icon to be drawn again");
				ProxyAdmin(reply, context, "/admin/regenerate-icons", { { "kind", kind } });
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
