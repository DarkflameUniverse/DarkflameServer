#ifndef __CAPTUREPROPERTY__H__
#define __CAPTUREPROPERTY__H__

#include <cstdint>
#include <functional>
#include <vector>

#include "CaptureBundle.h"
#include "json.hpp"

/**
 * A property as a packet capture saw it, for the capture viewer and the World 3D replay: per world (zone, instance
 * and clone) the models placed on it over time and what the property said about itself.
 *
 * Everything comes from the captured packets:
 *  - models: the replica constructions, serializations and destructions of objects with a model component (and brick
 *    built models, LOT 14), each with its position and rotation per time span (from its physics component);
 *  - placed: the server's PlaceModelResponse saying a model was placed, with the model that was then made at that
 *    position; removed: a model's destruction, with the reason from the DeleteModelFromClient that asked for it;
 *    moved: a model's serialization with a new position;
 *  - info: each DownloadPropertyData (owner, name, description, access, moderation, reputation) and each
 *    GetModelsOnProperty (how many models the property had);
 *  - behaviors: the behavior messages (ControlBehaviors and the others named for behaviors) and the game messages
 *    sent to or from a model while it stood there.
 * A world is listed when a property message was captured on it or a model was constructed there.
 */
namespace CaptureProperty {
	// The fields ReplicaDecoder read from record `index` (nullptr when it isn't a replica packet)
	using ReplicaFields = std::function<const nlohmann::json*(size_t index)>;

	/**
	 * {worlds: [{zone, instance, clone, t, info: [...], counts: [...], models: [...], events: [...]}]}; times are seconds
	 * from `startUs`, `i` the record's index. A model: {object, lot, spawner, ugcId, behaviors, spans: [{from, to, i,
	 * position, rotation}]} (`to` null: still there when the capture ended).
	 */
	nlohmann::json Build(const std::vector<CaptureBundle::Record>& records, int64_t startUs, const ReplicaFields& replica);
}

#endif  //!__CAPTUREPROPERTY__H__
