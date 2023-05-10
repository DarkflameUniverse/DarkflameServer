#pragma once

#include "dCommonVars.h"

/**
 * Database information that specifies a pet
 */
struct DatabasePet
{
	/**
	 * The lot of this pet
	 */
	LOT lot = LOT_NULL;

	/**
	 * The name of the pet
	 */
	std::string name;

	/**
	 * The current moderation state, see PetComponent for more info
	 */
	int32_t moderationState = 0;
};

const DatabasePet DATABASE_PET_INVALID;
