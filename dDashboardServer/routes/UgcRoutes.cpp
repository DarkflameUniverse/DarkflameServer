#include "UgcRoutes.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <set>

#include "CDClientDatabase.h"
#include "ServerState.h"
#include "Database.h"
#include "UgcIconParams.h"
#include "NifFile.h"
#include "Scenery.h"
#include "SettingsCatalog.h"
#include "SettingsHistory.h"
#include "UgcAssemblies.h"
#include "UgcFetch.h"
#include "UgcLinks.h"
#include "UgcLookup.h"
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
using namespace UgcFetch;

namespace {
	constexpr uint32_t PAGE_SIZE = 50;

	std::string StateName(IUgc::eProcessState state) { return IUgc::ProcessStateName(state); }

	std::optional<IUgc::eProcessState> ParseState(const std::string& text) { return IUgc::ParseProcessState(text); }


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
	nlohmann::json LoadIconKinds() {
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

	// Client data read once by Preload (main thread), only read after that
	nlohmann::json g_IconKinds = nlohmann::json::array();
	std::map<uint32_t, UgcAssemblies::ModuleInfo> g_Modules;

	// The icon kinds: player models, then each car or rocket build type with its name
	const nlohmann::json& IconKinds() { return g_IconKinds; }

	// Every module in the client's data (ModuleComponent) with its build type and name
	const std::map<uint32_t, UgcAssemblies::ModuleInfo>& Modules() { return g_Modules; }

	// A car or rocket's kind, from its first module's build type
	std::optional<std::string> ModularKind(std::string modules) {
		std::replace(modules.begin(), modules.end(), '-', '+'); // a combination's key too
		const auto lots = UgcModularKey::Lots(modules);
		if (lots.empty()) return std::nullopt;
		const auto it = Modules().find(lots.front());
		if (it == Modules().end() || it->second.buildType < 0) return std::nullopt;
		return UgcIconParams::BuildKind(it->second.buildType);
	}

	// Web thread: each kind with something to edit its preset on: the newest made player model, the most used combination of each build type
	nlohmann::json WithSamples(nlohmann::json kinds) {
		std::map<std::string, std::pair<std::string, uint64_t>> best; // kind -> (combination, builds)
		std::map<std::string, uint64_t> uses;
		for (const auto& [ldf, count] : Database::Get()->GetModularBuildConfigCounts()) uses[UgcModularKey::Normalize(ldf)] += count;
		for (const auto& [key, count] : uses) {
			if (key.empty()) continue;
			std::string modules = key;
			std::replace(modules.begin(), modules.end(), '-', '+');
			const auto kind = ModularKind(modules);
			if (kind && count > best[*kind].second) best[*kind] = { key, count };
		}
		for (auto& entry : kinds) {
			const auto kind = entry.value("kind", std::string());
			if (kind == UgcIconParams::ModelKind()) {
				const auto models = Database::Get()->GetUgcProcessList(IUgc::eProcessState::DONE, "", 0, 1);
				entry["sample"] = models.empty() ? nlohmann::json(nullptr) : nlohmann::json(std::to_string(models.front().id));
			} else if (const auto it = best.find(kind); it != best.end()) {
				entry["sample"] = it->second.first;
				entry["sampleBuilds"] = it->second.second;
			} else {
				entry["sample"] = nullptr;
			}
		}
		return kinds;
	}

	// Main thread (Preload): every module in the client's data (ModuleComponent) with its build type and name
	std::map<uint32_t, UgcAssemblies::ModuleInfo> LoadModules() {
		std::map<uint32_t, UgcAssemblies::ModuleInfo> modules;
		auto result = CDClientDatabase::ExecuteQuery("SELECT cr.id AS lot, m.buildType AS buildType, o.name AS name, o.displayName AS displayName FROM ComponentsRegistry cr "
			"JOIN ModuleComponent m ON m.id = cr.component_id LEFT JOIN Objects o ON o.id = cr.id WHERE cr.component_type = 28;");
		while (!result.eof()) {
			std::string name = result.getStringField("displayName", "");
			if (name.empty()) name = result.getStringField("name", "");
			modules[static_cast<uint32_t>(result.getIntField("lot", 0))] = { result.getIntField("buildType", -1), name };
			result.nextRow();
		}
		return modules;
	}

	// The /ugc search box: "state:" and "kind:" (or "type:") words are filters of their own, the rest is a UGC search
	// (UgcLookup::ParseQuery: "owner:", "account:", "property:", "name:", "lot:"/"module:", "id:", or plain text)
	struct ListSearch {
		IUgcLookup::UgcSearch search;
		std::optional<IUgc::eProcessState> state;
		std::string kind;
		std::string text; // what went to ParseQuery
	};
	ListSearch ParseListSearch(const std::string& input) {
		ListSearch out;
		std::string rest;
		size_t start = 0;
		while (start < input.size()) {
			auto end = input.find(' ', start);
			if (end == std::string::npos) end = input.size();
			const auto word = input.substr(start, end - start);
			const auto lower = UgcAssemblies::Lower(word);
			if (lower.starts_with("state:") && IUgc::ParseProcessState(lower.substr(6))) out.state = IUgc::ParseProcessState(lower.substr(6));
			else if (lower.starts_with("kind:") && lower.size() > 5) out.kind = lower.substr(5);
			else if (lower.starts_with("type:") && lower.size() > 5) out.kind = lower.substr(5);
			else if (!word.empty()) rest += (rest.empty() ? "" : " ") + word;
			start = end + 1;
		}
		out.text = rest;
		out.search = UgcLookup::ParseQuery(rest);
		return out;
	}

	nlohmann::json ModulesJson(const std::vector<uint32_t>& lots) {
		nlohmann::json out = nlohmann::json::array();
		const auto& modules = Modules();
		for (const auto lot : lots) {
			const auto it = modules.find(lot);
			out.push_back({ { "lot", lot }, { "name", it != modules.end() ? it->second.name : std::string() }, { "icon", "/api/icon/" + std::to_string(lot) } });
		}
		return out;
	}

	nlohmann::json EntryJson(const IUgcLookup::UgcEntry& entry) {
		return { { "id", std::to_string(entry.id) }, { "characterId", std::to_string(entry.characterId) }, { "characterName", entry.characterName },
			{ "accountId", entry.accountId }, { "accountName", entry.accountName }, { "state", IUgc::ProcessStateName(entry.state) }, { "attempts", entry.attempts },
			{ "processedAt", entry.processedAt }, { "error", entry.error }, { "bakeAo", entry.bakeAo }, { "processAfter", entry.processAfter },
			{ "detail", entry.detail }, { "bricks", entry.bricks }, { "triangles", entry.triangles }, { "processMs", entry.processMs }, { "processCpuMs", entry.processCpuMs }, { "processMemoryKb", entry.processMemoryKb }, { "modelName", entry.modelName }, { "trianglesBefore", entry.trianglesBefore } };
	}

	// Web thread: the cars and rockets grouped into assemblies (one per combination of modules), from every build.
	// Grouping all builds is the slow part of the Cars and rockets list, so the result is kept for a short while and
	// shared by the requests in that time (it follows new builds and makes within ASSEMBLY_CACHE_TIME).
	constexpr auto ASSEMBLY_CACHE_TIME = std::chrono::seconds(15);
	std::vector<IUgcLookup::UgcEntry> AllBuilds(const IUgcLookup::UgcSearch& search);
	std::shared_ptr<const std::vector<UgcAssemblies::Assembly>> CachedAssemblies() {
		static std::mutex mutex;
		static std::shared_ptr<const std::vector<UgcAssemblies::Assembly>> cached;
		static std::chrono::steady_clock::time_point at;
		std::lock_guard lock(mutex);
		const auto now = std::chrono::steady_clock::now();
		if (!cached || now - at > ASSEMBLY_CACHE_TIME) {
			cached = std::make_shared<const std::vector<UgcAssemblies::Assembly>>(UgcAssemblies::Group(AllBuilds({}), Modules()));
			at = now;
		}
		return cached;
	}

	// Web thread: every car and rocket build (the assemblies are made from them)
	std::vector<IUgcLookup::UgcEntry> AllBuilds(const IUgcLookup::UgcSearch& search = {}) {
		IUgcLookup::UgcListQuery query;
		query.search = search;
		query.limit = 1000000;
		return Database::Get()->ListUgc(IUgcLookup::eUgcKind::MODULAR, query).first;
	}

	// Web thread: the values stored for a target, or null
	nlohmann::json StoredValues(const std::string& target) {
		const auto stored = Database::Get()->GetUgcIconSettings(target);
		if (!stored) return nullptr;
		return nlohmann::json::parse(UgcIconParams::ToJson(UgcIconParams::Parse(*stored)));
	}

	const std::set<std::string> FILES = { "icon.png", "model.nif", "model.noao.nif", "stats.json", "combo.json",
		"previous.icon.png", "previous.model.nif", "previous.model.noao.nif", "previous.stats.json" };

	std::optional<std::string> KindOf(const std::string& text) {
		if (text == "model" || text == "modular") return text;
		return std::nullopt;
	}

	// An id from a request body, as a string or a number; nullopt when it's missing or not one
	std::optional<LWOOBJID> BodyId(const nlohmann::json& body) {
		const auto it = body.find("id");
		if (it == body.end()) return std::nullopt;
		return GeneralUtils::TryParse<LWOOBJID>(it->is_string() ? it->get<std::string>() : it->dump());
	}

	// Sums over what the UGC server has made (IUgc::GetUgcProcessTotals)
	nlohmann::json Totals(const IUgc::ProcessTotals& t) {
		return { { "made", t.made }, { "timed", t.timed }, { "ms", t.milliseconds }, { "cpuMs", t.cpuMilliseconds }, { "maxMs", t.maxMilliseconds },
			{ "averageMs", t.timed ? t.milliseconds / t.timed : 0 }, { "memoryKbAverage", t.memoryKbAverage }, { "memoryKbMax", t.memoryKbMax },
			{ "bricks", t.bricks }, { "triangles", t.triangles }, { "trianglesBefore", t.trianglesBefore }, { "trianglesAfter", t.trianglesAfter } };
	}

	nlohmann::json Counts(const std::vector<std::pair<IUgc::eProcessState, uint64_t>>& counts) {
		nlohmann::json out = nlohmann::json::object();
		for (const auto state : magic_enum::enum_values<IUgc::eProcessState>()) out[IUgc::ProcessStateName(state)] = 0;
		for (const auto& [state, count] : counts) out[StateName(state)] = count;
		return out;
	}
}

namespace UgcRoutes {
	void Preload() {
		g_Modules = LoadModules();
		g_IconKinds = LoadIconKinds();
	}

