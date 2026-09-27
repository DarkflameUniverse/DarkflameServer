#include "UgcLinks.h"

#include <map>
#include <memory>
#include <set>

#include "Database.h"
#include "dConfig.h"
#include "eHTTPMethod.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "NifFile.h"
#include "RouteUtils.h"
#include "UgcFetch.h"
#include "UgcLookup.h"
#include "Workers.h"

using namespace RouteUtils;
using namespace UgcLookup;
using namespace UgcFetch;

namespace {
	constexpr uint32_t SEARCH_LIMIT = 50;
	// Creators whose inventories a search looks in for creations that are not placed or mailed
	constexpr size_t MAX_INVENTORIES = 25;


	// ---- Who may see a creation ----

	// The property page's rule (as /api/properties/:id): properties_view, or the owner with own_properties
	bool MayViewProperty(const HTTPContext& context, LWOOBJID propertyId) {
		if (Can(context, "properties_view")) return true;
		const auto info = Database::Get()->GetPropertyInfo(propertyId);
		if (!info || !context.isAuthenticated) return false;
		const auto owner = Database::Get()->GetCharacterInfo(info->ownerId);
		return owner && owner->accountId == context.accountId && Can(context, "own_properties");
	}

	// The creations in a character's saved inventories
	std::vector<ItemLink> CharacterCreations(LWOOBJID characterId) {
		const auto refs = InventoryRefs(Database::Get()->GetCharacterXml(characterId));
		if (refs.empty()) return {};
		std::set<LWOOBJID> models, modular;
		for (const auto& entry : Database::Get()->GetUgcEntries(Candidates(refs))) (entry.kind == eUgcKind::MODEL ? models : modular).insert(entry.id);
		return LinkItems(refs, models, modular);
	}

	/**
	 * Whether the viewer may see what the UGC server made of a creation: with properties_view (as the /ugc page), when
	 * one of their own characters made it, or through a page they may see that shows it: ?property= (placed there) or
	 * ?character= (in that character's inventories).
	 */
	bool MaySee(const HTTPContext& context, eUgcKind kind, LWOOBJID id) {
		if (Can(context, "properties_view")) return true;
		if (!context.isAuthenticated) return false;
		const auto entries = Database::Get()->GetUgcEntries({ id });
		const auto entry = std::find_if(entries.begin(), entries.end(), [kind](const auto& e) { return e.kind == kind; });
		if (entry == entries.end()) return false;
		if (entry->accountId == context.accountId) return true;
		if (const auto property = GeneralUtils::TryParse<LWOOBJID>(QueryValue(context.queryString, "property"))) {
			if (!MayViewProperty(context, *property)) return false;
			for (const auto& placement : Database::Get()->GetUgcPlacements({ id })) {
				if (placement.propertyId == *property) return true;
			}
		}
		if (const auto character = GeneralUtils::TryParse<LWOOBJID>(QueryValue(context.queryString, "character"))) {
			const auto info = Database::Get()->GetCharacterInfo(*character);
			if (!info || !CanViewCharacter(context, info->accountId)) return false;
			for (const auto& link : CharacterCreations(*character)) {
				if (link.kind == kind && link.ugcId == id) return true;
			}
		}
		return false;
	}

	// The icon's address; `via` is the page's ?property= or ?character= that lets its viewer see it
	std::string IconUrl(eUgcKind kind, LWOOBJID id, const std::string& via) {
		return std::string("/api/ugc_links/icon/") + KindName(kind) + "/" + std::to_string(id) + (via.empty() ? "" : "?" + via);
	}

	nlohmann::json EntryJson(const IUgcLookup::UgcEntry& entry, const std::string& via, bool viewer) {
		return {
			{ "kind", KindName(entry.kind) }, { "ugcId", std::to_string(entry.id) }, { "state", StateName(entry.state) }, { "error", entry.error },
			{ "icon", IconUrl(entry.kind, entry.id, via) }, { "link", viewer ? nlohmann::json(ViewerLink(entry.kind, entry.id)) : nlohmann::json() }
		};
	}
}

