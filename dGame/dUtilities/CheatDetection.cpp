#include "CheatDetection.h"
#include "Database.h"
#include "Entity.h"
#include "PossessableComponent.h"
#include "Game.h"
#include "EntityManager.h"
#include "Character.h"
#include "User.h"
#include "UserManager.h"
#include "dConfig.h"
#include <optional>
#include "PlayerManager.h"

Entity* GetPossessedEntity(const LWOOBJID& objId) {
	auto* entity = Game::entityManager->GetEntity(objId);
	if (!entity) return nullptr;

	auto* possessableComponent = entity->GetComponent<PossessableComponent>();
	// If no possessable, then this entity is the most possessed entity.
	if (!possessableComponent) return entity;

	// If not, get the entity that possesses the fetched entity.
	return Game::entityManager->GetEntity(possessableComponent->GetPossessor());
}

void ReportCheat(User* user, const SystemAddress& sysAddr, const char* messageIfNotSender, va_list args) {
	if (!user) {
		LOG("WARNING: User is null, using defaults.");
	}

	IPlayerCheatDetections::Info info;
	if (user) info.userId = user->GetAccountID();
	info.username = user ? user->GetUsername().c_str() : "User is null.";

	// user string here because ToString is static and may change.
	info.systemAddress = sysAddr.ToString();

	constexpr int32_t bufSize = 4096;
	char extraMsg[bufSize];
	vsnprintf(extraMsg, bufSize, messageIfNotSender, args);
	info.extraMessage = extraMsg;

	Database::Get()->InsertCheatDetection(info);

	LOG("Anti-cheat message: %s", extraMsg);
}

void LogAndSaveFailedAntiCheatCheck(const LWOOBJID& id, const SystemAddress& sysAddr, const CheckType checkType, const char* messageIfNotSender, va_list args) {
	User* toReport = nullptr;
	switch (checkType) {
	case CheckType::Entity: {
		auto* player = PlayerManager::GetPlayer(sysAddr);
		auto* entity = GetPossessedEntity(id);
		
		// If player exists and entity exists in world, use both for logging info.
		if (entity && player) {
			LOG("Player (%s) (%llu) at system address (%s) with sending player (%s) (%llu) does not match their own.",
				player->GetCharacter()->GetName().c_str(), player->GetObjectID(),
				sysAddr.ToString(),
				entity->GetCharacter()->GetName().c_str(), entity->GetObjectID());
			if (player->GetCharacter()) toReport = player->GetCharacter()->GetParentUser();
		// In the case that the target entity id did not exist, just log the player info.
		} else if (player) {
			LOG("Player (%s) (%llu) at system address (%s) with sending player (%llu) does not match their own.",
				player->GetCharacter()->GetName().c_str(), player->GetObjectID(),
				sysAddr.ToString(), id);
			if (player->GetCharacter()) toReport = player->GetCharacter()->GetParentUser();
		// In the rare case that the player does not exist, just log the system address and who the target id was.
		} else {
			LOG("Player at system address (%s) with sending player (%llu) does not match their own.",
				sysAddr.ToString(), id);
		}
		break;
	}
	case CheckType::User: {
		auto* user = UserManager::Instance()->GetUser(sysAddr);
		
		if (user) {
			LOG("User at system address (%s) (%s) (%llu) sent a packet as (%i) which is not an id they own.",
				sysAddr.ToString(), user->GetLastUsedChar()->GetName().c_str(), user->GetLastUsedChar()->GetObjectID(), static_cast<int32_t>(id));
		// Can't know sending player. Just log system address for IP banning.
		} else {
			LOG("No user found for system address (%s).", sysAddr.ToString());
		}
		toReport = user;
		break;
	}
	};
	ReportCheat(toReport, sysAddr, messageIfNotSender, args);
}

void CheatDetection::ReportCheat(User* user, const SystemAddress& sysAddr, const char* messageIfNotSender, ...) {
	va_list args;
	va_start(args, messageIfNotSender);
	ReportCheat(user, sysAddr, messageIfNotSender, args);
	va_end(args);
}

bool CheatDetection::VerifyLwoobjidIsSender(const LWOOBJID& id, const SystemAddress& sysAddr, const CheckType checkType, const char* messageIfNotSender, ...) {
	// Get variables we'll need for the whole function
	bool invalidPacket = false;
	switch (checkType) {
	case CheckType::Entity: {
		// In this case, the sender may be an entity in the world.
		auto* entity = GetPossessedEntity(id);
		if (entity) {
			invalidPacket = entity->IsPlayer() && entity->GetSystemAddress() != sysAddr;
		}
		break;
	}
	case CheckType::User: {
		// In this case, the player is not an entity in the world, but may be a user still in world server if they are connected.
		// Check here if the system address has a character with id matching the lwoobjid after unsetting the flag bits.
		auto* sendingUser = UserManager::Instance()->GetUser(sysAddr);
		if (!sendingUser) {
			LOG("No user found for system address (%s).", sysAddr.ToString());
			return false;
		}
		invalidPacket = true;
		const uint32_t characterId = static_cast<uint32_t>(id);
		// Check to make sure the ID provided is one of the user's characters.
		for (const auto& character : sendingUser->GetCharacters()) {
			if (character && character->GetID() == characterId) {
				invalidPacket = false;
				break;
			}
		}
	}
	};

	// This will be true if the player does not possess the entity they are trying to send a packet as.
	// or if the user does not own the character they are trying to send a packet as.

	return !invalidPacket;
}
