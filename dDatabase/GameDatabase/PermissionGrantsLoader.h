#pragma once

#include <ctime>
#include <memory>

#include "Database.h"
#include "PermissionGrants.h"

namespace PermissionGrants {
	// The grants in force now of an account and, when characterId isn't 0, one of its characters (charinfo ID)
	inline std::shared_ptr<const Held> Load(uint32_t accountId, int64_t characterId = 0) {
		auto held = std::make_shared<Held>();
		if (accountId == 0) return held;
		for (auto& row : Database::Get()->GetActivePermissionGrants(accountId, characterId, static_cast<int64_t>(std::time(nullptr)))) {
			held->Add(row.kind, std::move(row.name), row.deny, row.expiresAt);
		}
		return held;
	}
}
