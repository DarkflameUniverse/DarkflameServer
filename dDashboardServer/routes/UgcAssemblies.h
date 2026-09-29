#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "IUgcLookup.h"
#include "UgcKeys.h"

/**
 * Cars and rockets as the /ugc page lists them: one assembly per combination of modules (UgcModularKey::Normalize),
 * however many builds (ugc_modular_build rows) use it, since the UGC server makes one icon per combination. Pure (the
 * rows and the modules' CDClient data come in), so it can be unit tested; routes/UgcRoutes.cpp runs it.
 */
namespace UgcAssemblies {
	struct ModuleInfo {
		int32_t buildType{ -1 }; // ModuleComponent.buildType, -1 when the LOT isn't a module
		std::string name;        // Objects.displayName, else name
	};

	struct Assembly {
		std::string key;
		std::vector<uint32_t> lots;
		int32_t buildType{ -1 };             // of the first module that is one
		std::vector<LWOOBJID> builds;        // newest first
		std::set<LWOOBJID> owners;           // creator characters
		IUgc::eProcessState state{ IUgc::eProcessState::PENDING };
		LWOOBJID iconBuild{};                // a build whose icon is made (the newest made one, else the newest)
		std::string error;                   // a failed build's error, when none is made
		// The newest build's creator
		LWOOBJID characterId{};
		std::string characterName;
		uint32_t accountId{};
		std::string accountName;
		// The combination's last make: when (the latest any build of it was made or attempted) and what it cost (the
		// build the UGC server made it for; the builds that shared it after it was made cost nothing)
		int64_t processedAt{};
		uint32_t processMs{};
		uint32_t processCpuMs{};
		uint32_t processMemoryKb{};
	};

	// The models list's sorts (IUgcLookup::eSort) that assemblies have: MODULES is the Size column (the most modules first)
	enum class eSort : uint8_t { NEWEST, OLDEST, REFERENCES, NAME, OWNER, MADE, SLOWEST, CPU, MEMORY, MODULES };

	inline std::optional<eSort> ParseSort(std::string_view text) {
		if (text.empty() || text == "newest") return eSort::NEWEST;
		if (text == "oldest") return eSort::OLDEST;
		if (text == "references" || text == "uses") return eSort::REFERENCES;
		if (text == "name") return eSort::NAME;
		if (text == "owner") return eSort::OWNER;
		if (text == "made") return eSort::MADE;
		if (text == "slowest") return eSort::SLOWEST;
		if (text == "cpu") return eSort::CPU;
		if (text == "memory") return eSort::MEMORY;
		if (text == "modules" || text == "bricks") return eSort::MODULES;
		return std::nullopt;
	}

	inline std::string Lower(std::string_view text) {
		std::string out(text);
		std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return out;
	}

	// The builds grouped by their combination; builds without modules are left out
	inline std::vector<Assembly> Group(const std::vector<IUgcLookup::UgcEntry>& builds, const std::map<uint32_t, ModuleInfo>& modules) {
		std::map<std::string, Assembly> byKey;
		for (const auto& build : builds) {
			auto key = UgcModularKey::Normalize(build.detail);
			if (key.empty()) continue;
			auto& assembly = byKey[key];
			if (assembly.key.empty()) {
				assembly.key = key;
				assembly.lots = UgcModularKey::Lots(build.detail);
				std::sort(assembly.lots.begin(), assembly.lots.end());
				assembly.lots.erase(std::unique(assembly.lots.begin(), assembly.lots.end()), assembly.lots.end());
				for (const auto lot : assembly.lots) {
					const auto it = modules.find(lot);
					if (it != modules.end() && it->second.buildType >= 0) {
						assembly.buildType = it->second.buildType;
						break;
					}
				}
			}
			assembly.builds.push_back(build.id);
			if (build.characterId != LWOOBJID_EMPTY) assembly.owners.insert(build.characterId);
		}
		// The combination's state is its icon's: made when any build of it is, else waiting, failed or empty
		std::map<LWOOBJID, const IUgcLookup::UgcEntry*> byId;
		for (const auto& build : builds) byId[build.id] = &build;
		std::vector<Assembly> out;
		for (auto& [_, assembly] : byKey) {
			std::sort(assembly.builds.rbegin(), assembly.builds.rend());
			bool made = false, waiting = false, failed = false;
			for (const auto id : assembly.builds) {
				const auto& build = *byId[id];
				if (build.state == IUgc::eProcessState::DONE && !made) {
					made = true;
					assembly.iconBuild = id;
				}
				waiting = waiting || build.state == IUgc::eProcessState::PENDING;
				if (build.state == IUgc::eProcessState::FAILED && !failed) {
					failed = true;
					assembly.error = build.error;
				}
			}
			if (!made) assembly.iconBuild = assembly.builds.front();
			const auto& newest = *byId[assembly.builds.front()];
			assembly.characterId = newest.characterId;
			assembly.characterName = newest.characterName;
			assembly.accountId = newest.accountId;
			assembly.accountName = newest.accountName;
			int64_t costAt = -1;
			for (const auto id : assembly.builds) {
				const auto& build = *byId[id];
				assembly.processedAt = std::max(assembly.processedAt, build.processedAt);
				if (build.processMs > 0 && build.processedAt > costAt) {
					costAt = build.processedAt;
					assembly.processMs = build.processMs;
					assembly.processCpuMs = build.processCpuMs;
					assembly.processMemoryKb = build.processMemoryKb;
				}
			}
			assembly.state = made ? IUgc::eProcessState::DONE : waiting ? IUgc::eProcessState::PENDING : failed ? IUgc::eProcessState::FAILED : IUgc::eProcessState::EMPTY;
			if (made) assembly.error.clear();
			out.push_back(std::move(assembly));
		}
		return out;
	}

