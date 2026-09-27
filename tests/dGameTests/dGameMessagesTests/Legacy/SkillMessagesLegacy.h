#ifndef SKILLMESSAGESLEGACY_H
#define SKILLMESSAGESLEGACY_H

// FROZEN ORACLE - DO NOT EDIT.
// Verbatim copies of the one-off skill and projectile message classes that SkillMessages.h replaced
// (dGame/dGameMessages/{StartSkill,EchoStartSkill,SyncSkill,EchoSyncSkill,RequestServerProjectileImpact,
// DoClientProjectileImpact}.h) and of GameMessages::SendAddSkill / SendRemoveSkill
// (dGame/dGameMessages/GameMessages.cpp), branched from origin/main 129199e4. Only the namespace changed.
// Each class's Serialize writes the message ID and then the payload; the old senders wrote the CLIENT/GAME_MSG
// header and the target object ID before it. The Read* function is the read sequence of the replaced switch case.

#include "LegacyPacketMacros.h"
#include "BehaviorSlot.h"
#include "BitStream.h"
#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "Entity.h"
#include "Game.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "ServiceType.h"

#include <cstdint>
#include <string>

namespace LegacyGameMessages {
/**
 * Same as sync skill but with different network options. An echo down to other clients that need to play the skill.
 */
class StartSkill {
public:
	StartSkill() {
		bUsedMouse = false;
		consumableItemID = LWOOBJID_EMPTY;
		fCasterLatency = 0.0f;
		iCastType = 0;
		lastClickedPosit = NiPoint3Constant::ZERO;
		optionalTargetID = LWOOBJID_EMPTY;
		originatorRot = QuatUtils::IDENTITY;
		uiSkillHandle = 0;
	}

	StartSkill(LWOOBJID _optionalOriginatorID, std::string _sBitStream, TSkillID _skillID, bool _bUsedMouse = false, LWOOBJID _consumableItemID = LWOOBJID_EMPTY, float _fCasterLatency = 0.0f, int32_t _iCastType = 0, NiPoint3 _lastClickedPosit = NiPoint3Constant::ZERO, LWOOBJID _optionalTargetID = LWOOBJID_EMPTY, NiQuaternion _originatorRot = QuatUtils::IDENTITY, uint32_t _uiSkillHandle = 0) {
		bUsedMouse = _bUsedMouse;
		consumableItemID = _consumableItemID;
		fCasterLatency = _fCasterLatency;
		iCastType = _iCastType;
		lastClickedPosit = _lastClickedPosit;
		optionalOriginatorID = _optionalOriginatorID;
		optionalTargetID = _optionalTargetID;
		originatorRot = _originatorRot;
		sBitStream = _sBitStream;
		skillID = _skillID;
		uiSkillHandle = _uiSkillHandle;
	}

	StartSkill(RakNet::BitStream& stream) : StartSkill() {
		Deserialize(stream);
	}

	~StartSkill() {
	}

	void Serialize(RakNet::BitStream& stream) {
		stream.Write(MessageType::Game::START_SKILL);

		stream.Write(bUsedMouse);

		stream.Write(consumableItemID != LWOOBJID_EMPTY);
		if (consumableItemID != LWOOBJID_EMPTY) stream.Write(consumableItemID);

		stream.Write(fCasterLatency != 0.0f);
		if (fCasterLatency != 0.0f) stream.Write(fCasterLatency);

		stream.Write(iCastType != 0);
		if (iCastType != 0) stream.Write(iCastType);

		stream.Write(lastClickedPosit != NiPoint3Constant::ZERO);
		if (lastClickedPosit != NiPoint3Constant::ZERO) stream.Write(lastClickedPosit);

		stream.Write(optionalOriginatorID);

		stream.Write(optionalTargetID != LWOOBJID_EMPTY);
		if (optionalTargetID != LWOOBJID_EMPTY) stream.Write(optionalTargetID);

		stream.Write(originatorRot != QuatUtils::IDENTITY);
		if (originatorRot != QuatUtils::IDENTITY) stream.Write(originatorRot);

		uint32_t sBitStreamLength = sBitStream.length();
		stream.Write(sBitStreamLength);
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			stream.Write(sBitStream[k]);
		}

		stream.Write(skillID);

		stream.Write(uiSkillHandle != 0);
		if (uiSkillHandle != 0) stream.Write(uiSkillHandle);
	}

