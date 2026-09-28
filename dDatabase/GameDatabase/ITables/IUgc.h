#ifndef __IUGC__H__
#define __IUGC__H__

#include <cstdint>
#include <sstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "magic_enum.hpp"

class IUgc {
public:
	struct Model {
		std::stringstream lxfmlData;
		LWOOBJID id{};
		LWOOBJID modelID{};
	};

	// Gets all UGC models for the given property id.
	virtual std::vector<IUgc::Model> GetUgcModels(const LWOOBJID& propertyId) = 0;

	// Gets all Ugcs models.
	virtual std::vector<IUgc::Model> GetAllUgcModels() = 0;
	
	// Removes ugc models that are not referenced by any property.
	virtual void RemoveUnreferencedUgcModels() = 0;

	// Deletes the ugc model for the given model id.
	virtual void DeleteUgcModelData(const LWOOBJID& modelId) = 0;

	// Inserts a new UGC model into the database.
	virtual void UpdateUgcModelData(const LWOOBJID& modelId, std::stringstream& lxfml, const int64_t processAfter) = 0;

	virtual std::optional<IUgc::Model> GetUgcModel(const LWOOBJID ugcId) = 0;

	// ---- Processing by the UGC server (is_optimized, processed_at, process_attempts, process_error) ----

	// is_optimized's values
	enum class eProcessState : int32_t {
		PENDING = 0, // not made yet (or to be made again)
		DONE = 1,    // the UGC server's files are made
		FAILED = 2,  // gave up after the allowed attempts
		EMPTY = 3,   // nothing to make: the model has no bricks (not a failure; never retried)
	};

	// A state's name for the dashboard, the API and metrics ("pending", "done", "failed", "empty")
	static std::string ProcessStateName(eProcessState state) {
		std::string name(magic_enum::enum_name(state));
		for (auto& c : name) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
		return name.empty() ? "unknown" : name;
	}

	static std::optional<eProcessState> ParseProcessState(std::string_view text) {
		for (const auto state : magic_enum::enum_values<eProcessState>()) {
			if (ProcessStateName(state) == text) return state;
		}
		return std::nullopt;
	}

	// A row's processing state, for the UGC server and the dashboard
	struct ProcessInfo {
		LWOOBJID id{};
		LWOOBJID characterId{};
		std::string characterName; // empty when the character is gone
		eProcessState state{};
		uint32_t attempts{};
		int64_t processedAt{}; // Unix seconds of the last attempt, 0 for none
		std::string error;
		bool bakeAo{};
		std::string details; // modular builds: the modules (ldf_config)
		int64_t processAfter{}; // models: Unix seconds before which it isn't made (the quiet period after a save)
	};

	// A model waiting to be made: its id, stored LXFML (sd0) and the attempts so far
	struct PendingModel {
		LWOOBJID id{};
		std::string lxfml;
		uint32_t attempts{};
	};

	// Up to `limit` pending models whose quiet period is over (process_after), the least tried and then the newest first
	virtual std::vector<PendingModel> GetUgcModelsToProcess(const uint32_t limit) = 0;

	// Ends the quiet period of a waiting model (a client asked for it), or of all a character's waiting models (they
	// left the property)
	virtual void ExpediteUgcModel(const LWOOBJID id) = 0;
	virtual void ExpediteUgcModels(const LWOOBJID characterId) = 0;

	// Records an attempt: the new state, how many attempts there have been, why it failed (empty when it didn't) and
	// whether lighting was baked in; processed_at becomes now
	virtual void SetUgcModelProcessed(const LWOOBJID id, const eProcessState state, const uint32_t attempts, const std::string_view error, const bool bakeAo) = 0;

	// What the UGC server counted when it made a model: its bricks and the most detailed mesh's triangles
	virtual void SetUgcModelStats(const LWOOBJID id, const uint32_t bricks, const uint32_t triangles) = 0;

	// The last successful make of a model: how long it took, the worker's CPU time and the estimated memory
	struct ProcessStats {
		uint32_t milliseconds{};
		uint32_t cpuMilliseconds{};
		uint32_t memoryKb{};
	};
	virtual void SetUgcModelProcessStats(const LWOOBJID id, const ProcessStats& stats) = 0;

	virtual std::optional<ProcessInfo> GetUgcProcessInfo(const LWOOBJID id) = 0;

	// Sets models back to pending with no attempts: one (`id`), or all of them (`id` nullopt; only the failed ones
	// with `failedOnly`). Returns how many rows changed.
	virtual uint64_t ResetUgcModelProcessing(const std::optional<LWOOBJID> id, const bool failedOnly) = 0;

	// A page of models (all, or those in `state`), the newest first; `search` (when not empty) matches the model's id
	// exactly or part of its owner's name
	virtual std::vector<ProcessInfo> GetUgcProcessList(const std::optional<eProcessState> state, const std::string_view search, const uint32_t offset, const uint32_t limit) = 0;

	// How many models are in each state
	virtual std::vector<std::pair<eProcessState, uint64_t>> GetUgcProcessCounts() = 0;

	// ---- Checksums of the files the UGC server made (ugc_file_checksums), for the client's manifest requests ----

	// What the files are stored under: a player model's ugc id, or a combination of car or rocket modules
	enum class eFileOwner : int32_t {
		MODEL = 0,
		COMBINATION = 1
	};

	// A made file as the client has it after inflating the download: its MD5 (32 lowercase hex digits) and size
	struct FileChecksum {
		std::string md5;
		uint32_t size{};
	};

	// Records (or replaces) the checksum of a made file, e.g. "icon.dds" or "model.nif"
	virtual void SetUgcFileChecksum(const eFileOwner owner, const LWOOBJID storageId, const std::string_view file, const std::string_view md5, const uint32_t size) = 0;

	// The checksum of a blueprint's file: a player model's (the blueprint is its ugc id), else a car or rocket build's
	// (the blueprint is its ugc_modular_build id; the file is its combination's)
	virtual std::optional<FileChecksum> GetUgcFileChecksum(const LWOOBJID blueprintId, const std::string_view file) = 0;
};
#endif  //!__IUGC__H__
