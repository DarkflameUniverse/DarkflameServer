#include "Showcase.h"

#include <chrono>
#include <map>
#include <memory>

#include "RouteUtils.h"
#include "Permissions.h"
#include "PublicRoutes.h"
#include "PropertyAssets.h"
#include "ReportRoutes.h"
#include "DashboardRoutes.h"
#include "ClientAssets.h"
#include "Scenery.h"
#include "BehaviorXml.h"
#include "RequireAuthMiddleware.h"
#include "TtlCache.h"
#include "CDClientDatabase.h"
#include "Database.h"
#include "ePropertyPrivacyOption.h"
#include "GeneralUtils.h"
#include "Sd0.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	using namespace std::chrono_literals;
	constexpr uint32_t PAGE_SIZE = 24;
	constexpr size_t MAX_SEARCH = 64;
	constexpr LOT UGC_MODEL_LOT = 14;
	constexpr size_t MEGABYTE = 1024 * 1024;

	// Only for visitors who aren't signed in (showcase_public=1). The 3D view asks for every model and brick design
	// on its own, so assets get a much larger allowance than the lists.
	RateLimiter g_DataLimiter(240, std::chrono::seconds(60));
	RateLimiter g_AssetLimiter(6000, std::chrono::seconds(60));

	// Whether a property may be shown is checked again every few seconds, so a property a moderator rejects or its
	// owner makes private drops out quickly; what it holds is kept longer
	TtlCache<LWOOBJID, IProperty::Info> g_Shown(10s, 1024);
	TtlCache<LWOOBJID, std::shared_ptr<const std::string>> g_Details(60s, 32 * MEGABYTE);
	TtlCache<LWOOBJID, std::shared_ptr<const std::map<LWOOBJID, IPropertyContents::Model>>> g_Models(60s, 256);
	TtlCache<LWOOBJID, std::shared_ptr<const std::map<LWOOBJID, std::string>>> g_UgcLxfml(10min, 64 * MEGABYTE); // by property: ugc id -> LXFML
	TtlCache<std::string, std::shared_ptr<const std::string>> g_Lists(10s, 256);

	enum class Kind { PAGE, DATA, ASSET };

	/**
	 * Who may see the showcase: anyone with showcase_public=1 (rate limited when not signed in), otherwise the same
	 * checks as any route guarded by showcase_view (signed in, two-factor set up where required, API access for
	 * tokens). Writes the reply and returns false when refused.
	 */
	bool Allowed(const HTTPContext& context, HTTPReply& reply, Kind kind) {
		if (ConfigFlag("showcase_public", false)) {
			if (context.isAuthenticated || kind == Kind::PAGE) return true;
			auto& limiter = kind == Kind::ASSET ? g_AssetLimiter : g_DataLimiter;
			if (limiter.Allow(ClientAddress(context))) return true;
			JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many requests, try again in a minute");
			return false;
		}
		static RequireAuthMiddleware gate(std::function<uint8_t()>([] { return Permissions::Level("showcase_view"); }));
		auto copy = context;
		return gate.Process(copy, reply);
	}

	bool Showable(const IProperty::Info& info) {
		return info.modApproved == 1 && info.privacyOption == static_cast<int32_t>(PropertyPrivacyOption::Public);
	}

	// The property if it is public and approved; a 404 otherwise, the same as for one that doesn't exist
	std::optional<IProperty::Info> ShownProperty(LWOOBJID id, HTTPReply& reply) {
		auto info = g_Shown.Get(id);
		if (!info) {
			info = Database::Get()->GetPropertyInfo(id);
			if (info && Showable(*info)) g_Shown.Put(id, *info);
		}
		if (!info || !Showable(*info)) {
			JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Property not found");
			return std::nullopt;
		}
		return info;
	}

	std::string ZoneName(uint32_t zoneId) {
		const auto& zones = ZoneNames();
		const auto key = std::to_string(zoneId);
		return zones.contains(key) && zones[key].is_string() ? zones[key].get<std::string>() : "Zone " + key;
	}

	// The zones properties can be claimed in (PropertyTemplate), for the zone filter
	const nlohmann::json& PropertyZones() {
		static const nlohmann::json zones = [] {
			nlohmann::json list = nlohmann::json::array();
			auto result = CDClientDatabase::ExecuteQuery("SELECT DISTINCT mapID FROM PropertyTemplate ORDER BY mapID;");
			for (; !result.eof(); result.nextRow()) {
				const auto id = static_cast<uint32_t>(result.getIntField(0));
				list.push_back({ {"id", id}, {"name", ZoneName(id)} });
			}
			return list;
		}();
		return zones;
	}

	std::shared_ptr<const std::map<LWOOBJID, IPropertyContents::Model>> Models(LWOOBJID propertyId) {
		if (auto cached = g_Models.Get(propertyId)) return *cached;
		auto models = std::make_shared<std::map<LWOOBJID, IPropertyContents::Model>>();
		for (auto& model : Database::Get()->GetPropertyModels(propertyId)) models->emplace(model.id, model);
		g_Models.Put(propertyId, models);
		return models;
	}

	// What the viewer needs about a property and its models. Owner by character name only; no account, no rent,
	// no moderation details.
	std::shared_ptr<const std::string> Details(const IProperty::Info& info) {
		if (auto cached = g_Details.Get(info.id)) return *cached;
		const auto owner = Database::Get()->GetCharacterInfo(info.ownerId);
		nlohmann::json models = nlohmann::json::array();
		for (const auto& [id, model] : *Models(info.id)) {
			models.push_back({
				{"id", std::to_string(model.id)},
				{"lot", model.lot},
				{"name", model.lot == UGC_MODEL_LOT ? "Player-built model" : ClientAssets::ItemName(model.lot)},
				{"position", {model.position.x, model.position.y, model.position.z}},
				{"rotation", {model.rotation.x, model.rotation.y, model.rotation.z, model.rotation.w}},
				{"behaviors", PropertyAssets::ModelBehaviors(model)}
			});
		}
		auto body = std::make_shared<const std::string>(nlohmann::json{
			{"success", true},
			{"id", std::to_string(info.id)},
			{"name", info.name},
			{"description", info.description},
			{"owner_name", owner ? owner->name : ""},
			{"zone_id", info.zoneId},
			{"zone_name", ZoneName(info.zoneId)},
			{"reputation", info.reputation},
			{"last_updated", info.lastUpdatedTime},
			{"models", models}
		}.dump());
		g_Details.Put(info.id, body, body->size());
		return body;
	}

	// Every player-built model's LXFML on a property, read in one query (a big property has hundreds)
	std::shared_ptr<const std::map<LWOOBJID, std::string>> UgcLxfml(LWOOBJID propertyId) {
		if (auto cached = g_UgcLxfml.Get(propertyId)) return *cached;
		auto lxfml = std::make_shared<std::map<LWOOBJID, std::string>>();
		size_t bytes = 0;
		for (auto& ugc : Database::Get()->GetUgcModels(propertyId)) {
			Sd0 sd0(ugc.lxfmlData);
			auto& text = (*lxfml)[ugc.id] = sd0.GetAsStringUncompressed();
			bytes += text.size();
		}
		g_UgcLxfml.Put(propertyId, lxfml, std::max<size_t>(bytes, 1));
		return lxfml;
	}

	// cacheControl empty: the default (no-store), for anything that stops being shown when a property is rejected
	void RawReply(HTTPReply& reply, eContentType type, std::string body, const std::string& cacheControl) {
		reply.status = eHTTPStatusCode::OK;
		reply.contentType = type;
		reply.message = std::move(body);
		if (!cacheControl.empty()) reply.headers.push_back("Cache-Control: " + cacheControl);
	}

	// Terrain and the build area come from the client's files and are the same for every property in a zone
	template<typename Fetch>
	void ZoneFileRoute(const std::string& path, const std::string& description, Fetch fetch) {
		Route(eHTTPMethod::GET, path, PUBLIC, description, [fetch](HTTPReply& reply, const HTTPContext& context) {
			if (!Allowed(context, reply, Kind::DATA)) return;
			const auto id = PathId<LWOOBJID>(context.path, 2);
			if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid ID");
			const auto info = ShownProperty(*id, reply);
			if (!info) return;
			fetch(reply, *info);
		});
	}
}

