#pragma once

/**
 * "Today's Top Properties": the four slots of the game's news screen, one per small property world (see
 * HotPropertySlots.h). Staff with feature_properties either let the panel show the four approved public properties
 * with the most reputation across every property world (full auto), or choose per slot: its location (any property
 * world) and what it shows there: the location's approved public property with the most reputation (auto), a
 * property they pick, or nothing. No property is shown in two slots. The game reads the choice when a player opens
 * the news screen.
 */
namespace FeaturedProperties {
	void RegisterRoutes();
};
