#include "EffectsMessages.h"

#include "AMFDeserialize.h"
#include "AmfSerialize.h"
#include "BitStreamUtils.h"
#include "CDClientManager.h"
#include "CDEmoteTable.h"
#include "Character.h"
#include "eMissionTaskType.h"
#include "eReplicaComponentType.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "MissionComponent.h"
#include "NiPoint3.h"
#include "RacingControlComponent.h"
#include "ScriptedActivityComponent.h"
#include "User.h"
#include "UserManager.h"

#include <stdexcept>

namespace {
	// Reads an AMF3 array written with Write<AMFBaseValue&>. Fails on anything that is not an array.
	bool ReadAmfArray(RakNet::BitStream& bitStream, AMFArrayValue& args) {
		try {
			AMFDeserialize reader;
			auto value = reader.Read(bitStream);
			if (!value || value->GetValueType() != eAmf::Array) return false;
			args = std::move(*static_cast<AMFArrayValue*>(value.get()));
			return true;
		} catch (const std::exception&) {
			return false;
		}
	}
}

namespace GameMessages {
	void PlayAnimation::Serialize(RakNet::BitStream& bitStream) const {
		const auto animationIDLength = static_cast<uint32_t>(GeneralUtils::UTF16ToWTF8(animationID).size());
		bitStream.Write(animationIDLength);
		bitStream.Write(LUWString(animationID, animationIDLength));
		bitStream.Write(bExpectAnimToExist);
		bitStream.Write(bPlayImmediate);
		bitStream.Write(bTriggerOnCompleteMsg);
		BitStreamUtils::WriteOptional(bitStream, fPriority, 0.0f);
		BitStreamUtils::WriteOptional(bitStream, fScale, 1.0f);
	}

