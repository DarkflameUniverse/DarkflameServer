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
	virtual std::vector<IUgc::ProcessInfo> GetModularBuildProcessList(const std::optional<IUgc::eProcessState> state, const uint32_t offset, const uint32_t limit) = 0;
	virtual std::vector<std::pair<IUgc::eProcessState, uint64_t>> GetModularBuildProcessCounts() = 0;
};

#endif  //!IUGCMODULARBUILD_H