	std::optional<std::string> Setting(const std::string& name) {
		return UgcSetting(name);
	}

	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/ugc", Perm("properties_view"), "What the UGC server made of players' models",
			[](HTTPReply& reply, const HTTPContext& context) { RenderPage(reply, context, "ugc.jinja2", "ugc"); });

		Route(eHTTPMethod::GET, "/api/ugc", Perm("properties_view"),
			"A page of player models (kind=model) or of car and rocket assemblies (kind=modular: one per combination of modules, however many builds use "
			"it). Query: q= (\"state:\", \"kind:\"/\"type:\" (a build type, e.g. build6), \"owner:\", \"account:\", \"property:\", \"name:\", "
			"\"lot:\"/\"module:\" (a LOT or a module's name), \"id:\", or plain text across names, owners and ids), state=, type=, sort=newest|oldest|owner|name|"
			"bricks|triangles|slowest|made|cpu|memory|savings (models) or newest|oldest|references|name (assemblies), reverse=1 (the sort's other direction), page= (from 0), size= (1-200). {items, total, page, size, counts, "
			"kinds, ugcPublicUrl, canManage}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const bool modular = QueryValue(context.queryString, "kind") == "modular";
				auto parsed = ParseListSearch(QueryValue(context.queryString, "q").substr(0, 100));
				if (const auto state = IUgc::ParseProcessState(QueryValue(context.queryString, "state"))) parsed.state = state;
				if (const auto type = QueryValue(context.queryString, "type"); !type.empty()) parsed.kind = type;
				const auto page = GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "page")).value_or(0);
				const auto size = std::clamp(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "size")).value_or(PAGE_SIZE), 1u, 200u);
				const auto sortText = QueryValue(context.queryString, "sort");
				const bool reverse = QueryValue(context.queryString, "reverse") == "1";
				nlohmann::json items = nlohmann::json::array();
				uint64_t total = 0;
				const auto kinds = IconKinds();
				if (!modular) {
					IUgcLookup::UgcListQuery query;
					query.search = parsed.search;
					query.state = parsed.state;
					static const std::map<std::string, IUgcLookup::eSort> SORTS = { { "newest", IUgcLookup::eSort::NEWEST }, { "oldest", IUgcLookup::eSort::OLDEST },
						{ "owner", IUgcLookup::eSort::OWNER }, { "name", IUgcLookup::eSort::NAME }, { "bricks", IUgcLookup::eSort::BRICKS }, { "triangles", IUgcLookup::eSort::TRIANGLES }, { "slowest", IUgcLookup::eSort::SLOWEST }, { "made", IUgcLookup::eSort::MADE }, { "cpu", IUgcLookup::eSort::CPU }, { "memory", IUgcLookup::eSort::MEMORY }, { "savings", IUgcLookup::eSort::SAVINGS } };
					if (const auto it = SORTS.find(sortText); it != SORTS.end()) query.sort = it->second;
					query.reverse = reverse;
					query.offset = page * size;
					query.limit = size;
					const auto [entries, count] = Database::Get()->ListUgc(IUgcLookup::eUgcKind::MODEL, query);
					total = count;
					for (const auto& entry : entries) items.push_back(EntryJson(entry));
				} else {
					UgcAssemblies::Filter filter;
					filter.state = parsed.state;
					if (!parsed.kind.empty()) {
						for (const auto& k : kinds) {
							if (k.contains("buildType") && (k["kind"] == parsed.kind || UgcAssemblies::Lower(k["label"].get<std::string>()).find(parsed.kind) != std::string::npos)) {
								filter.buildType = k["buildType"].get<int32_t>();
								break;
							}
						}
						if (!filter.buildType) filter.buildType = -2; // no such type: nothing
					}
					const auto& modules = Modules();
					if (!parsed.search.text.empty() || parsed.search.number) {
						using eField = IUgcLookup::UgcSearch::eField;
						const auto field = parsed.search.field;
						if (field != eField::LOT) {
							std::set<LWOOBJID> matched;
							for (const auto& build : AllBuilds(parsed.search)) matched.insert(build.id);
							filter.builds = matched;
						}
						if (field == eField::ANY || field == eField::LOT) {
							filter.moduleText = parsed.search.text;
							if (parsed.search.number) filter.moduleLot = static_cast<uint32_t>(*parsed.search.number);
						}
					}
					auto assemblies = *CachedAssemblies();
					std::erase_if(assemblies, [&](const auto& a) { return !UgcAssemblies::Matches(a, filter, modules); });
					UgcAssemblies::Sort(assemblies, UgcAssemblies::ParseSort(sortText).value_or(UgcAssemblies::eSort::NEWEST), modules, reverse);
					total = assemblies.size();
					for (size_t i = static_cast<size_t>(page) * size; i < assemblies.size() && i < static_cast<size_t>(page + 1) * size; i++) {
						const auto& a = assemblies[i];
						std::string label, kind;
						for (const auto& k : kinds) {
							if (k.contains("buildType") && k["buildType"] == a.buildType) {
								label = k["label"];
								kind = k["kind"];
							}
						}
						std::string ldf = a.key;
						std::replace(ldf.begin(), ldf.end(), '-', '+');
						items.push_back({ { "id", a.key }, { "key", a.key }, { "modules", ldf }, { "moduleList", ModulesJson(a.lots) }, { "buildType", a.buildType },
							{ "kind", kind }, { "kindLabel", label }, { "state", IUgc::ProcessStateName(a.state) }, { "error", a.error }, { "uses", a.builds.size() },
							{ "owners", a.owners.size() }, { "iconBuild", std::to_string(a.iconBuild) }, { "newestBuild", std::to_string(a.builds.front()) },
							{ "storageId", std::to_string(UgcModularKey::StorageId(a.key)) } });
					}
				}
				JsonSuccess(reply, { { "counts", { { "model", Counts(Database::Get()->GetUgcProcessCounts()) }, { "modular", Counts(Database::Get()->GetModularBuildProcessCounts()) } } },
					{ "totals", { { "model", Totals(Database::Get()->GetUgcProcessTotals(false)) }, { "modular", Totals(Database::Get()->GetUgcProcessTotals(true)) } } },
					{ "items", items }, { "total", total }, { "page", page }, { "size", size }, { "more", static_cast<uint64_t>(page + 1) * size < total }, { "kinds", kinds },
					{ "ugcPublicUrl", Game::config->GetValue("ugc_public_url") }, { "canManage", Can(context, "ugc_manage") } });
			});

		Route(eHTTPMethod::GET, "/api/ugc/assembly/builds", Perm("properties_view"),
			"The builds (ugc_modular_build rows) that use a combination of modules: {items: [{id, characterId, characterName, accountId, accountName, state, "
			"attempts, processedAt, error, where: [{type: property|mail|inventory, ...}]}], total, page, size}. Query: modules= (the combination), q= (owner, "
			"account, property or id, as the list's search), sort=id|owner|account|state (default id, the newest first; owner and account A to Z, state "
			"by state then newest), reverse=1 (the other direction), page=, size=, build= (a build to show: the page holding it is given)",
			[](HTTPReply& reply, const HTTPContext& context) {
				auto asked = QueryValue(context.queryString, "modules");
				std::replace(asked.begin(), asked.end(), '-', '+');
				const auto key = UgcModularKey::Normalize(asked);
				if (key.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "No modules");
				const auto parsed = ParseListSearch(QueryValue(context.queryString, "q").substr(0, 100));
				auto builds = AllBuilds(parsed.search);
				std::erase_if(builds, [&](const auto& b) { return UgcModularKey::Normalize(b.detail) != key || (parsed.state && b.state != *parsed.state); });
				const auto sort = QueryValue(context.queryString, "sort");
				const auto lowerName = [](const std::string& name) { return UgcAssemblies::Lower(name); };
				std::stable_sort(builds.begin(), builds.end(), [&](const auto& a, const auto& b) {
					if (sort == "owner" && lowerName(a.characterName) != lowerName(b.characterName)) return lowerName(a.characterName) < lowerName(b.characterName);
					if (sort == "account" && lowerName(a.accountName) != lowerName(b.accountName)) return lowerName(a.accountName) < lowerName(b.accountName);
					if (sort == "state" && a.state != b.state) return a.state < b.state;
					return a.id > b.id;
				});
				if (QueryValue(context.queryString, "reverse") == "1") std::reverse(builds.begin(), builds.end());
				const auto size = std::clamp(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "size")).value_or(25), 1u, 200u);
				auto page = GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "page")).value_or(0);
				if (const auto wanted = GeneralUtils::TryParse<LWOOBJID>(QueryValue(context.queryString, "build"))) {
					const auto it = std::find_if(builds.begin(), builds.end(), [&](const auto& b) { return b.id == *wanted; });
					if (it != builds.end()) page = static_cast<uint32_t>((it - builds.begin()) / size);
				}
				std::vector<IUgcLookup::UgcEntry> shown;
				for (size_t i = static_cast<size_t>(page) * size; i < builds.size() && i < static_cast<size_t>(page + 1) * size; i++) shown.push_back(builds[i]);
				const auto where = UgcLinks::Whereabouts(shown);
				nlohmann::json items = nlohmann::json::array();
				for (const auto& build : shown) {
					auto item = EntryJson(build);
					const auto found = where.find(build.id);
					item["where"] = found == where.end() ? nlohmann::json::array() : found->second;
					items.push_back(std::move(item));
				}
				JsonSuccess(reply, { { "items", items }, { "total", builds.size() }, { "page", page }, { "size", size }, { "key", key } });
			});

		Route(eHTTPMethod::GET, "/api/ugc/assembly/of/:id", Perm("properties_view"),
			"The assembly (combination of modules) a car or rocket build uses: {key, modules}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = PathId<LWOOBJID>(context.path, 4);
				if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid id");
				const auto info = Database::Get()->GetModularBuildProcessInfo(*id);
				if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such build");
				const auto key = UgcModularKey::Normalize(info->details);
				std::string ldf = key;
				std::replace(ldf.begin(), ldf.end(), '-', '+');
				JsonSuccess(reply, { { "key", key }, { "modules", ldf }, { "moduleList", ModulesJson(UgcModularKey::Lots(ldf)) } });
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
					const auto fetched = CachedGet(base + "/status", eCache::STATUS);
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

		Route(eHTTPMethod::GET, "/api/diagnostics/ugc", Perm("health_view"),
			"The UGC server for the Diagnostics page: {enabled, online, status (its /status: queue, workers, CPU, memory, limits, "
			"throttling, recent makes) or error, counts: {model, modular} per state}",
			[](HTTPReply& reply, const HTTPContext& context) {
				bool enabled = false, online = false;
				{
					std::lock_guard lock(ServerState::g_StatusMutex);
					enabled = ServerState::g_UgcEnabled;
					online = ServerState::g_UgcStatus.online;
				}
				const auto base = InternalUrl();
				Workers::Reply(reply, context, false, [base, enabled, online](HTTPReply& out) {
					nlohmann::json body = { { "success", true }, { "enabled", enabled }, { "online", online },
						{ "counts", { { "model", Counts(Database::Get()->GetUgcProcessCounts()) }, { "modular", Counts(Database::Get()->GetModularBuildProcessCounts()) } } } };
					if (enabled) {
						const auto fetched = CachedGet(base + "/status", eCache::STATUS);
						if (fetched->status == 200) body["status"] = nlohmann::json::parse(fetched->body, nullptr, false);
						else body["error"] = fetched->status == 0 ? fetched->error : "answered " + std::to_string(fetched->status);
					}
					JsonReply(out, eHTTPStatusCode::OK, body);
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
					const auto fetched = CachedGet(url);
					if (fetched->status != 200) return ReplyError(out, *fetched, url);
					out.status = eHTTPStatusCode::OK;
					out.contentType = file.ends_with(".png") ? eContentType::IMAGE_PNG : file.ends_with(".json") ? eContentType::APPLICATION_JSON : eContentType::APPLICATION_OCTET_STREAM;
					out.message = fetched->body;
					out.headers.push_back("Cache-Control: private, no-cache");
				}, file.ends_with(".png") ? WorkerPool::ePriority::URGENT : WorkerPool::ePriority::NORMAL);
			});

		Route(eHTTPMethod::GET, "/api/ugc/mesh/:id", Perm("properties_view"),
			"A player model's generated .nif converted for the 3D view (NifFile::Encode, as the scenery meshes, with each mesh's shader look; the glitter "
			"groups' meshes have the GLITTER look, their UVs and uvScroll, and the texture name \"glitter\"). Query: ?lod=0 (most detailed) "
			"to 3, &version=current|previous, &ao=0 for the mesh before the lighting bake. The header adds triangles and vertices",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = PathId<LWOOBJID>(context.path, 3);
				if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid id");
				const auto lod = std::min(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "lod")).value_or(0), 3u);
				const bool previous = QueryValue(context.queryString, "version") == "previous";
				const bool baked = QueryValue(context.queryString, "ao") != "0";
				const std::string file = std::string(previous ? "previous." : "") + (baked ? "model.nif" : "model.noao.nif");
				const auto url = InternalUrl() + "/files/model/" + std::to_string(*id) + "/" + file;
				// The glitter groups' tag: the setting's, and the client's LEGO-AnimUV (21) for models made with another
				const auto glitterTag = GeneralUtils::TryParse<int32_t>(UgcSetting("shader_glitter").value_or("21")).value_or(21);
				Workers::Reply(reply, context, false, [url, lod, glitterTag](HTTPReply& out) {
					const auto fetched = CachedGet(url);
					if (fetched->status != 200) return ReplyError(out, *fetched, url);
					std::string error;
					const auto model = NifFile::Parse(fetched->body, lod, error);
					if (!model) return JsonError(out, eHTTPStatusCode::UNPROCESSABLE_ENTITY, "The .nif can't be read: " + error);
					auto looks = Scenery::MultishaderLooks(*model);
					std::vector<std::string> textures(model->meshes.size());
					for (size_t i = 0; i < model->meshes.size(); i++) {
						const auto& material = model->meshes[i].material;
						if (material.embeddedTexture < 0 || (material.shaderTag != glitterTag && material.shaderTag != 21)) continue;
						looks[i] |= NifFile::GLITTER;
						textures[i] = "glitter";
					}
					out.status = eHTTPStatusCode::OK;
					out.contentType = eContentType::APPLICATION_OCTET_STREAM;
					out.message = NifFile::Encode(*model, textures, {}, looks);
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
				const auto id = BodyId(*body);
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
			"What an icon's framing and light can be set to: {params: [{key, group, setting, label, unit, min, max, step, default, description}], kinds: [{kind, "
			"label, buildType, sample}]} (player models, and each car or rocket build type in the client's data; sample: a model id or the most used module "
			"combination of that kind, to edit its preset on)",
			[](HTTPReply& reply, const HTTPContext&) {
				nlohmann::json params = nlohmann::json::array();
				for (const auto& param : UgcIconParams::List()) {
					params.push_back({ { "key", param.key }, { "group", param.group }, { "setting", param.setting }, { "label", param.label }, { "unit", param.unit }, { "min", param.min },
						{ "max", param.max }, { "step", param.step }, { "default", param.defaultValue }, { "description", param.description } });
				}
				JsonSuccess(reply, { { "params", params }, { "kinds", WithSamples(IconKinds()) } });
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
				JsonSuccess(reply, { { "kind", kind }, { "target", target }, { "settings", settings }, { "preset", kind == UgcIconParams::ModelKind() ? nlohmann::json() : StoredValues(UgcIconParams::KindTarget(kind)) },
					{ "presets", kind != UgcIconParams::ModelKind() },
					{ "own", target.empty() ? nlohmann::json(nullptr) : StoredValues(target) } });
			});

		Route(eHTTPMethod::GET, "/api/ugc/assembly", Perm("properties_view"),
			"A car or rocket's modules put together as the icon renderer does (turned by its build type's AdditionalModelRotation), converted for the 3D "
			"view (NifFile::Encode). Made by the UGC server on a worker and cached per combination. Query: ?modules=4713-4714-4715 (or an ldf_config)",
			[](HTTPReply& reply, const HTTPContext& context) {
				auto asked = QueryValue(context.queryString, "modules");
				std::replace(asked.begin(), asked.end(), '-', '+');
				const auto key = UgcModularKey::Normalize(asked);
				if (key.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "No modules");
				const auto url = InternalUrl() + "/admin/assembly";
				Workers::Reply(reply, context, false, [url, key, admin = AdminKey()](HTTPReply& out) {
					std::string modules = key;
					std::replace(modules.begin(), modules.end(), '-', '+');
					const auto fetched = AdminPost(url, admin, nlohmann::json{ { "modules", modules } }.dump());
					if (fetched->status == 0) return JsonError(out, eHTTPStatusCode::BAD_GATEWAY, "The UGC server doesn't answer at " + url + " (" + fetched->error + ")");
					if (fetched->status != 200) {
						const auto error = nlohmann::json::parse(fetched->body, nullptr, false);
						return JsonError(out, eHTTPStatusCode::UNPROCESSABLE_ENTITY, error.is_object() ? error.value("error", fetched->body) : fetched->body);
					}
					std::string error;
					const auto model = NifFile::Parse(fetched->body, 0, error);
					if (!model) return JsonError(out, eHTTPStatusCode::UNPROCESSABLE_ENTITY, "The .nif can't be read: " + error);
					out.status = eHTTPStatusCode::OK;
					out.contentType = eContentType::APPLICATION_OCTET_STREAM;
					out.message = NifFile::Encode(*model, std::vector<std::string>(model->meshes.size()));
					out.headers.push_back("Cache-Control: private, max-age=60");
				});
			});

		Route(eHTTPMethod::POST, "/api/ugc/icon/preview", Perm("properties_view"),
			"An icon drawn by the UGC server with the given values, not stored (PNG). Body: {kind: model, id, values} or {kind: modular, modules, values}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const bool model = body->value("kind", std::string()) == "model";
				ProxyAdmin(reply, context, "/admin/preview", { { "kind", model ? "model" : "modular" }, { "id", std::to_string(BodyId(*body).value_or(0)) },
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
					if (kind == UgcIconParams::ModelKind()) {
						return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Player models have no shared icon preset: each is fitted to its icon. Change one model's icon instead.");
					}
					target = UgcIconParams::KindTarget(kind);
					what = "the " + kind + " icon preset";
				} else if (body->value("kind", std::string()) == "modular") {
					const auto key = UgcModularKey::Normalize(body->value("modules", std::string()));
					if (key.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "No modules");
					target = UgcIconParams::CombinationTarget(key);
					what = "the icon of the module combination " + key;
				} else {
					const auto id = BodyId(*body);
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
			"Have the UGC server make items again. Body: {kind: model|modular, id} for one, {kind: model, property} for every model placed on a property, {kind, failedOnly: true} for the failed ones, {kind} for all",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const bool modular = body->value("kind", "model") == "modular";
				if (!modular && body->contains("property")) {
					const auto property = GeneralUtils::TryParse<LWOOBJID>((*body)["property"].is_string() ? (*body)["property"].get<std::string>() : (*body)["property"].dump());
					if (!property) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid property");
					const auto changed = Database::Get()->ResetPropertyUgcModelProcessing(*property);
					Audit(context, "ugc_reprocess", "Queued " + std::to_string(changed) + " model(s) on property " + std::to_string(*property) + " to be made again");
					BroadcastTableChanged("ugc");
					return JsonSuccess(reply, { { "message", std::to_string(changed) + " model" + (changed == 1 ? "" : "s") + " will be made again" } });
				}
				std::optional<LWOOBJID> id;
				if (body->contains("id")) {
					id = BodyId(*body);
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