void RegisterShowcaseRoutes() {
	Route(eHTTPMethod::GET, "/showcase", PUBLIC, "Browse approved public properties (showcase_view, or everyone with showcase_public=1)",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!Allowed(context, reply, Kind::PAGE)) return;
			RenderPage(reply, context, "showcase.jinja2", "showcase", { {"public_page", PublicPageJson()} });
		});

	Route(eHTTPMethod::GET, "/showcase/:id", PUBLIC, "3D view of an approved public property, read only (showcase_view, or everyone with showcase_public=1)",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!Allowed(context, reply, Kind::PAGE)) return;
			const auto id = PathId<LWOOBJID>(context.path, 1);
			if (!id) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Invalid property ID");
			static const auto behaviorRules = BehaviorXml::Rules(ClientAssets::ItemName).dump();
			RenderPage(reply, context, "property-3d.jinja2", "showcase", {
				{"property_id", std::to_string(*id)}, {"behavior_rules", behaviorRules}, {"showcase", true}, {"public_page", PublicPageJson()}
			});
		});

	Route(eHTTPMethod::GET, "/api/showcase", PUBLIC,
		"Approved public properties, 24 per page. Query: ?search= (name, description or owner), &zone= (zone ID), &sort=reputation|newest|name, &page= (from 1). "
		"Needs showcase_view unless showcase_public=1",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!Allowed(context, reply, Kind::DATA)) return;
			IProperty::ShowcaseQuery query;
			query.search = QueryValue(context.queryString, "search").substr(0, MAX_SEARCH);
			query.zoneId = GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "zone")).value_or(0);
			const auto sort = QueryValue(context.queryString, "sort");
			query.sort = sort == "newest" ? IProperty::ShowcaseSort::NEWEST : sort == "name" ? IProperty::ShowcaseSort::NAME : IProperty::ShowcaseSort::REPUTATION;
			const auto page = std::clamp<uint32_t>(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "page")).value_or(1), 1, 100000);
			query.start = (page - 1) * PAGE_SIZE;
			query.length = PAGE_SIZE;

			const auto key = query.search + '\n' + std::to_string(query.zoneId) + '\n' + std::to_string(static_cast<int>(query.sort)) + '\n' + std::to_string(page);
			auto body = g_Lists.Get(key);
			if (!body) {
				const auto result = Database::Get()->GetShowcaseProperties(query);
				nlohmann::json properties = nlohmann::json::array();
				for (const auto& entry : result.entries) {
					properties.push_back({
						{"id", std::to_string(entry.info.id)},
						{"name", entry.info.name},
						{"description", entry.info.description},
						{"owner_name", entry.ownerName},
						{"zone_id", entry.info.zoneId},
						{"zone_name", ZoneName(entry.info.zoneId)},
						{"models", entry.modelCount},
						{"reputation", entry.info.reputation},
						{"last_updated", entry.info.lastUpdatedTime}
					});
				}
				body = std::make_shared<const std::string>(nlohmann::json{
					{"success", true}, {"total", result.total}, {"page", page}, {"page_size", PAGE_SIZE}, {"properties", properties}, {"zones", PropertyZones()}
				}.dump());
				g_Lists.Put(key, *body);
			}
			RawReply(reply, eContentType::APPLICATION_JSON, **body, "");
		});

	Route(eHTTPMethod::GET, "/api/showcase/:id", PUBLIC, "An approved public property and its placed models, for the 3D view. Needs showcase_view unless showcase_public=1",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!Allowed(context, reply, Kind::DATA)) return;
			const auto id = PathId<LWOOBJID>(context.path, 2);
			if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid ID");
			const auto info = ShownProperty(*id, reply);
			if (!info) return;
			RawReply(reply, eContentType::APPLICATION_JSON, *Details(*info), "");
		});

	ZoneFileRoute("/api/showcase/:id/terrain_chunks", "The terrain of a showcased property's zone, as the game draws it (needs client_location)",
		[](HTTPReply& reply, const IProperty::Info& info) {
			const auto terrain = ZoneTerrainChunksJson(info.zoneId);
			if (!terrain) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No terrain for this zone");
			RawReply(reply, eContentType::APPLICATION_JSON, *terrain, "private, max-age=86400");
		});

	ZoneFileRoute("/api/showcase/:id/terrain", "A height grid of a showcased property's zone (needs client_location)",
		[](HTTPReply& reply, const IProperty::Info& info) {
			const auto terrain = ZoneTerrainJson(info.zoneId);
			if (!terrain) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No terrain for this zone");
			RawReply(reply, eContentType::APPLICATION_JSON, *terrain, "private, max-age=86400");
		});

	ZoneFileRoute("/api/showcase/:id/boundary", "Where a showcased property's owner may build (needs client_location)",
		[](HTTPReply& reply, const IProperty::Info& info) {
			const auto areas = ZonePropertyAreasJson(info.zoneId);
			if (!areas) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Couldn't read this zone's file");
			JsonSuccess(reply, { {"areas", *areas} });
		});

	Route(eHTTPMethod::GET, "/api/showcase/:id/models/:model/lxfml", PUBLIC, "A model on a showcased property as LXFML, for the 3D view",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!Allowed(context, reply, Kind::ASSET)) return;
			const auto id = PathId<LWOOBJID>(context.path, 2);
			const auto modelId = PathId<LWOOBJID>(context.path, 4);
			if (!id || !modelId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid ID");
			const auto info = ShownProperty(*id, reply);
			if (!info) return;
			// Only models placed on this property
			const auto models = Models(*id);
			const auto model = models->find(*modelId);
			if (model == models->end()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Model not found");

			std::optional<std::string> lxfml;
			if (model->second.lot == UGC_MODEL_LOT) {
				const auto all = UgcLxfml(*id);
				if (const auto it = all->find(model->second.ugcId); it != all->end()) lxfml = it->second;
			} else if (const auto file = PropertyAssets::ModelLxfml(model->second, *id)) {
				lxfml = file->first;
			}
			if (!lxfml) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Model data not available");
			RawReply(reply, eContentType::APPLICATION_OCTET_STREAM, std::move(*lxfml), "");
		});

	// Client files, the same for every property
	Route(eHTTPMethod::GET, "/api/showcase/bricks/:lod/:design", PUBLIC, "Brick geometry for the showcase's 3D view (see /api/bricks/:lod/:design)",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!Allowed(context, reply, Kind::ASSET)) return;
			const auto lod = PathId<uint32_t>(context.path, 3);
			const auto design = PathId<uint32_t>(context.path, 4);
			const auto bundle = lod && design ? PropertyAssets::BrickBundle(*lod, *design) : std::nullopt;
			if (!bundle) {
				reply.status = eHTTPStatusCode::NOT_FOUND;
				reply.message = "";
				return;
			}
			RawReply(reply, eContentType::APPLICATION_OCTET_STREAM, *bundle, "public, max-age=604800");
		});

	Route(eHTTPMethod::GET, "/api/showcase/terrain_textures/:id", PUBLIC, "A terrain texture as PNG for the showcase's 3D view",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!Allowed(context, reply, Kind::ASSET)) return;
			const auto textureId = PathId<uint32_t>(context.path, 3);
			if (!textureId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid ID");
			const auto png = TerrainTextureFile(*textureId);
			if (!png) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Texture not found");
			RawReply(reply, eContentType::IMAGE_PNG, *png, "public, max-age=604800");
		});

	ZoneFileRoute("/api/showcase/:id/scenery", "Everything the game draws around a showcased property: its zone's scene objects with their models and the sky (needs client_location)",
		[](HTTPReply& reply, const IProperty::Info& info) {
			const auto scenery = Scenery::ZoneJson(info.zoneId);
			if (!scenery) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No scenery for this zone");
			RawReply(reply, eContentType::APPLICATION_JSON, *scenery, "private, max-age=86400");
		});

	// Scenery models and textures of the zones properties are in (the only ones the showcase shows)
	const auto propertyZone = [](HTTPReply& reply, const HTTPContext& context) -> std::optional<uint32_t> {
		if (!Allowed(context, reply, Kind::ASSET)) return std::nullopt;
		const auto zone = PathId<uint32_t>(context.path, 3);
		const auto& zones = PropertyZones();
		if (!zone || std::none_of(zones.begin(), zones.end(), [&zone](const nlohmann::json& z) { return z["id"] == *zone; })) {
			JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Not a property zone");
			return std::nullopt;
		}
		return zone;
	};
	const auto lodOf = [](const HTTPContext& context) { return GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "lod")).value_or(0); };

	Route(eHTTPMethod::GET, "/api/showcase/scenery/:zone/mesh/:asset", PUBLIC, "A scenery model of a property zone for the showcase's 3D view (see /api/scenery/:zone/mesh/:asset)",
		[propertyZone, lodOf](HTTPReply& reply, const HTTPContext& context) {
			const auto zone = propertyZone(reply, context);
			if (!zone) return;
			const auto asset = PathId<uint32_t>(context.path, 5);
			if (!asset) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid model");
			Scenery::ReplyMesh(reply, context, *zone, *asset, lodOf(context));
		});

	Route(eHTTPMethod::GET, "/api/showcase/scenery/:zone/texture/:asset/:slot", PUBLIC, "A scenery texture of a property zone as DDS for the showcase's 3D view",
		[propertyZone, lodOf](HTTPReply& reply, const HTTPContext& context) {
			const auto zone = propertyZone(reply, context);
			if (!zone) return;
			const auto asset = PathId<uint32_t>(context.path, 5);
			const auto slot = PathId<uint32_t>(context.path, 6);
			if (!asset || !slot) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid model or texture");
			Scenery::ReplyTexture(reply, context, *zone, *asset, *slot, lodOf(context));
		});
}
