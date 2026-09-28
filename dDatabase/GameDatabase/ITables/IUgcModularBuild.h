#ifndef IUGCMODULARBUILD_H
#define IUGCMODULARBUILD_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "IUgc.h"

class IUgcModularBuild {
public:
	virtual void InsertUgcBuild(const std::string& modules, const LWOOBJID bigId, const std::optional<LWOOBJID> characterId) = 0;
	virtual void DeleteUgcBuild(const LWOOBJID bigId) = 0;

	// ---- Icons made by the UGC server; the same columns and meanings as IUgc's processing ----

	// A modular build waiting for its icon: its id, modules (ldf_config) and the attempts so far
	struct PendingBuild {
		LWOOBJID id{};
		std::string modules;
		uint32_t attempts{};
	};

	virtual std::vector<PendingBuild> GetModularBuildsToProcess(const uint32_t limit) = 0;
	virtual void SetModularBuildProcessed(const LWOOBJID id, const IUgc::eProcessState state, const uint32_t attempts, const std::string_view error) = 0;
	virtual std::optional<IUgc::ProcessInfo> GetModularBuildProcessInfo(const LWOOBJID id) = 0;
	virtual uint64_t ResetModularBuildProcessing(const std::optional<LWOOBJID> id, const bool failedOnly) = 0;
	virtual std::vector<IUgc::ProcessInfo> GetModularBuildProcessList(const std::optional<IUgc::eProcessState> state, const std::string_view search, const uint32_t offset, const uint32_t limit) = 0;
	virtual std::vector<std::pair<IUgc::eProcessState, uint64_t>> GetModularBuildProcessCounts() = 0;

	// The combination of modules a build was made as (UgcModularKey::StorageId of its modules), whose files it shares
	virtual void SetModularBuildCombination(const LWOOBJID id, const LWOOBJID combinationId) = 0;

	// The last successful make of a build's icon (see IUgc::ProcessStats)
	virtual void SetModularBuildProcessStats(const LWOOBJID id, const IUgc::ProcessStats& stats) = 0;

	// Up to `limit` builds whose combination isn't recorded yet (combination_id 0): their ids and modules
	virtual std::vector<PendingBuild> GetModularBuildsWithoutCombination(const uint32_t limit) = 0;

	// How many builds there are of each ldf_config as stored (the same modules may be written differently)
	virtual std::vector<std::pair<std::string, uint64_t>> GetModularBuildConfigCounts() = 0;

	// Icon settings (UgcIconParams JSON) set on the dashboard for a target: "kind:<kind>" (a preset for player models or
	// a car or rocket build type), "model:<id>" or "combo:<modules key>" (one item's or combination's own)
	virtual std::optional<std::string> GetUgcIconSettings(const std::string_view target) = 0;
	virtual void SetUgcIconSettings(const std::string_view target, const std::string_view params) = 0;
	virtual void DeleteUgcIconSettings(const std::string_view target) = 0;
};

#endif  //!IUGCMODULARBUILD_H