namespace UgcLinks {
	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/ugc_search", Perm("properties_view"), "Find players' models, cars and rockets and where they are",
			[](HTTPReply& reply, const HTTPContext& context) {
				RenderPage(reply, context, "ugc-search.jinja2", "ugc_search", { { "query", QueryValue(context.queryString, "q").substr(0, 100) } });
			});

		Route(eHTTPMethod::GET, "/api/ugc_links/search", Perm("properties_view"),
			"Players' creations matching ?q=: a number matches the ugc / blueprint id, a placed model's object id, the property, creator "
			"character or account id, or a LOT (a modular build's modules); text matches the creator's character or account name, a "
			"property name, the name or description given to a placed model, or the upload's file name. \"field: text\" searches one of "
			"id, owner, property, model, lot. {items: [{kind, ugcId, state, error, detail, characterId, characterName, accountId, accountName, "
			"icon, link, where: [{type: property|mail|inventory, ...}]}]}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto search = ParseQuery(QueryValue(context.queryString, "q"));
				if (search.text.empty() && !search.number) return JsonSuccess(reply, { { "items", nlohmann::json::array() } });
				const auto limit = std::clamp(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "limit")).value_or(SEARCH_LIMIT), 1u, SEARCH_LIMIT);
				const auto entries = Database::Get()->SearchUgc(search, limit);

				std::vector<LWOOBJID> ids, modularIds;
				std::set<LWOOBJID> models, modular;
				for (const auto& entry : entries) {
					ids.push_back(entry.id);
					if (entry.kind == eUgcKind::MODEL) {
						models.insert(entry.id);
					} else {
						modular.insert(entry.id);
						modularIds.push_back(entry.id);
					}
				}
				std::map<std::pair<eUgcKind, LWOOBJID>, nlohmann::json> where;
				for (const auto& p : Database::Get()->GetUgcPlacements(ids)) {
					const auto kind = models.contains(p.ugcId) ? eUgcKind::MODEL : eUgcKind::MODULAR;
					where[{ kind, p.ugcId }].push_back({ { "type", "property" }, { "propertyId", std::to_string(p.propertyId) }, { "propertyName", p.propertyName },
						{ "ownerId", std::to_string(p.ownerId) }, { "ownerName", p.ownerName }, { "zoneId", p.zoneId }, { "modelId", std::to_string(p.modelId) },
						{ "lot", p.lot }, { "modelName", p.modelName }, { "modelDescription", p.modelDescription } });
				}
				for (const auto& mail : Database::Get()->GetUgcMail(modularIds, MODEL_ITEM_LOT)) {
					if (const auto creation = MailCreation(mail, models, modular)) {
						where[*creation].push_back({ { "type", "mail" }, { "mailId", std::to_string(mail.id) }, { "characterId", std::to_string(mail.receiverId) },
							{ "characterName", mail.receiverName } });
					}
				}
				// Not placed or mailed: most are still with whoever made them
				std::set<LWOOBJID> creators;
				for (const auto& entry : entries) {
					if (!where.contains({ entry.kind, entry.id }) && entry.characterId != LWOOBJID_EMPTY && creators.size() < MAX_INVENTORIES) creators.insert(entry.characterId);
				}
				for (const auto creator : creators) {
					for (const auto& link : CharacterCreations(creator)) {
						const std::pair key{ link.kind, link.ugcId };
						const bool wanted = link.kind == eUgcKind::MODEL ? models.contains(link.ugcId) : modular.contains(link.ugcId);
						if (!wanted) continue;
						where[key].push_back({ { "type", "inventory" }, { "characterId", std::to_string(creator) }, { "inventory", link.item.inventory },
							{ "itemId", std::to_string(link.item.itemId) }, { "lot", link.item.lot } });
					}
				}

				nlohmann::json items = nlohmann::json::array();
				for (const auto& entry : entries) {
					auto item = EntryJson(entry, "", true);
					item["detail"] = entry.detail;
					item["characterId"] = std::to_string(entry.characterId);
					item["characterName"] = entry.characterName;
					item["accountId"] = entry.accountId;
					item["accountName"] = entry.accountName;
					const auto found = where.find({ entry.kind, entry.id });
					item["where"] = found == where.end() ? nlohmann::json::array() : found->second;
					items.push_back(std::move(item));
				}
				// The newest of both kinds first (ids are handed out in order)
				std::stable_sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
					return GeneralUtils::TryParse<LWOOBJID>(a["ugcId"].template get<std::string>()).value_or(0) > GeneralUtils::TryParse<LWOOBJID>(b["ugcId"].template get<std::string>()).value_or(0);
				});
				JsonSuccess(reply, { { "items", items } });
			});

		Route(eHTTPMethod::GET, "/api/ugc_links/property/:id", 0,
			"The player-built models placed on a property, with what the UGC server made of them: {items: [{modelId, kind, ugcId, state, error, "
			"icon, mesh, link}]}. Whoever may view the property (properties_view, or its owner with own_properties)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto propertyId = PathId<LWOOBJID>(context.path, 3);
				if (!propertyId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid property id");
				if (!Database::Get()->GetPropertyInfo(*propertyId)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Property not found");
				if (!MayViewProperty(context, *propertyId)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may only view your own properties");
				std::map<LWOOBJID, std::vector<LWOOBJID>> placed; // ugc id -> placed models
				for (const auto& model : Database::Get()->GetPropertyModels(*propertyId)) {
					if (model.ugcId != 0) placed[model.ugcId].push_back(model.id);
				}
				std::vector<LWOOBJID> ids;
				for (const auto& [id, _] : placed) ids.push_back(id);
				const auto via = "property=" + std::to_string(*propertyId);
				const bool viewer = Can(context, "properties_view");
				nlohmann::json items = nlohmann::json::array();
				for (const auto& entry : Database::Get()->GetUgcEntries(ids)) {
					for (const auto modelId : placed[entry.id]) {
						auto item = EntryJson(entry, via, viewer);
						item["modelId"] = std::to_string(modelId);
						if (entry.kind == eUgcKind::MODEL && entry.state == IUgc::eProcessState::DONE) {
							item["mesh"] = "/api/ugc_links/mesh/" + std::to_string(entry.id) + "?" + via;
						}
						items.push_back(std::move(item));
					}
				}
				JsonSuccess(reply, { { "items", items } });
			});

		Route(eHTTPMethod::GET, "/api/ugc_links/character/:id", 0,
			"The creations (models, cars, rockets) in a character's saved inventories: {items: [{itemId, lot, inventory, kind, ugcId, state, error, icon, link}]}. "
			"Whoever may view the character",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto characterId = PathId<LWOOBJID>(context.path, 3);
				const auto info = characterId ? Database::Get()->GetCharacterInfo(*characterId) : std::nullopt;
				if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
				if (!CanViewCharacter(context, info->accountId)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may only view your own characters");
				const auto links = CharacterCreations(*characterId);
				std::vector<LWOOBJID> ids;
				for (const auto& link : links) ids.push_back(link.ugcId);
				std::map<std::pair<eUgcKind, LWOOBJID>, IUgcLookup::UgcEntry> entries;
				for (auto& entry : Database::Get()->GetUgcEntries(ids)) entries[{ entry.kind, entry.id }] = std::move(entry);
				const auto via = "character=" + std::to_string(*characterId);
				const bool viewer = Can(context, "properties_view");
				nlohmann::json items = nlohmann::json::array();
				for (const auto& link : links) {
					const auto entry = entries.find({ link.kind, link.ugcId });
					if (entry == entries.end()) continue;
					auto item = EntryJson(entry->second, via, viewer);
					item["itemId"] = std::to_string(link.item.itemId);
					item["lot"] = link.item.lot;
					item["inventory"] = link.item.inventory;
					items.push_back(std::move(item));
				}
				JsonSuccess(reply, { { "items", items } });
			});

		Route(eHTTPMethod::GET, "/api/ugc_links/model/:id", 0,
			"Everything known of a player-built model (by its UGC / blueprint id) for the property pages: {ugcId, state, error, attempts, processedAt, "
			"processAfter, bakeAo, fileName, characterId, characterName, accountName, placements: [{modelId, propertyId, name, description}], icon, "
			"previousIcon, nif, previousNif, link, canManage, stats, previousStats} (stats: the UGC server's stats.json, null when it has none). "
			"Who may see it as /api/ugc_links/icon; 404 when the model has no UGC row",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = PathId<LWOOBJID>(context.path, 3);
				if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid id");
				const auto entries = Database::Get()->GetUgcEntries({ *id });
				const auto entry = std::find_if(entries.begin(), entries.end(), [](const auto& e) { return e.kind == eUgcKind::MODEL; });
				if (entry == entries.end()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No UGC data for this model");
				if (!MaySee(context, eUgcKind::MODEL, *id)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may not view this creation");

				// The query string that let the viewer see it, passed on to the file links
				std::string via;
				if (const auto property = GeneralUtils::TryParse<LWOOBJID>(QueryValue(context.queryString, "property"))) via = "property=" + std::to_string(*property);
				else if (const auto character = GeneralUtils::TryParse<LWOOBJID>(QueryValue(context.queryString, "character"))) via = "character=" + std::to_string(*character);
				const auto file = [&](const std::string& name) {
					return "/api/ugc_links/file/" + std::to_string(*id) + "/" + name + (via.empty() ? "" : "?" + via);
				};

				auto out = EntryJson(*entry, via, Can(context, "properties_view"));
				out["fileName"] = entry->detail;
				out["characterId"] = std::to_string(entry->characterId);
				out["characterName"] = entry->characterName;
				out["accountName"] = Can(context, "accounts_view") ? nlohmann::json(entry->accountName) : nlohmann::json();
				const auto process = Database::Get()->GetUgcProcessList(std::nullopt, std::to_string(*id), 0, 1);
				if (!process.empty() && process.front().id == *id) {
					const auto& info = process.front();
					out["attempts"] = info.attempts;
					out["processedAt"] = info.processedAt;
					out["processAfter"] = info.processAfter;
					out["bakeAo"] = info.bakeAo;
				}
				nlohmann::json placements = nlohmann::json::array();
				for (const auto& placement : Database::Get()->GetUgcPlacements({ *id })) {
					if (!MayViewProperty(context, placement.propertyId)) continue;
					placements.push_back({ { "modelId", std::to_string(placement.modelId) }, { "propertyId", std::to_string(placement.propertyId) },
						{ "propertyName", placement.propertyName }, { "name", placement.modelName }, { "description", placement.modelDescription } });
				}
				out["placements"] = placements;
				out["previousIcon"] = file("previous.icon.png");
				out["nif"] = file("model.nif");
				out["previousNif"] = file("previous.model.nif");
				out["canManage"] = Can(context, "ugc_manage");

				const auto base = InternalUrl() + "/files/model/" + std::to_string(*id) + "/";
				Workers::Reply(reply, context, false, [base, out = std::move(out)](HTTPReply& answer) mutable {
					const auto stats = [&](const std::string& name) {
						const auto fetched = CachedGet(base + name);
						if (fetched->status != 200) return nlohmann::json();
						auto parsed = nlohmann::json::parse(fetched->body, nullptr, false);
						return parsed.is_discarded() ? nlohmann::json() : parsed;
					};
					out["stats"] = stats("stats.json");
					out["previousStats"] = stats("previous.stats.json");
					JsonSuccess(answer, out);
				}, WorkerPool::ePriority::URGENT);
			});

		Route(eHTTPMethod::GET, "/api/ugc_links/file/:id/:name", 0,
			"A file the UGC server made of a player-built model: model.nif, previous.model.nif (downloads) or previous.icon.png. "
			"Who may see it as /api/ugc_links/icon",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = PathId<LWOOBJID>(context.path, 3);
				const std::string name(PathSegment(context.path, 4));
				if (!id || (name != "model.nif" && name != "previous.model.nif" && name != "previous.icon.png")) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid id or file");
				if (!MaySee(context, eUgcKind::MODEL, *id)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may not view this creation");
				const auto url = InternalUrl() + "/files/model/" + std::to_string(*id) + "/" + name;
				const bool icon = name.ends_with(".png");
				const auto download = std::to_string(*id) + (name.starts_with("previous.") ? ".previous.nif" : ".nif");
				Workers::Reply(reply, context, false, [url, icon, download](HTTPReply& out) {
					const auto fetched = CachedGet(url);
					if (fetched->status != 200) return ReplyError(out, *fetched);
					out.status = eHTTPStatusCode::OK;
					out.contentType = icon ? eContentType::IMAGE_PNG : eContentType::APPLICATION_OCTET_STREAM;
					out.message = fetched->body;
					out.headers.push_back("Cache-Control: private, max-age=60");
					if (!icon) out.headers.push_back("Content-Disposition: attachment; filename=\"" + download + "\"");
				});
			});

		Route(eHTTPMethod::GET, "/api/ugc_links/icon/:kind/:id", 0,
			"The icon the UGC server made of a creation (kind model or modular), fetched from ugc_internal_url. With properties_view, for your own "
			"characters' creations, or with ?property= / ?character= naming a page you may view that shows it. 404 until it is made",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto kind = ParseKind(PathSegment(context.path, 3));
				const auto id = PathId<LWOOBJID>(context.path, 4);
				if (!kind || !id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid kind or id");
				if (!MaySee(context, *kind, *id)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may not view this creation");
				const auto url = InternalUrl() + "/files/" + KindName(*kind) + "/" + std::to_string(*id) + "/icon.png";
				Workers::Reply(reply, context, false, [url](HTTPReply& out) {
					const auto fetched = CachedGet(url);
					if (fetched->status != 200) return ReplyError(out, *fetched);
					out.status = eHTTPStatusCode::OK;
					out.contentType = eContentType::IMAGE_PNG;
					out.message = fetched->body;
					out.headers.push_back("Cache-Control: private, max-age=60");
				}, WorkerPool::ePriority::URGENT);
			});

		Route(eHTTPMethod::GET, "/api/ugc_links/mesh/:id", 0,
			"The .nif the UGC server made of a player-built model, converted for the 3D view (as /api/ugc/mesh/:id). ?lod=0 (most detailed) to 3. "
			"Who may see it as /api/ugc_links/icon",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = PathId<LWOOBJID>(context.path, 3);
				if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid id");
				if (!MaySee(context, eUgcKind::MODEL, *id)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may not view this creation");
				const auto lod = std::min(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "lod")).value_or(0), 3u);
				const auto url = InternalUrl() + "/files/model/" + std::to_string(*id) + "/model.nif";
				Workers::Reply(reply, context, false, [url, lod](HTTPReply& out) {
					const auto fetched = CachedGet(url);
					if (fetched->status != 200) return ReplyError(out, *fetched);
					std::string error;
					const auto model = NifFile::Parse(fetched->body, lod, error);
					if (!model) return JsonError(out, eHTTPStatusCode::UNPROCESSABLE_ENTITY, "The .nif can't be read: " + error);
					out.status = eHTTPStatusCode::OK;
					out.contentType = eContentType::APPLICATION_OCTET_STREAM;
					out.message = NifFile::Encode(*model, std::vector<std::string>(model->meshes.size()));
					out.headers.push_back("Cache-Control: private, max-age=60");
				});
			});
	}
}