	bool PlayAnimation::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, animationID));
		VALIDATE_READ(bitStream.Read(bExpectAnimToExist));
		VALIDATE_READ(bitStream.Read(bPlayImmediate));
		VALIDATE_READ(bitStream.Read(bTriggerOnCompleteMsg));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, fPriority, 0.0f));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, fScale, 1.0f));
		return true;
	}

	void PlayNDAudioEmitter::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional<LWOOBJID>(bitStream, NDAudioCallbackMessageData, 0);
		BitStreamUtils::WriteOptional<uint32_t>(bitStream, NDAudioEmitterID, 0);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, NDAudioEventGUID);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, NDAudioMetaEventName);
		bitStream.Write(result);
		BitStreamUtils::WriteOptional(bitStream, targetObjectIDForNDAudioCallbackMessages, LWOOBJID_EMPTY);
	}

	bool PlayNDAudioEmitter::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional<LWOOBJID>(bitStream, NDAudioCallbackMessageData, 0));
		VALIDATE_READ(BitStreamUtils::ReadOptional<uint32_t>(bitStream, NDAudioEmitterID, 0));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, NDAudioEventGUID));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, NDAudioMetaEventName));
		VALIDATE_READ(bitStream.Read(result));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, targetObjectIDForNDAudioCallbackMessages, LWOOBJID_EMPTY));
		return true;
	}

	void PlayEmbeddedEffectOnAllClientsNearObject::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, effectName);
		bitStream.Write(fromObjectID);
		bitStream.Write(radius);
	}

	bool PlayEmbeddedEffectOnAllClientsNearObject::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, effectName));
		VALIDATE_READ(bitStream.Read(fromObjectID));
		VALIDATE_READ(bitStream.Read(radius));
		return true;
	}

	void PlayFXEffect::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, effectID, -1);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, effectType);
		BitStreamUtils::WriteOptional(bitStream, scale, 1.0f);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, name);
		BitStreamUtils::WriteOptional(bitStream, priority, 1.0f);
		BitStreamUtils::WriteOptional(bitStream, secondary, LWOOBJID_EMPTY);
		bitStream.Write(serialize);
	}

	bool PlayFXEffect::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, effectID, -1));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, effectType));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, scale, 1.0f));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, name));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, priority, 1.0f));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, secondary, LWOOBJID_EMPTY));
		VALIDATE_READ(bitStream.Read(serialize));
		return true;
	}

	void StopFXEffect::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(killImmediate);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, name);
	}

	bool StopFXEffect::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(killImmediate));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, name));
		return true;
	}

	void BroadcastTextToChatbox::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, attrs);
		bitStream.Write<uint16_t>(0x00); // null terminator, always written
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, wsText);
	}

	bool BroadcastTextToChatbox::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, attrs));
		uint16_t nullTerminator{};
		VALIDATE_READ(bitStream.Read(nullTerminator));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, wsText));
		return true;
	}

	void Play2DAmbientSound::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, audioGUID);
		bitStream.Write(result);
	}

	bool Play2DAmbientSound::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, audioGUID));
		VALIDATE_READ(bitStream.Read(result));
		return true;
	}

	void Stop2DAmbientSound::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(force);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, audioGUID);
		bitStream.Write(result);
	}

	bool Stop2DAmbientSound::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(force));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, audioGUID));
		VALIDATE_READ(bitStream.Read(result));
		return true;
	}

	void UIMessageServerToSingleClient::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<AMFBaseValue&>(const_cast<AMFArrayValue&>(args));
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, strMessageName);
	}

	bool UIMessageServerToSingleClient::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadAmfArray(bitStream, args));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, strMessageName));
		return true;
	}

	void UIMessageServerToAllClients::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<AMFBaseValue&>(const_cast<AMFArrayValue&>(args));
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, strMessageName);
	}

	bool UIMessageServerToAllClients::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadAmfArray(bitStream, args));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, strMessageName));
		return true;
	}

	void StartCelebrationEffect::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, animation);
		BitStreamUtils::WriteOptional<LOT>(bitStream, backgroundObject, 11164);
		BitStreamUtils::WriteOptional<LOT>(bitStream, cameraPathLOT, 12458);
		BitStreamUtils::WriteOptional(bitStream, celeLeadIn, 1.0f);
		BitStreamUtils::WriteOptional(bitStream, celeLeadOut, 0.8f);
		bitStream.Write1(); // DLU always sends celebrationID
		bitStream.Write(celebrationID);
		bitStream.Write(duration);
		bitStream.Write(iconID);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, mainText);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, mixerProgram);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, musicCue);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, pathNodeName);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, soundGUID);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, subText);
	}

	bool StartCelebrationEffect::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, animation));
		VALIDATE_READ(BitStreamUtils::ReadOptional<LOT>(bitStream, backgroundObject, 11164));
		VALIDATE_READ(BitStreamUtils::ReadOptional<LOT>(bitStream, cameraPathLOT, 12458));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, celeLeadIn, 1.0f));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, celeLeadOut, 0.8f));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, celebrationID, -1));
		VALIDATE_READ(bitStream.Read(duration));
		VALIDATE_READ(bitStream.Read(iconID));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, mainText));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, mixerProgram));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, musicCue));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, pathNodeName));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, soundGUID));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, subText));
		return true;
	}

	void DisplayMessageBox::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bShow);
		bitStream.Write(callbackClient);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, identifier);
		bitStream.Write(imageID);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, text);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, userData);
	}

	bool DisplayMessageBox::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bShow));
		VALIDATE_READ(bitStream.Read(callbackClient));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, identifier));
		VALIDATE_READ(bitStream.Read(imageID));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, text));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, userData));
		return true;
	}

	void MessageBoxRespond::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(iButton);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, identifier);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, userData);
	}

	bool MessageBoxRespond::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(iButton));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, identifier));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, userData));
		return true;
	}

	void MessageBoxRespond::Handle(Entity& entityRef, const SystemAddress& sysAddr) {
		Entity* entity = &entityRef;
		LOG("Button: %d; LOT: %u identifier: %s; userData: %s", iButton, entity->GetLOT(), GeneralUtils::UTF16ToWTF8(identifier).c_str(), GeneralUtils::UTF16ToWTF8(userData).c_str());

		auto* user = UserManager::Instance()->GetUser(sysAddr);

		if (user == nullptr) {
			return;
		}

		auto* userEntity = user->GetLastUsedChar()->GetEntity();

		if (userEntity == nullptr) {
			return;
		}

		entity->OnMessageBoxResponse(userEntity, iButton, identifier, userData);

		auto* scriptedActivityComponent = entity->GetComponent<ScriptedActivityComponent>();

		if (scriptedActivityComponent != nullptr) {
			scriptedActivityComponent->HandleMessageBoxResponse(userEntity, GeneralUtils::UTF16ToWTF8(identifier));
		}

		auto* racingControlComponent = entity->GetComponent<RacingControlComponent>();

		if (racingControlComponent != nullptr) {
			racingControlComponent->HandleMessageBoxResponse(userEntity, iButton, GeneralUtils::UTF16ToWTF8(identifier));
		}

		for (auto* shootingGallery : Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::SHOOTING_GALLERY)) {
			shootingGallery->OnMessageBoxResponse(userEntity, iButton, identifier, userData);
		}
	}

	void ChoiceBoxRespond::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, buttonIdentifier);
		bitStream.Write(iButton);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, identifier);
	}

	bool ChoiceBoxRespond::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, buttonIdentifier));
		VALIDATE_READ(bitStream.Read(iButton));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, identifier));
		return true;
	}

	void ChoiceBoxRespond::Handle(Entity& entityRef, const SystemAddress& sysAddr) {
		Entity* entity = &entityRef;
		LOG("Button: %d; LOT: %u buttonIdentifier: %s; userData: %s", iButton, entity->GetLOT(), GeneralUtils::UTF16ToWTF8(buttonIdentifier).c_str(), GeneralUtils::UTF16ToWTF8(identifier).c_str());

		auto* user = UserManager::Instance()->GetUser(sysAddr);

		if (user == nullptr) {
			return;
		}

		auto* userEntity = user->GetLastUsedChar()->GetEntity();

		if (userEntity == nullptr) {
			return;
		}

		entity->OnChoiceBoxResponse(userEntity, iButton, buttonIdentifier, identifier);
	}

	void DisplayChatBubble::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, wsText);
	}

	bool DisplayChatBubble::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, wsText));
		return true;
	}

	void ChangeIdleFlags::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, flagsOff, eAnimationFlags::IDLE_NONE);
		BitStreamUtils::WriteOptional(bitStream, flagsOn, eAnimationFlags::IDLE_NONE);
	}

	bool ChangeIdleFlags::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, flagsOff, eAnimationFlags::IDLE_NONE));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, flagsOn, eAnimationFlags::IDLE_NONE));
		return true;
	}

	void PlayCinematic::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(allowGhostUpdates);
		bitStream.Write(bCloseMultiInteract);
		bitStream.Write(bSendServerNotify);
		bitStream.Write(bUseControlledObjectForAudioListener);
		BitStreamUtils::WriteOptional(bitStream, endBehavior, eEndBehavior::RETURN);
		bitStream.Write(hidePlayerDuringCine);
		BitStreamUtils::WriteOptional(bitStream, leadIn, -1.0f);
		bitStream.Write(leavePlayerLockedWhenFinished);
		bitStream.Write(lockPlayer);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, pathName);
		bitStream.Write(result);
		bitStream.Write(skipIfSamePath);
		bitStream.Write(startTimeAdvance);
	}

	bool PlayCinematic::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(allowGhostUpdates));
		VALIDATE_READ(bitStream.Read(bCloseMultiInteract));
		VALIDATE_READ(bitStream.Read(bSendServerNotify));
		VALIDATE_READ(bitStream.Read(bUseControlledObjectForAudioListener));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, endBehavior, eEndBehavior::RETURN));
		VALIDATE_READ(bitStream.Read(hidePlayerDuringCine));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, leadIn, -1.0f));
		VALIDATE_READ(bitStream.Read(leavePlayerLockedWhenFinished));
		VALIDATE_READ(bitStream.Read(lockPlayer));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, pathName));
		VALIDATE_READ(bitStream.Read(result));
		VALIDATE_READ(bitStream.Read(skipIfSamePath));
		VALIDATE_READ(bitStream.Read(startTimeAdvance));
		return true;
	}

	void EndCinematic::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, leadOut, -1.0f);
		bitStream.Write(leavePlayerLocked);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, pathName);
	}

	bool EndCinematic::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, leadOut, -1.0f));
		VALIDATE_READ(bitStream.Read(leavePlayerLocked));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, pathName));
		return true;
	}

	void CinematicUpdate::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, event, eCinematicEvent::STARTED);
		BitStreamUtils::WriteOptional(bitStream, overallTime, -1.0f);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, pathName);
		BitStreamUtils::WriteOptional(bitStream, pathTime, -1.0f);
		BitStreamUtils::WriteOptional(bitStream, waypoint, -1);
	}

	bool CinematicUpdate::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, event, eCinematicEvent::STARTED));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, overallTime, -1.0f));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, pathName));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, pathTime, -1.0f));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, waypoint, -1));
		return true;
	}

	void CinematicUpdate::Handle(Entity& entity, const SystemAddress& sysAddr) {
		std::vector<Entity*> scriptedActs = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::SCRIPT);
		for (Entity* scriptEntity : scriptedActs) {
			scriptEntity->OnCinematicUpdate(scriptEntity, &entity, event, pathName, pathTime, overallTime, waypoint);
		}
	}

	void SlashCommandTextFeedback::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, text);
	}

	bool SlashCommandTextFeedback::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, text));
		return true;
	}

	void PlayEmote::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(emoteID);
		bitStream.Write(targetID);
	}

	bool PlayEmote::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(emoteID));
		VALIDATE_READ(bitStream.Read(targetID));
		return true;
	}

	void PlayEmote::Handle(Entity& entityRef, const SystemAddress& sysAddr) {
		Entity* entity = &entityRef;
		LOG_DEBUG("Emote (%i) (%llu)", emoteID, targetID);

		//TODO: If targetID != 0, and we have one of the "perform emote" missions, complete them.

		if (emoteID == 0) return;
		std::string sAnimationName = "deaded"; //Default name in case we fail to get the emote

		CDEmoteTableTable* emotes = CDClientManager::GetTable<CDEmoteTableTable>();
		if (emotes) {
			CDEmoteTable* emote = emotes->GetEmote(emoteID);
			if (emote) sAnimationName = emote->animationName;
		}

		GameMessages::EmotePlayed msg;
		msg.target = entity->GetObjectID();
		msg.emoteID = emoteID;
		msg.targetID = targetID;      // The emote’s target entity or 0 if none
		msg.Send(UNASSIGNED_SYSTEM_ADDRESS);  // Broadcast to all clients

		MissionComponent* missionComponent = entity->GetComponent<MissionComponent>();
		if (!missionComponent) return;

		if (targetID != LWOOBJID_EMPTY) {
			auto* targetEntity = Game::entityManager->GetEntity(targetID);

			LOG_DEBUG("Emote target found (%d)", targetEntity != nullptr);

			if (targetEntity != nullptr) {
				targetEntity->OnEmoteReceived(emoteID, entity);
				missionComponent->Progress(eMissionTaskType::EMOTE, emoteID, targetID);
			}
		} else {
			LOG_DEBUG("Target ID is empty, using backup");
			const auto scriptedEntities = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::SCRIPT);

			const auto& referencePoint = entity->GetPosition();

			for (auto* scripted : scriptedEntities) {
				if (Vector3::DistanceSquared(scripted->GetPosition(), referencePoint) > 5.0f * 5.0f) continue;

				scripted->OnEmoteReceived(emoteID, entity);
				missionComponent->Progress(eMissionTaskType::EMOTE, emoteID, scripted->GetObjectID());
			}
		}
	}

	void SetEmoteLockState::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bLock);
		bitStream.Write(emoteID);
	}

	bool SetEmoteLockState::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bLock));
		VALIDATE_READ(bitStream.Read(emoteID));
		return true;
	}

	void DisplayTooltip::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(doOrDie);
		bitStream.Write(noRepeat);
		bitStream.Write(noRevive);
		bitStream.Write(isPropertyTooltip);
		bitStream.Write(show);
		bitStream.Write(translate);
		bitStream.Write(time);
		bitStream.Write<int32_t>(id.size());
		bitStream.Write(id);

		std::string toWrite;
		for (const auto& item : localizeParams | std::views::values) {
			toWrite += item->GetString() + "\n";
		}
		if (!toWrite.empty()) toWrite.pop_back();
		bitStream.Write<int32_t>(toWrite.size());
		bitStream.Write(GeneralUtils::ASCIIToUTF16(toWrite));
		if (!toWrite.empty()) bitStream.Write<uint16_t>(0x00); // Null Terminator

		bitStream.Write<int32_t>(imageName.size());
		bitStream.Write(imageName);
		bitStream.Write<int32_t>(text.size());
		bitStream.Write(text);
	}

	void EmotePlayed::Serialize(RakNet::BitStream& stream) const {
		stream.Write(emoteID);
		stream.Write(targetID);
	}

	bool EmotePlayed::Deserialize(RakNet::BitStream& stream) {
		VALIDATE_READ(stream.Read(emoteID));
		VALIDATE_READ(stream.Read(targetID));
		return true;
	}
}
