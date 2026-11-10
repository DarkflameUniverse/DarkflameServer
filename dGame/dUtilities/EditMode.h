#pragma once

#include "eGameMasterLevel.h"

/**
 * Edit mode (docs/EditMode.md): a private instance of a zone whose world server constructs none of the zone's level
 * objects, spawners or triggers, for editing the zone in the game client. Only the plumbing is here; nothing starts
 * such an instance yet.
 */
namespace EditMode {
	// Whether an account may be in an edit mode instance
	inline bool MayEnter(const eGameMasterLevel accountLevel, const eGameMasterLevel requiredLevel) {
		return accountLevel != eGameMasterLevel::CIVILIAN && accountLevel >= requiredLevel;
	}

	// The account GM level an edit mode instance lets in: editor_gm_level (9 unless set)
	eGameMasterLevel RequiredLevel();
}
