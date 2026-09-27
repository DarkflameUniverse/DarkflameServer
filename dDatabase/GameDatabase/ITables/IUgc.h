#ifndef __IUGC__H__
#define __IUGC__H__

#include <cstdint>
#include <sstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
	virtual void UpdateUgcModelData(const LWOOBJID& modelId, std::stringstream& lxfml) = 0;

	virtual std::optional<IUgc::Model> GetUgcModel(const LWOOBJID ugcId) = 0;

	// ---- Processing by the UGC server (is_optimized, processed_at, process_attempts, process_error) ----

	// is_optimized's values
	enum class eProcessState : int32_t {
		PENDING = 0, // not made yet (or to be made again)
		DONE = 1,    // the UGC server's files are made
		FAILED = 2,  // gave up after the allowed attempts
	};

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
	};

	// A model waiting to be made: its id, stored LXFML (sd0) and the attempts so far
	struct PendingModel {
		LWOOBJID id{};
		std::string lxfml;
		uint32_t attempts{};
	};

	// Up to `limit` pending models, the least tried and then the newest first
	virtual std::vector<PendingModel> GetUgcModelsToProcess(const uint32_t limit) = 0;

	// Records an attempt: the new state, how many attempts there have been, why it failed (empty when it didn't) and
	// whether lighting was baked in; processed_at becomes now
	virtual void SetUgcModelProcessed(const LWOOBJID id, const eProcessState state, const uint32_t attempts, const std::string_view error, const bool bakeAo) = 0;

	virtual std::optional<ProcessInfo> GetUgcProcessInfo(const LWOOBJID id) = 0;

	// Sets models back to pending with no attempts: one (`id`), or all of them (`id` nullopt; only the failed ones
	// with `failedOnly`). Returns how many rows changed.
	virtual uint64_t ResetUgcModelProcessing(const std::optional<LWOOBJID> id, const bool failedOnly) = 0;

	// A page of models (all, or those in `state`), the newest first
	virtual std::vector<ProcessInfo> GetUgcProcessList(const std::optional<eProcessState> state, const uint32_t offset, const uint32_t limit) = 0;

	// How many models are in each state
	virtual std::vector<std::pair<eProcessState, uint64_t>> GetUgcProcessCounts() = 0;
};
#endif  //!__IUGC__H__
