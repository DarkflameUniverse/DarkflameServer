#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "BehaviorStates.h"

/**
 * The property model behavior blocks the server plays (Strip::ProcNormalAction in dGame/dPropertyBehaviors/Strip.cpp),
 * by the Type the client saves for them in the behavior XML. The dashboard's behavior player serves the same tables to
 * the browser, so the two play the same blocks the same way. A block missing here is logged and skipped by both.
 */
namespace PropertyBehaviorActions {
	// Trigger blocks: a strip waits on its trigger until the model is interacted with, attacked, or hears the phrase
	constexpr std::string_view ON_INTERACT = "OnInteract";
	constexpr std::string_view ON_ATTACK = "OnAttack";
	constexpr std::string_view ON_CHAT = "OnChat";
	constexpr std::array TRIGGERS{ ON_INTERACT, ON_ATTACK, ON_CHAT };

	// Moves the block's distance along a world axis at the model's speed
	struct Move {
		std::string_view type;
		char axis; // 'x', 'y' or 'z'
		float sign;
	};
	constexpr std::array MOVES{
		Move{ "MoveRight", 'x', 1.0f }, Move{ "MoveLeft", 'x', -1.0f },
		Move{ "FlyUp", 'y', 1.0f }, Move{ "FlyDown", 'y', -1.0f },
		Move{ "MoveForward", 'z', 1.0f }, Move{ "MoveBackward", 'z', -1.0f },
	};

	// Blocks that make an object: spawns make one enemy at the model; drops give every player one powerup per unit of
	// the block's value
	struct LotBlock {
		std::string_view type;
		int32_t lot;
	};
	constexpr std::array SPAWNS{
		LotBlock{ "SpawnStromling", 10495 }, // Stromling property
		LotBlock{ "SpawnPirate", 10497 },    // Maelstrom Pirate property
		LotBlock{ "SpawnRonin", 10498 },     // Dark Ronin property
	};
	constexpr std::array DROPS{
		LotBlock{ "DropImagination", 935 }, // 1 Imagination powerup
		LotBlock{ "DropHealth", 177 },      // 1 Life powerup
		LotBlock{ "DropArmor", 6431 },      // 1 Armor powerup
	};

	// Switches every strip of the behavior to another state's strips
	struct StateChange {
		std::string_view type;
		BehaviorState state;
	};
	constexpr std::array STATE_CHANGES{
		StateChange{ "ChangeStateHome", BehaviorState::HOME_STATE },
		StateChange{ "ChangeStateCircle", BehaviorState::CIRCLE_STATE },
		StateChange{ "ChangeStateSquare", BehaviorState::SQUARE_STATE },
		StateChange{ "ChangeStateDiamond", BehaviorState::DIAMOND_STATE },
		StateChange{ "ChangeStateTriangle", BehaviorState::TRIANGLE_STATE },
		StateChange{ "ChangeStateStar", BehaviorState::STAR_STATE },
	};

	// The rest, each with its own handling
	constexpr std::string_view SET_SPEED = "SetSpeed";
	constexpr std::string_view SMASH = "Smash";
	constexpr std::string_view UNSMASH = "UnSmash";
	constexpr std::string_view WAIT = "Wait";
	constexpr std::string_view CHAT = "Chat";
	constexpr std::string_view PRIVATE_MESSAGE = "PrivateMessage";
	constexpr std::string_view PLAY_SOUND = "PlaySound";
	constexpr std::string_view RESTART = "Restart";
	constexpr std::array OTHERS{ SET_SPEED, SMASH, UNSMASH, WAIT, CHAT, PRIVATE_MESSAGE, PLAY_SOUND, RESTART };

	// The speed models move at until a Set Speed block (ModelComponent)
	constexpr float DEFAULT_SPEED = 3.0f;

	// The table entry for a block type, or nullptr
	template<typename Table>
	constexpr const typename Table::value_type* Find(const Table& table, std::string_view type) {
		for (const auto& entry : table) if (entry.type == type) return &entry;
		return nullptr;
	}
}