	bool Deserialize(RakNet::BitStream& stream) {
		stream.Read(bUsedMouse);

		bool consumableItemIDIsDefault{};
		stream.Read(consumableItemIDIsDefault);
		if (consumableItemIDIsDefault != 0) stream.Read(consumableItemID);

		bool fCasterLatencyIsDefault{};
		stream.Read(fCasterLatencyIsDefault);
		if (fCasterLatencyIsDefault != 0) stream.Read(fCasterLatency);

		bool iCastTypeIsDefault{};
		stream.Read(iCastTypeIsDefault);
		if (iCastTypeIsDefault != 0) stream.Read(iCastType);

		bool lastClickedPositIsDefault{};
		stream.Read(lastClickedPositIsDefault);
		if (lastClickedPositIsDefault != 0) stream.Read(lastClickedPosit);

		stream.Read(optionalOriginatorID);

		bool optionalTargetIDIsDefault{};
		stream.Read(optionalTargetIDIsDefault);
		if (optionalTargetIDIsDefault != 0) stream.Read(optionalTargetID);

		bool originatorRotIsDefault{};
		stream.Read(originatorRotIsDefault);
		if (originatorRotIsDefault != 0) stream.Read(originatorRot);

		uint32_t sBitStreamLength{};
		stream.Read(sBitStreamLength);
		if (sBitStreamLength > MAX_MESSAGE_LENGTH) return false;
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			unsigned char character;
			stream.Read(character);
			sBitStream.push_back(character);
		}

		stream.Read(skillID);

		bool uiSkillHandleIsDefault{};
		stream.Read(uiSkillHandleIsDefault);
		if (uiSkillHandleIsDefault != 0) stream.Read(uiSkillHandle);

		return true;
	}

	bool bUsedMouse = false;
	LWOOBJID consumableItemID{};
	float fCasterLatency{};
	int32_t iCastType{};
	NiPoint3 lastClickedPosit{};
	LWOOBJID optionalOriginatorID{};
	LWOOBJID optionalTargetID{};
	NiQuaternion originatorRot = QuatUtils::IDENTITY;
	std::string sBitStream = "";
	TSkillID skillID = 0;
	uint32_t uiSkillHandle = 0;
};

/*  Same as start skill but with different network options. An echo down to other clients that need to play the skill. */
class EchoStartSkill {
public:
	EchoStartSkill() {
		bUsedMouse = false;
		fCasterLatency = 0.0f;
		iCastType = 0;
		lastClickedPosit = NiPoint3Constant::ZERO;
		optionalTargetID = LWOOBJID_EMPTY;
		originatorRot = QuatUtils::IDENTITY;
		uiSkillHandle = 0;
	}

	EchoStartSkill(LWOOBJID _optionalOriginatorID, std::string _sBitStream, TSkillID _skillID, bool _bUsedMouse = false, float _fCasterLatency = 0.0f, int32_t _iCastType = 0, NiPoint3 _lastClickedPosit = NiPoint3Constant::ZERO, LWOOBJID _optionalTargetID = LWOOBJID_EMPTY, NiQuaternion _originatorRot = QuatUtils::IDENTITY, uint32_t _uiSkillHandle = 0) {
		bUsedMouse = _bUsedMouse;
		fCasterLatency = _fCasterLatency;
		iCastType = _iCastType;
		lastClickedPosit = _lastClickedPosit;
		optionalOriginatorID = _optionalOriginatorID;
		optionalTargetID = _optionalTargetID;
		originatorRot = _originatorRot;
		sBitStream = _sBitStream;
		skillID = _skillID;
		uiSkillHandle = _uiSkillHandle;
	}

	EchoStartSkill(RakNet::BitStream& stream) : EchoStartSkill() {
		Deserialize(stream);
	}

	~EchoStartSkill() {
	}

	void Serialize(RakNet::BitStream& stream) {
		stream.Write(MessageType::Game::ECHO_START_SKILL);

		stream.Write(bUsedMouse);

		stream.Write(fCasterLatency != 0.0f);
		if (fCasterLatency != 0.0f) stream.Write(fCasterLatency);

		stream.Write(iCastType != 0);
		if (iCastType != 0) stream.Write(iCastType);

		stream.Write(lastClickedPosit != NiPoint3Constant::ZERO);
		if (lastClickedPosit != NiPoint3Constant::ZERO) stream.Write(lastClickedPosit);

		stream.Write(optionalOriginatorID);

		stream.Write(optionalTargetID != LWOOBJID_EMPTY);
		if (optionalTargetID != LWOOBJID_EMPTY) stream.Write(optionalTargetID);

		stream.Write(originatorRot != QuatUtils::IDENTITY);
		if (originatorRot != QuatUtils::IDENTITY) stream.Write(originatorRot);

		uint32_t sBitStreamLength = sBitStream.length();
		stream.Write(sBitStreamLength);
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			stream.Write(sBitStream[k]);
		}

