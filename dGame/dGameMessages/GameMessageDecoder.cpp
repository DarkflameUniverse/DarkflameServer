#include "GameMessageDecoder.h"

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Amf3.h"
#include "GameMessages.h"
#include "GameMessageHandler.h"
#include "GeneralUtils.h"
#include "LDFFormat.h"
#include "magic_enum.hpp"
#include "PacketJson.h"

#include "ActivityMessages.h"
#include "BuildingMessages.h"
#include "CombatMessages.h"
#include "EffectsMessages.h"
#include "InventoryMessages.h"
#include "MissionMessages.h"
#include "MovementMessages.h"
#include "ObjectMessages.h"
#include "PetMessages.h"
#include "PlayerMessages.h"
#include "PropertyMessages.h"
#include "QuickBuildMessages.h"
#include "RacingMessages.h"
#include "SkillMessages.h"
#include "TradeMessages.h"
#include "VendorMessages.h"
#include "ZoneMessages.h"

// Enums the message structs only forward declare
#include "BaseCombatAIComponent.h" // AiState
#include "BehaviorSlot.h"
#include "eAnimationFlags.h"
#include "eCinematicEvent.h"
#include "eControlScheme.h"
#include "eHelpType.h"
#include "eInventoryType.h"
#include "eKillType.h"
#include "eMatchUpdate.h"
#include "eMissionLockState.h"
#include "eMissionState.h"
#include "eObjectWorldState.h"
#include "ePetAbilityType.h"
#include "ePetTamingNotifyType.h"
#include "eQuickBuildFailReason.h"
#include "eQuickBuildState.h"
#include "eRacingClientNotificationType.h"
#include "eReponseMoveItemBetweenInventoryTypeCode.h"
#include "eStateChangeType.h"
#include "eTerminateType.h"
#include "eUnequippableActiveType.h"
#include "eUseItemResponse.h"
#include "eVendorTransactionResult.h"

namespace {
	using json = nlohmann::json;

	// How a member becomes JSON (PacketJson.h, and the game's own types here). Every member type of a message struct
	// needs one: the build fails otherwise.
	using PacketJson::ToJson;
	using PacketJson::Hex;

	json ToJson(const Brick& brick) { return json{ {"designerID", brick.designerID}, {"materialID", brick.materialID} }; }

	json ToJson(const AMFBaseValue* value) {
		if (!value) return nullptr;
		switch (value->GetValueType()) {
		case eAmf::Null:
		case eAmf::Undefined: return nullptr;
		case eAmf::True: return true;
		case eAmf::False: return false;
		case eAmf::Integer: return static_cast<const AMFIntValue*>(value)->GetValue();
		case eAmf::Double: return static_cast<const AMFDoubleValue*>(value)->GetValue();
		case eAmf::String: return static_cast<const AMFStringValue*>(value)->GetValue();
		case eAmf::Array: {
			const auto* array = static_cast<const AMFArrayValue*>(value);
			json out = json::object();
			for (const auto& [key, item] : array->GetAssociative()) out[key] = ToJson(item.get());
			if (!array->GetDense().empty()) {
				json dense = json::array();
				for (const auto& item : array->GetDense()) dense.push_back(ToJson(item.get()));
				out["[]"] = dense;
			}
			return out;
		}
		default: return "(AMF type " + std::to_string(static_cast<int>(value->GetValueType())) + ")";
		}
	}
	json ToJson(const AMFArrayValue& value) { return ToJson(static_cast<const AMFBaseValue*>(&value)); }
	json ToJson(const std::unique_ptr<AMFArrayValue>& value) { return ToJson(static_cast<const AMFBaseValue*>(value.get())); }

	template<typename T> json ToJson(const std::optional<T>& value);
	template<typename T> json ToJson(const std::vector<T>& values);
	template<typename A, typename B> json ToJson(const std::pair<A, B>& value);

	enum class eDirection { TO_SERVER, TO_CLIENT };

	struct Read {
		std::optional<json> fields;
		uint32_t unreadBits{};
	};
	using Reader = Read(*)(RakNet::BitStream&);