	struct Filter {
		std::optional<int32_t> buildType;
		std::optional<IUgc::eProcessState> state;
		std::optional<std::set<LWOOBJID>> builds; // only assemblies using one of these builds (a database search's matches)
		std::string moduleText;                   // or: a module's name contains this (any case)
		std::optional<uint32_t> moduleLot;        // or: this module is in it
	};

	inline bool Matches(const Assembly& assembly, const Filter& filter, const std::map<uint32_t, ModuleInfo>& modules) {
		if (filter.buildType && assembly.buildType != *filter.buildType) return false;
		if (filter.state && assembly.state != *filter.state) return false;
		const bool searching = filter.builds || !filter.moduleText.empty() || filter.moduleLot;
		if (!searching) return true;
		if (filter.builds) {
			for (const auto id : assembly.builds) {
				if (filter.builds->contains(id)) return true;
			}
		}
		if (filter.moduleLot && std::find(assembly.lots.begin(), assembly.lots.end(), *filter.moduleLot) != assembly.lots.end()) return true;
		if (!filter.moduleText.empty()) {
			const auto wanted = Lower(filter.moduleText);
			for (const auto lot : assembly.lots) {
				const auto it = modules.find(lot);
				if (it != modules.end() && Lower(it->second.name).find(wanted) != std::string::npos) return true;
			}
		}
		return false;
	}

	// The first module's name (what "name" sorts by)
	inline std::string Name(const Assembly& assembly, const std::map<uint32_t, ModuleInfo>& modules) {
		for (const auto lot : assembly.lots) {
			const auto it = modules.find(lot);
			if (it != modules.end() && !it->second.name.empty()) return it->second.name;
		}
		return assembly.key;
	}

	// reverse: the sort's other direction (the order is flipped as a whole)
	inline void Sort(std::vector<Assembly>& list, eSort sort, const std::map<uint32_t, ModuleInfo>& modules, bool reverse = false) {
		// The most first, ties the newest first
		const auto most = [](auto x, auto y, const Assembly& a, const Assembly& b) { return x != y ? x > y : a.builds.front() > b.builds.front(); };
		std::stable_sort(list.begin(), list.end(), [&](const Assembly& a, const Assembly& b) {
			switch (sort) {
			case eSort::OLDEST: return a.builds.back() < b.builds.back();
			case eSort::REFERENCES: return most(a.builds.size(), b.builds.size(), a, b);
			case eSort::NAME: {
				const auto an = Lower(Name(a, modules)), bn = Lower(Name(b, modules));
				return an != bn ? an < bn : a.key < b.key;
			}
			case eSort::OWNER: {
				const auto an = Lower(a.characterName), bn = Lower(b.characterName);
				return an != bn ? an < bn : a.builds.front() > b.builds.front();
			}
			case eSort::MADE: return most(a.processedAt, b.processedAt, a, b);
			case eSort::SLOWEST: return most(a.processMs, b.processMs, a, b);
			case eSort::CPU: return most(a.processCpuMs, b.processCpuMs, a, b);
			case eSort::MEMORY: return most(a.processMemoryKb, b.processMemoryKb, a, b);
			case eSort::MODULES: return most(a.lots.size(), b.lots.size(), a, b);
			default: return a.builds.front() > b.builds.front();
			}
		});
		if (reverse) std::reverse(list.begin(), list.end());
	}
}
