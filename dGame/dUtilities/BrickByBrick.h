#ifndef BRICKBYBRICK_H
#define BRICKBYBRICK_H

#include "dCommonVars.h"
#include "IBbbAutosave.h"
#include "LDFFormat.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class Entity;
class InventoryComponent;

namespace GameMessages {
	struct MoveInventoryBatch;
	struct FetchModelMetadataResponse;
}

// Brick by brick building and the model items it works on, as the 1.10.64 client expects them (docs/BuildWorkflow.md).
//
// While a player builds, MODELS_IN_BBB holds the models they opened (their original items) and BRICKS_IN_BBB the bricks
// they took out of their backpack. A save turns the build into new models on the property and uses up the originals.
// Anything else that ends a build (leaving without saving, a disconnect or a crash) goes through RecoverUnfinishedBuild:
// the client's last autosave is rebuilt into models, or the originals go back to MODELS, so a model is never lost.
namespace BrickByBrick {
	// A brick built model as an item (in MODELS, VAULT_MODELS or MODELS_IN_BBB)
	constexpr LOT MODEL_ITEM_LOT = 6662;
	// A brick built model placed in the world
	constexpr LOT MODEL_OBJECT_LOT = 14;

	// DeleteModelFromClient's reason
	enum class eDeleteReason : int32_t {
		PICKING_MODEL_UP = 0,           // into MODELS, equipped to carry it
		RETURNING_MODEL_TO_INVENTORY,   // into MODELS
		BREAKING_MODEL_APART,           // into MODELS; the client then opens it with BBBLoadItemRequest
	};

	// PlaceModelResponse's response
	constexpr int32_t PLACE_MODEL_PLACED = 14;
	constexpr int32_t PLACE_MODEL_REMOVED = 16;

	// ---- Rules with no game state (unit tested) ----

	// Whether a SetBBBAutosave / BBBSaveRequest payload holds no model: nothing, or only the sd0 header the client sends
	// to clear its autosave
	bool IsEmptyModel(std::string_view sd0);

	// What taking a model off the property does with it
	struct ModelRemoval {
		bool equip{};            // the player carries it
		bool notifyPostDelete{}; // HandleUGCEquipPostDeleteBasedOnEditMode tells the client which item it became
	};
	ModelRemoval PlanModelRemoval(int32_t reason);

	// What ending a build without a save does
	struct Recovery {
		bool rebuild{};                  // the autosave is rebuilt into models
		std::vector<LWOOBJID> consume;   // originals the rebuilt models replace (only once a model was rebuilt)
		std::vector<LWOOBJID> giveBack;  // MODELS_IN_BBB items that go back to MODELS
	};
	Recovery PlanRecovery(const std::vector<LWOOBJID>& modelsInBbb, const std::optional<IBbbAutosave::Info>& autosave);

	// The config of a brick built model item (6662) and of its placed object (14)
	LwoNameValue ModelItemConfig(LWOOBJID blueprintId, LWOOBJID userModelId, const std::string& behaviors = "");

	// An object id kept in a config (blueprintid, userModelID), or LWOOBJID_EMPTY
	LWOOBJID ConfigObjectId(const LwoNameValue& config, const std::u16string& key);

	// A model's behavior ids from its userModelBehaviors config ("id,id,id,id,id"): always 5, missing ones 0 (live
	// always sent 5 in FetchModelMetadataResponse)
	std::vector<LWOOBJID> BehaviorIds(std::string_view behaviors);

	// BlueprintMetadata's brick list: every brick's LOT followed by ':' ("66:" for one 1x4 plate), as live sent it
	std::u16string BrickList(const std::vector<LOT>& brickLots);

	// ---- Handlers ----

	// FetchModelMetadataRequest: fills in the UG data (and, for a brick built model, the blueprint data) of the model
	// ugId: one of the player's model items (its subkey), else a model placed on any property. Neither: both left out.
	void FillModelMetadata(Entity& player, LWOOBJID ugId, GameMessages::FetchModelMetadataResponse& response);

	// BBBLoadItemRequest: moves the model item from MODELS to MODELS_IN_BBB keeping its object id (as live did).
	// Returns the id in MODELS_IN_BBB, or LWOOBJID_EMPTY when the player has no such model.
	LWOOBJID LoadModel(Entity& player, LWOOBJID itemId);

	// BBBSaveRequest
	void Save(Entity& player, LWOOBJID localId, const std::string& sd0);

	// SetBBBAutosave: the client's quick save
	void Autosave(Entity& player, const std::string& sd0);

	// UnUseBBBModel: a model the player opened goes back where it came from (the import tool's undo, or a model that
	// could not be loaded). With a world transform it goes back on the property there, otherwise to MODELS.
	void ReturnModel(Entity& player, LWOOBJID itemId, bool hasWorldTransform, const NiPoint3& position, const NiQuaternion& rotation);

	// MoveInventoryBatch between BRICKS and BRICKS_IN_BBB
	void MoveBricks(Entity& player, const GameMessages::MoveInventoryBatch& move);

	// ActivateBrickMode leaving brick mode
	void EndSession(Entity& player);

	// PlayerLoaded
	void OnPlayerLoaded(Entity& player);

	// Resolves a build that ended without a save; returns how many models were rebuilt from the autosave
	uint32_t RecoverUnfinishedBuild(Entity& player);

	// Whether an inventory bag saved as a BBB bag loads into the normal one (MODELS_IN_BBB -> MODELS,
	// BRICKS_IN_BBB -> BRICKS): a build never survives a reload, its items do.
	uint32_t InventoryToLoadInto(uint32_t savedType);
};

#endif //!BRICKBYBRICK_H
