#include "GameMessageDecoder.h"

#include <functional>
#include <map>
#include <string>
#include <utility>

#include "GameMessages.h"
#include "GameMessageHandler.h"
#include "ActivityMessages.h"
#include "InventoryMessages.h"
#include "ObjectMessages.h"
#include "SkillMessages.h"
#include "master/MessageCapture.h"

namespace {
	using json = nlohmann::json;
	using Decoder = std::function<std::optional<json>(RakNet::BitStream&)>;

	// Object IDs as strings: they don't fit in a JavaScript number
	std::string Id(LWOOBJID id) { return std::to_string(id); }
	json Point(const NiPoint3& point) { return json::array({ point.x, point.y, point.z }); }
	json Rotation(const NiQuaternion& rotation) { return json::array({ rotation.x, rotation.y, rotation.z, rotation.w }); }
	// A skill's behavior data, which is its own bit stream
	json Bytes(const std::string& bytes) { return MessageCapture::ToHex(bytes); }

	// Reads T with its Deserialize, then lists its fields with `fields`
	template<typename T>
	Decoder Make(std::function<void(const T&, json&)> fields) {
		return [fields](RakNet::BitStream& stream) -> std::optional<json> {
			T message;
			if (!message.Deserialize(stream)) return std::nullopt;
			json out = json::object();
			fields(message, out);
			return out;
		};
	}

	// (to server, message ID) -> decoder
	const std::map<std::pair<bool, MessageType::Game>, Decoder>& Decoders() {
		using namespace GameMessages;
		using enum MessageType::Game;
		static const std::map<std::pair<bool, MessageType::Game>, Decoder> decoders{
			// Sent by the client
			{ { true, REQUEST_USE }, Make<RequestUse>([](const RequestUse& m, json& j) {
				j = { {"bIsMultiInteractUse", m.bIsMultiInteractUse}, {"multiInteractID", m.multiInteractID},
					{"multiInteractType", m.multiInteractType}, {"object", Id(m.object)}, {"secondary", m.secondary} };
			}) },
			{ { true, REQUEST_SERVER_OBJECT_INFO }, Make<RequestServerObjectInfo>([](const RequestServerObjectInfo& m, json& j) {
				j = { {"bVerbose", m.bVerbose}, {"clientId", Id(m.clientId)}, {"targetForReport", Id(m.targetForReport)} };
			}) },
			{ { true, SHOOTING_GALLERY_FIRE }, Make<ShootingGalleryFire>([](const ShootingGalleryFire& m, json& j) {
				j = { {"target", Point(m.target)}, {"rotation", Rotation(m.rotation)} };
			}) },
			{ { true, PICKUP_ITEM }, Make<PickupItem>([](const PickupItem& m, json& j) {
				j = { {"lootID", Id(m.lootID)}, {"lootOwnerID", Id(m.lootOwnerID)} };
			}) },
			{ { true, START_SKILL }, Make<StartSkill>([](const StartSkill& m, json& j) {
				j = { {"bUsedMouse", m.bUsedMouse}, {"consumableItemID", Id(m.consumableItemID)}, {"fCasterLatency", m.fCasterLatency},
					{"iCastType", m.iCastType}, {"lastClickedPosit", Point(m.lastClickedPosit)}, {"optionalOriginatorID", Id(m.optionalOriginatorID)},
					{"optionalTargetID", Id(m.optionalTargetID)}, {"originatorRot", Rotation(m.originatorRot)}, {"sBitStream", Bytes(m.sBitStream)},
					{"skillID", m.skillID}, {"uiSkillHandle", m.uiSkillHandle} };
			}) },
			{ { true, SYNC_SKILL }, Make<SyncSkill>([](const SyncSkill& m, json& j) {
				j = { {"bDone", m.bDone}, {"sBitStream", Bytes(m.sBitStream)}, {"uiBehaviorHandle", m.uiBehaviorHandle}, {"uiSkillHandle", m.uiSkillHandle} };
			}) },
			{ { true, REQUEST_SERVER_PROJECTILE_IMPACT }, Make<RequestServerProjectileImpact>([](const RequestServerProjectileImpact& m, json& j) {
				j = { {"i64LocalID", Id(m.i64LocalID)}, {"i64TargetID", Id(m.i64TargetID)}, {"sBitStream", Bytes(m.sBitStream)} };
			}) },
			// Sent to the client
			{ { false, ECHO_START_SKILL }, Make<EchoStartSkill>([](const EchoStartSkill& m, json& j) {
				j = { {"bUsedMouse", m.bUsedMouse}, {"fCasterLatency", m.fCasterLatency}, {"iCastType", m.iCastType},
					{"lastClickedPosit", Point(m.lastClickedPosit)}, {"optionalOriginatorID", Id(m.optionalOriginatorID)},
					{"optionalTargetID", Id(m.optionalTargetID)}, {"originatorRot", Rotation(m.originatorRot)}, {"sBitStream", Bytes(m.sBitStream)},
					{"skillID", m.skillID}, {"uiSkillHandle", m.uiSkillHandle} };
			}) },
			{ { false, ECHO_SYNC_SKILL }, Make<EchoSyncSkill>([](const EchoSyncSkill& m, json& j) {
				j = { {"bDone", m.bDone}, {"sBitStream", Bytes(m.sBitStream)}, {"uiBehaviorHandle", m.uiBehaviorHandle}, {"uiSkillHandle", m.uiSkillHandle} };
			}) },
			{ { false, DO_CLIENT_PROJECTILE_IMPACT }, Make<DoClientProjectileImpact>([](const DoClientProjectileImpact& m, json& j) {
				j = { {"i64OrgID", Id(m.i64OrgID)}, {"i64OwnerID", Id(m.i64OwnerID)}, {"i64TargetID", Id(m.i64TargetID)}, {"sBitStream", Bytes(m.sBitStream)} };
			}) },
		};
		return decoders;
	}
}

namespace GameMessageDecoder {
	bool CanDecode(MessageType::Game messageId, bool toServer) {
		return Decoders().contains({ toServer, messageId });
	}

	std::optional<nlohmann::json> Decode(MessageType::Game messageId, bool toServer, RakNet::BitStream& payload) {
		const auto it = Decoders().find({ toServer, messageId });
		if (it == Decoders().end()) return std::nullopt;
		return it->second(payload);
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
