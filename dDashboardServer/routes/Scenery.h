#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct HTTPReply;
struct HTTPContext;

namespace WorldScene {
	struct Object;
}

namespace NifFile {
	struct Model;
}

/**
 * A zone's scenery for the 3D views, drawn from the game client's files the way the client draws it: every object in
 * the zone's scene files (.lvl) with the model of its render component (RenderComponent.render_asset, a .nif or a
 * .kfm naming one), plus the sky the scene's environment chunk names.
 *
 * The browser gets one manifest per zone (which models, and where each object is) and then fetches the models it
 * wants, nearest first. Models are converted from .nif on the server (NifFile) to a small binary format and kept in a
 * bounded in-memory cache; textures are sent as the client's own DDS files (textures stored inside a .nif are
 * wrapped as DDS), which the browser decodes itself, so nothing needs ImageMagick.
 *
 * Everything here is thread safe: the manifests are built on worker threads too (the routes use Workers::Reply).
 * Converting a big model takes a while, so models and textures not in memory are answered from a few worker threads
 * (Web::Defer, Workers; scenery_workers in dashboardconfig.ini) and the web server keeps answering meanwhile.
 * Flairs and small models go first, in a lane of their own; the same model asked for twice at once is converted
 * once. When a zone's manifest is asked for, its models are converted ahead of time onto the disk cache while
 * someone views the zone.
 */
namespace Scenery {
	/**
	 * Read what the builders need from the CDClient (render components, flairs) and index the client's model files,
	 * at startup: the manifests are built on worker threads, which never query the CDClient.
	 */
	void Preload();

	/**
	 * The zone's manifest as JSON: {zone, sky (asset index or -1), assets: [res path, ...],
	 * objects: {asset: [...], pos: [x, y, z, ...], rot: [x, y, z, w, ...], scale: [...], hidden: [0 or 1, ...]}}, where
	 * hidden objects have a model the client doesn't draw (trigger and blocking volumes; WorldScene::ClientDraws), for
	 * views that show them on request. nullopt without client files.
	 */
	std::optional<std::string> ZoneJson(uint32_t zoneId);

	// Whether ZoneJson is built (so answering it is quick)
	bool ZoneReady(uint32_t zoneId);

	/**
	 * The eShaderLook bits (NifFile::ShaderLookFor) of each mesh of a multishader model (a player model: RenderComponent
	 * shader Multishader), from its parts' tags (mapShaders ids, read by Preload); for NifFile::Encode's `looks`.
	 * Any thread.
	 */
	std::vector<uint16_t> MultishaderLooks(const NifFile::Model& model);

	/**
	 * The zone's flairs (the grass, flowers and small rocks its terrain file strews over it, models from FlairTable) as a
	 * manifest in ZoneJson's form, whose models the same mesh and texture routes serve, plus: distance (how far from the
	 * camera the client draws them), color ([r, g, b, ...] per flair) and colorScale (what a color byte of 1 tints by).
	 * nullopt without client files.
	 */
	std::optional<std::string> FlairsJson(uint32_t zoneId);

	// Whether FlairsJson is built
	bool FlairsReady(uint32_t zoneId);

	// Whether the client draws a model for this scene object (its render component's, or its nif_name)
	bool HasModel(const WorldScene::Object& object);

	// Reply with model `asset` of the zone's manifest (NifFile::Encode), `lod` 0 the most detailed. Deferred (answered
	// from a worker thread) unless the model is in memory.
	void ReplyMesh(HTTPReply& reply, const HTTPContext& context, uint32_t zoneId, uint32_t asset, uint32_t lod);

	// Reply with texture `slot` (the model's "textures" list at that `lod`) of model `asset`, as a DDS file. Deferred.
	void ReplyTexture(HTTPReply& reply, const HTTPContext& context, uint32_t zoneId, uint32_t asset, uint32_t slot, uint32_t lod);

	// /api/scenery/:zone/mesh/:asset and /api/scenery/:zone/texture/:asset/:slot, for anyone signed in
	void RegisterRoutes();


}