	struct Entry {
		MessageType::Game id;
		eDirection direction;
		const char* structName;
		Reader read;
	};

	template<typename T> Read ReadWith(RakNet::BitStream& stream);

#include "GameMessageFields.inc"

	template<typename T> json ToJson(const std::optional<T>& value) { return value ? ToJson(*value) : json(nullptr); }
	template<typename T> json ToJson(const std::vector<T>& values) {
		if constexpr (std::is_same_v<T, uint8_t>) {
			return json{ {"hex", Hex(std::string(values.begin(), values.end()))} };
		} else {
			json out = json::array();
			for (const auto& value : values) out.push_back(ToJson(value));
			return out;
		}
	}
	template<typename A, typename B> json ToJson(const std::pair<A, B>& value) { return json::array({ ToJson(value.first), ToJson(value.second) }); }

	// Reads the message with its own Deserialize and lists its members
	template<typename T> Read ReadWith(RakNet::BitStream& stream) {
		T message;
		if (!message.Deserialize(stream)) return {};
		return { ToJson(message), stream.GetNumberOfUnreadBits() };
	}

	// (to server, ID) -> the struct that reads it
	const std::map<std::pair<bool, MessageType::Game>, const Entry*>& Index() {
		static const auto index = [] {
			std::map<std::pair<bool, MessageType::Game>, const Entry*> out;
			// The struct the server reads a message with, for its direction...
			for (const auto& entry : Entries()) out.emplace(std::pair{ entry.direction == eDirection::TO_SERVER, entry.id }, &entry);
			// ...and for the other direction when no struct is only sent that way (the layout is the same both ways)
			for (const auto& entry : Entries()) out.emplace(std::pair{ entry.direction != eDirection::TO_SERVER, entry.id }, &entry);
			return out;
		}();
		return index;
	}

	const Entry* Find(MessageType::Game messageId, bool toServer) {
		const auto it = Index().find({ toServer, messageId });
		return it == Index().end() ? nullptr : it->second;
	}
}

namespace GameMessageDecoder {
	bool CanDecode(MessageType::Game messageId, bool toServer) {
		const auto* entry = Find(messageId, toServer);
		return entry && entry->read;
	}

	bool HasStruct(MessageType::Game messageId) { return Find(messageId, true) != nullptr; }

	std::optional<nlohmann::json> Decode(MessageType::Game messageId, bool toServer, RakNet::BitStream& payload) {
		const auto* entry = Find(messageId, toServer);
		if (!entry || !entry->read) return std::nullopt;
		auto read = entry->read(payload);
		if (!read.fields) return std::nullopt;
		// Whole bytes the struct didn't read: the message has more than the struct knows (padding is under a byte)
		if (read.unreadBits >= 8) (*read.fields)["(unread bits)"] = read.unreadBits;
		return read.fields;
	}

	std::vector<MessageType::Game> Decodable() {
		std::vector<MessageType::Game> out;
		for (const auto& entry : Entries()) {
			if (entry.read) out.push_back(entry.id);
		}
		return out;
	}

	std::optional<bool> RoundTripReceived(MessageType::Game messageId, RakNet::BitStream& payload) {
		auto message = GameMessageHandler::CreateReceived(messageId);
		if (!message) return std::nullopt;
		const auto start = payload.GetReadOffset();
		if (!message->Deserialize(payload)) return std::nullopt;
		const auto read = payload.GetReadOffset() - start;
		// A whole byte or more left over: the struct stopped short of fields the message has
		if (payload.GetNumberOfUnreadBits() >= 8) return false;
		RakNet::BitStream written;
		message->Serialize(written);
		if (written.GetNumberOfBitsUsed() != read) return false;
		// The bits read, compared with the bits written
		payload.SetReadOffset(start);
		for (uint32_t bit = 0; bit < read; bit++) {
			bool original{}, again{};
			if (!payload.Read(original) || !written.Read(again) || original != again) return false;
		}
		return true;
	}
}