		stream.Write(skillID);

		stream.Write(uiSkillHandle != 0);
		if (uiSkillHandle != 0) stream.Write(uiSkillHandle);
	}

	bool Deserialize(RakNet::BitStream& stream) {
		stream.Read(bUsedMouse);

		bool fCasterLatencyIsDefault{};
		stream.Read(fCasterLatencyIsDefault);
		if (fCasterLatencyIsDefault != 0) stream.Read(fCasterLatency);

		bool iCastTypeIsDefault{};
		stream.Read(iCastTypeIsDefault);
		if (iCastTypeIsDefault != 0) stream.Read(iCastType);

		bool lastClickedPositIsDefault{};
		stream.Read(lastClickedPositIsDefault);
		if (lastClickedPositIsDefault != 0) stream.Read(lastClickedPosit);

		stream.Read(optionalOriginatorID);

		bool optionalTargetIDIsDefault{};
		stream.Read(optionalTargetIDIsDefault);
		if (optionalTargetIDIsDefault != 0) stream.Read(optionalTargetID);

		bool originatorRotIsDefault{};
		stream.Read(originatorRotIsDefault);
		if (originatorRotIsDefault != 0) stream.Read(originatorRot);

		uint32_t sBitStreamLength{};
		stream.Read(sBitStreamLength);
		if (sBitStreamLength > MAX_MESSAGE_LENGTH) return false;
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			unsigned char character;
			stream.Read(character);
			sBitStream.push_back(character);
		}

		stream.Read(skillID);

		bool uiSkillHandleIsDefault{};
		stream.Read(uiSkillHandleIsDefault);
		if (uiSkillHandleIsDefault != 0) stream.Read(uiSkillHandle);

		return true;
	}

	bool bUsedMouse;
	float fCasterLatency;
	int32_t iCastType;
	NiPoint3 lastClickedPosit;
	LWOOBJID optionalOriginatorID;
	LWOOBJID optionalTargetID;
	NiQuaternion originatorRot = QuatUtils::IDENTITY;
	std::string sBitStream;
	TSkillID skillID;
	uint32_t uiSkillHandle;
};

/*  Message to synchronize a skill cast */
class SyncSkill {
public:
	SyncSkill() {
		bDone = false;
	}

	SyncSkill(std::string _sBitStream, uint32_t _uiBehaviorHandle, uint32_t _uiSkillHandle, bool _bDone = false) {
		bDone = _bDone;
		sBitStream = _sBitStream;
		uiBehaviorHandle = _uiBehaviorHandle;
		uiSkillHandle = _uiSkillHandle;
	}

	SyncSkill(RakNet::BitStream& stream) : SyncSkill() {
		Deserialize(stream);
	}

	~SyncSkill() {
	}

	void Serialize(RakNet::BitStream& stream) {
		stream.Write(MessageType::Game::SYNC_SKILL);

		stream.Write(bDone);
		uint32_t sBitStreamLength = sBitStream.length();
		stream.Write(sBitStreamLength);
		for (unsigned int k = 0; k < sBitStreamLength; k++) {
			stream.Write(sBitStream[k]);
		}

		stream.Write(uiBehaviorHandle);
		stream.Write(uiSkillHandle);
	}

	bool Deserialize(RakNet::BitStream& stream) {
		stream.Read(bDone);
		uint32_t sBitStreamLength{};
		stream.Read(sBitStreamLength);
		if (sBitStreamLength > MAX_MESSAGE_LENGTH) return false;
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			unsigned char character;
			stream.Read(character);
			sBitStream.push_back(character);
		}

		stream.Read(uiBehaviorHandle);
		stream.Read(uiSkillHandle);

		return true;
	}

	bool bDone{};
	std::string sBitStream{};
	uint32_t uiBehaviorHandle{};
	uint32_t uiSkillHandle{};
};

