#include "ActivityMessages.h"

#include "BitStreamUtils.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "LeaderboardManager.h"
#include "Logger.h"
#include "ScriptedActivityComponent.h"
#include "eReplicaComponentType.h"

namespace GameMessages {
	void ActivityStop::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bExit);
		bitStream.Write(bUserCancel);
	}

	bool ActivityStop::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bExit));
		VALIDATE_READ(bitStream.Read(bUserCancel));
		return true;
	}

	void ActivityPause::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bPause);
	}

	bool ActivityPause::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bPause));
		return true;
	}

	void StartActivityTime::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(startTime);
	}

	bool StartActivityTime::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(startTime));
		return true;
	}

	void RequestActivityEnter::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bStart);
		bitStream.Write(userID);
	}

	bool RequestActivityEnter::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bStart));
		VALIDATE_READ(bitStream.Read(userID));
		return true;
	}

	void RequestActivityExit::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bUserCancel);
		bitStream.Write(userID);
	}

	bool RequestActivityExit::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bUserCancel));
		VALIDATE_READ(bitStream.Read(userID));
		return true;
	}

	void RequestActivityExit::Handle(Entity& entity, const SystemAddress& sysAddr) {
		if (!bUserCancel) return;

		auto* player = Game::entityManager->GetEntity(userID);
		if (!player) return;
		entity.RequestActivityExit(&entity, userID, bUserCancel);
	}

	void ShowActivityCountdown::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bPlayAdditionalSound);
		bitStream.Write(bPlayCountdownSound);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, sndName);
		bitStream.Write(stateToPlaySoundOn);
	}

	bool ShowActivityCountdown::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bPlayAdditionalSound));
		VALIDATE_READ(bitStream.Read(bPlayCountdownSound));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, sndName));
		VALIDATE_READ(bitStream.Read(stateToPlaySoundOn));
		return true;
	}

	namespace {
		void WritePoint(RakNet::BitStream& bitStream, const NiPoint3& point) {
			bitStream.Write(point.x);
			bitStream.Write(point.y);
			bitStream.Write(point.z);
		}

		bool ReadPoint(RakNet::BitStream& bitStream, NiPoint3& point) {
			return bitStream.Read(point.x) && bitStream.Read(point.y) && bitStream.Read(point.z);
		}
	}

	void MatchResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(response);
	}

	bool MatchResponse::Deserialize(RakNet::BitStream& bitStream) {
		return bitStream.Read(response);
	}

	void MatchUpdate::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteNameValueText(bitStream, std::u16string(data.begin(), data.end()));
		bitStream.Write(type);
	}

	bool MatchUpdate::Deserialize(RakNet::BitStream& bitStream) {
		std::u16string wide;
		VALIDATE_READ(BitStreamUtils::ReadNameValueText(bitStream, wide));
		data.assign(wide.begin(), wide.end());
		VALIDATE_READ(bitStream.Read(type));
		return true;
	}

	void MatchRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(activator);
		BitStreamUtils::WriteNameValueText(bitStream, std::u16string(playerChoices.begin(), playerChoices.end()));
		bitStream.Write(type);
		bitStream.Write(value);
	}

	bool MatchRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(activator));
		std::u16string wide;
		VALIDATE_READ(BitStreamUtils::ReadNameValueText(bitStream, wide));
		playerChoices.clear();
		for (const auto character : wide) playerChoices.push_back(character);
		VALIDATE_READ(bitStream.Read(type));
		VALIDATE_READ(bitStream.Read(value));
		return true;
	}

	void MatchRequest::Handle(Entity& entity, const SystemAddress& sysAddr) {
		std::vector<Entity*> scriptedActs = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::SCRIPTED_ACTIVITY);
		if (type == 0) { // join
			if (value != 0) {
				for (Entity* scriptedAct : scriptedActs) {
					auto* comp = scriptedAct->GetComponent<ScriptedActivityComponent>();
					if (!comp) continue;
					if (comp->GetActivityID() == value) {
						comp->PlayerJoin(&entity, playerChoices);
					}
				}
			}
		} else if (type == 1) { // ready/unready
			// Answered like a join (live), then the chat server tells the lobby (docs/Matchmaking.md)
			MatchResponse response;
			response.target = entity.GetObjectID();
			response.response = 0;
			response.SendToClient(sysAddr);
			ActivityComponent::PlayerReady(&entity, value != 0);
		}
	}

	void RequestActivitySummaryLeaderboardData::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, gameID, 0);
		BitStreamUtils::WriteOptional(bitStream, queryType, 1);
		BitStreamUtils::WriteOptional(bitStream, resultsEnd, 10);
		BitStreamUtils::WriteOptional(bitStream, resultsStart, 0);
		bitStream.Write(targetID);
		bitStream.Write(weekly);
	}

	bool RequestActivitySummaryLeaderboardData::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, gameID, 0));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, queryType, 1));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, resultsEnd, 10));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, resultsStart, 0));
		VALIDATE_READ(bitStream.Read(targetID));
		VALIDATE_READ(bitStream.Read(weekly));
		return true;
	}

	void RequestActivitySummaryLeaderboardData::Handle(Entity& entity, const SystemAddress& sysAddr) {
		// The client won't accept more than 10 results even if we wanted it to
		LeaderboardManager::SendLeaderboard(gameID, static_cast<Leaderboard::InfoType>(queryType), weekly, entity.GetObjectID(), entity.GetObjectID(), 10);
	}

	void SendActivitySummaryLeaderboardData::Serialize(RakNet::BitStream& bitStream) const {
		if (leaderboard) leaderboard->Serialize(bitStream);
	}

	void SendActivitySummaryLeaderboardData::Handle(Entity& entity, const SystemAddress& sysAddr) {
		LOG("We got mail!");
	}

	void NotifyClientShootingGalleryScore::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(addTime);
		bitStream.Write(score);
		bitStream.Write(targetID);
		WritePoint(bitStream, targetPos);
	}

	bool NotifyClientShootingGalleryScore::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(addTime));
		VALIDATE_READ(bitStream.Read(score));
		VALIDATE_READ(bitStream.Read(targetID));
		VALIDATE_READ(ReadPoint(bitStream, targetPos));
		return true;
	}

	void UpdateShootingGalleryRotation::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(angle);
		WritePoint(bitStream, facing);
		WritePoint(bitStream, muzzlePos);
	}

	bool UpdateShootingGalleryRotation::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(angle));
		VALIDATE_READ(ReadPoint(bitStream, facing));
		VALIDATE_READ(ReadPoint(bitStream, muzzlePos));
		return true;
	}

	void ShootingGalleryFire::Serialize(RakNet::BitStream& bitStream) const {
		WritePoint(bitStream, target);
		bitStream.Write(rotation.w);
		bitStream.Write(rotation.x);
		bitStream.Write(rotation.y);
		bitStream.Write(rotation.z);
	}

	bool ShootingGalleryFire::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPoint(bitStream, target));
		VALIDATE_READ(bitStream.Read(rotation.w));
		VALIDATE_READ(bitStream.Read(rotation.x));
		VALIDATE_READ(bitStream.Read(rotation.y));
		VALIDATE_READ(bitStream.Read(rotation.z));
		return true;
	}

	void ShootingGalleryFire::Handle(Entity& entity, const SystemAddress& sysAddr) {
		entity.OnShootingGalleryFire(*this);
	}

	void ActivityStateChangeRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(objectID);
		bitStream.Write(value1);
		bitStream.Write(value2);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, stringValue);
	}

	bool ActivityStateChangeRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(objectID));
		VALIDATE_READ(bitStream.Read(value1));
		VALIDATE_READ(bitStream.Read(value2));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, stringValue));
		return true;
	}

	void ActivityStateChangeRequest::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* assosiate = Game::entityManager->GetEntity(objectID);

		LOG("%s [%i, %i] from %i to %i", GeneralUtils::UTF16ToWTF8(stringValue).c_str(), value1, value2, entity.GetLOT(), assosiate != nullptr ? assosiate->GetLOT() : 0);

		std::vector<Entity*> scriptedActs = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::SHOOTING_GALLERY);
		for (Entity* scriptEntity : scriptedActs) {
			scriptEntity->OnActivityStateChangeRequest(objectID, value1, value2, stringValue);
		}

		entity.OnActivityStateChangeRequest(objectID, value1, value2, stringValue);
	}
}
