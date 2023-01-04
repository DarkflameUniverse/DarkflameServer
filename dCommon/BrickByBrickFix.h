#pragma once

#include <cstdint>

namespace BrickByBrickFix {
	/**
	 * @brief Deletes all broken BrickByBrick models that have invalid XML
	 *
	 * @return The number of BrickByBrick models that were truncated
	 */
	uint32_t TruncateBrokenBrickByBrickXml();

	/**
	 * @brief Updates all BrickByBrick models in the database to be
	 * in the sd0 format as opposed to a zlib compressed format.
	 *
	 * @return The number of BrickByBrick models that were updated
	 */
	uint32_t UpdateBrickByBrickModelsToSd0();
};