/*  Message to synchronize a skill cast */
class EchoSyncSkill {
public:
	EchoSyncSkill() {
		bDone = false;
	}

	EchoSyncSkill(std::string _sBitStream, uint32_t _uiBehaviorHandle, uint32_t _uiSkillHandle, bool _bDone = false) {
		bDone = _bDone;
		sBitStream = _sBitStream;
		uiBehaviorHandle = _uiBehaviorHandle;
		uiSkillHandle = _uiSkillHandle;
	}

	EchoSyncSkill(RakNet::BitStream& stream) : EchoSyncSkill() {
		Deserialize(stream);
	}

	~EchoSyncSkill() {
	}

	void Serialize(RakNet::BitStream& stream) {
		stream.Write(MessageType::Game::ECHO_SYNC_SKILL);

		stream.Write(bDone);
		uint32_t sBitStreamLength = sBitStream.length();
		stream.Write(sBitStreamLength);
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			stream.Write(sBitStream[k]);
		}

		stream.Write(uiBehaviorHandle);
		stream.Write(uiSkillHandle);
	}

	bool Deserialize(RakNet::BitStream& stream) {
		stream.Read(bDone);

		uint32_t sBitStreamLength{};
		stream.Read(sBitStreamLength);
		if (sBitStreamLength > MAX_MESSAGE_LENGTH) return false;
		for (unsigned int k = 0; k < sBitStreamLength; k++) {
			unsigned char character;
			stream.Read(character);
			sBitStream.push_back(character);
		}

		stream.Read(uiBehaviorHandle);
		stream.Read(uiSkillHandle);

		return true;
	}

	bool bDone{};
	std::string sBitStream{};
	uint32_t uiBehaviorHandle{};
	uint32_t uiSkillHandle{};
};

/*  Notifying the server that a locally owned projectile impacted. Sent to the caster of the projectile
		should always be the local char. */
class RequestServerProjectileImpact {
public:
	RequestServerProjectileImpact() {
		i64LocalID = LWOOBJID_EMPTY;
		i64TargetID = LWOOBJID_EMPTY;
	}

	RequestServerProjectileImpact(std::string _sBitStream, LWOOBJID _i64LocalID = LWOOBJID_EMPTY, LWOOBJID _i64TargetID = LWOOBJID_EMPTY) {
		i64LocalID = _i64LocalID;
		i64TargetID = _i64TargetID;
		sBitStream = _sBitStream;
	}

	RequestServerProjectileImpact(RakNet::BitStream& stream) : RequestServerProjectileImpact() {
		Deserialize(stream);
	}

	~RequestServerProjectileImpact() {
	}

	void Serialize(RakNet::BitStream& stream) {
		stream.Write(MessageType::Game::REQUEST_SERVER_PROJECTILE_IMPACT);

		stream.Write(i64LocalID != LWOOBJID_EMPTY);
		if (i64LocalID != LWOOBJID_EMPTY) stream.Write(i64LocalID);

		stream.Write(i64TargetID != LWOOBJID_EMPTY);
		if (i64TargetID != LWOOBJID_EMPTY) stream.Write(i64TargetID);

		uint32_t sBitStreamLength = sBitStream.length();
		stream.Write(sBitStreamLength);
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			stream.Write(sBitStream[k]);
		}

	}

	bool Deserialize(RakNet::BitStream& stream) {
		bool i64LocalIDIsDefault{};
		stream.Read(i64LocalIDIsDefault);
		if (i64LocalIDIsDefault != 0) stream.Read(i64LocalID);

		bool i64TargetIDIsDefault{};
		stream.Read(i64TargetIDIsDefault);
		if (i64TargetIDIsDefault != 0) stream.Read(i64TargetID);

		uint32_t sBitStreamLength{};
		stream.Read(sBitStreamLength);
		if (sBitStreamLength > MAX_MESSAGE_LENGTH) return false;
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			unsigned char character;
			stream.Read(character);
			sBitStream.push_back(character);
		}


		return true;
	}

	LWOOBJID i64LocalID;
	LWOOBJID i64TargetID;
	std::string sBitStream;
};

/*  Tell a client local projectile to impact */
class DoClientProjectileImpact {
public:
	DoClientProjectileImpact() {
		i64OrgID = LWOOBJID_EMPTY;
		i64OwnerID = LWOOBJID_EMPTY;
		i64TargetID = LWOOBJID_EMPTY;
	}

