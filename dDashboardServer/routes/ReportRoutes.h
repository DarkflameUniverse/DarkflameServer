#pragma once

// Economy reports: coin/U-score/item flows, transfers between players, item tracing and duplicate scans
void RegisterReportRoutes();

#include "json.hpp"

#include <functional>
#include <string>

/**
 * Scan every character and unclaimed mail for duplicated object ids in the background; the result is also what the
 * Duplicates tab shows. done runs on the main thread. Returns false if a scan is already running.
 */
bool StartDuplicateScan(std::function<void(const nlohmann::json& scan, const std::string& error)> done);

#include <cstdint>
#include <optional>

// A zone's terrain as the map reports use it (heights quantized to 16 bits, base64), cached; nullopt without a client
std::optional<std::string> ZoneTerrainJson(uint32_t zoneId);

#include "Raw.h"

/**
 * The zone data below is built on the dashboard's worker threads too, so it is thread safe: results are built once
 * (OnceCache) and the CDClient tables it needs are read at startup by PreloadZoneData (workers never query the
 * CDClient). The ...Ready functions say whether a result is built, so a route can answer it at once.
 */
void PreloadZoneData();
bool ZoneTerrainJsonReady(uint32_t zoneId);
bool ZoneTerrainChunksReady(uint32_t zoneId);
bool ZoneTerrainLayersReady(uint32_t zoneId);
bool TerrainTextureReady(uint32_t textureId);

// A zone's .luz file (relative to res/maps) from ZoneTable
std::optional<std::string> ZoneLuzPath(uint32_t zoneId);

#include <memory>

// ZoneRaw, shared: the last few zones read are kept, and a zone asked for by several threads at once is read once
std::shared_ptr<const Raw::Raw> ZoneRawShared(uint32_t zoneId);

// A zone's terrain file (.raw) read whole, as its .luz names it; nullopt without client files or when it's damaged
std::optional<Raw::Raw> ZoneRaw(uint32_t zoneId);

// Every terrain chunk of a zone as the client draws it (heights, textures, color and blend maps); nullopt without files
std::optional<std::string> ZoneTerrainChunksJson(uint32_t zoneId);

/**
 * The rest of a zone's terrain file, for the 3D world view's terrain layers: per chunk (in ZoneTerrainChunksJson's
 * order) the scene of each terrain cell, plus the scenes found with their names from the .luz, the game's scene colors
 * and their share of the terrain. Cached; nullopt without files.
 */
std::optional<std::string> ZoneTerrainLayersJson(uint32_t zoneId);

// A terrain texture (mapTextureResource ID) as a PNG
std::optional<std::string> TerrainTextureFile(uint32_t textureId);

// A zone's property build areas (outline and height limit) from its .luz; nullopt without client files
std::optional<nlohmann::json> ZonePropertyAreasJson(uint32_t zoneId);

// Where a player can be put in a zone: [{name, lot, x, y, z}] from its scene files; nullopt without client files
std::optional<nlohmann::json> ZoneSpawnPointsJson(uint32_t zoneId);

// Economy source id (as a string) -> name, for labelling flows
nlohmann::json EconomySourceNames();