	DoClientProjectileImpact(std::string _sBitStream, LWOOBJID _i64OrgID = LWOOBJID_EMPTY, LWOOBJID _i64OwnerID = LWOOBJID_EMPTY, LWOOBJID _i64TargetID = LWOOBJID_EMPTY) {
		i64OrgID = _i64OrgID;
		i64OwnerID = _i64OwnerID;
		i64TargetID = _i64TargetID;
		sBitStream = _sBitStream;
	}

	DoClientProjectileImpact(RakNet::BitStream& stream) : DoClientProjectileImpact() {
		Deserialize(stream);
	}

	~DoClientProjectileImpact() {
	}

	void Serialize(RakNet::BitStream& stream) {
		stream.Write(MessageType::Game::DO_CLIENT_PROJECTILE_IMPACT);

		stream.Write(i64OrgID != LWOOBJID_EMPTY);
		if (i64OrgID != LWOOBJID_EMPTY) stream.Write(i64OrgID);

		stream.Write(i64OwnerID != LWOOBJID_EMPTY);
		if (i64OwnerID != LWOOBJID_EMPTY) stream.Write(i64OwnerID);

		stream.Write(i64TargetID != LWOOBJID_EMPTY);
		if (i64TargetID != LWOOBJID_EMPTY) stream.Write(i64TargetID);

		uint32_t sBitStreamLength = sBitStream.length();
		stream.Write(sBitStreamLength);
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			stream.Write(sBitStream[k]);
		}

	}

	bool Deserialize(RakNet::BitStream& stream) {
		bool i64OrgIDIsDefault{};
		stream.Read(i64OrgIDIsDefault);
		if (i64OrgIDIsDefault != 0) stream.Read(i64OrgID);

		bool i64OwnerIDIsDefault{};
		stream.Read(i64OwnerIDIsDefault);
		if (i64OwnerIDIsDefault != 0) stream.Read(i64OwnerID);

		bool i64TargetIDIsDefault{};
		stream.Read(i64TargetIDIsDefault);
		if (i64TargetIDIsDefault != 0) stream.Read(i64TargetID);

		uint32_t sBitStreamLength{};
		stream.Read(sBitStreamLength);
		if (sBitStreamLength > MAX_MESSAGE_LENGTH) return false;
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			unsigned char character;
			stream.Read(character);
			sBitStream.push_back(character);
		}


		return true;
	}

	LWOOBJID i64OrgID;
	LWOOBJID i64OwnerID;
	LWOOBJID i64TargetID;
	std::string sBitStream;
};

inline void SendAddSkill(Entity* entity, TSkillID skillID, BehaviorSlot slotID) {
	int AICombatWeight = 0;
	bool bFromSkillSet = false;
	int castType = 0;
	float fTimeSecs = -1.0f;
	int iTimesCanCast = -1;
	bool temporary = true;

	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::ADD_SKILL);

	bitStream.Write(AICombatWeight != 0);
	if (AICombatWeight != 0) bitStream.Write(AICombatWeight);

	bitStream.Write(bFromSkillSet);

	bitStream.Write(castType != 0);
	if (castType != 0) bitStream.Write(castType);

	bitStream.Write(fTimeSecs != -1.0f);
	if (fTimeSecs != -1.0f) bitStream.Write(fTimeSecs);

	bitStream.Write(iTimesCanCast != -1);
	if (iTimesCanCast != -1) bitStream.Write(iTimesCanCast);

	bitStream.Write(skillID);

	bitStream.Write(slotID != BehaviorSlot::Invalid);
	if (slotID != BehaviorSlot::Invalid) bitStream.Write(slotID);

	bitStream.Write(temporary);

	SystemAddress sysAddr = entity->GetSystemAddress();
	SEND_PACKET;
}

inline void SendRemoveSkill(Entity* entity, TSkillID skillID) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::REMOVE_SKILL);
	bitStream.Write(false);
	bitStream.Write(skillID);

	SystemAddress sysAddr = entity->GetSystemAddress();
	SEND_PACKET;
}

// The START_SKILL switch case in GameMessageHandler::HandleMessage read the message with StartSkill::Deserialize,
// SYNC_SKILL with the SyncSkill(RakNet::BitStream&) constructor and REQUEST_SERVER_PROJECTILE_IMPACT with
// RequestServerProjectileImpact::Deserialize; the classes above are those read sequences.
}

#endif // SKILLMESSAGESLEGACY_H
