#include "CheatDetection.h"
#include "Database.h"
#include "Entity.h"
#include "PossessableComponent.h"
#include "Player.h"
#include "Game.h"
#include "EntityManager.h"
#include "Character.h"
#include "User.h"
#include "UserManager.h"
#include "dConfig.h"

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
		Game::logger->Log("CheatDetection", "WARNING: User is null, using defaults.");
	}
	std::unique_ptr<sql::PreparedStatement> stmt(Database::CreatePreppedStmt(
		"INSERT INTO player_cheat_detections (account_id, name, violation_msg, violation_system_address) VALUES (?, ?, ?, ?)")
	);
	stmt->setInt(1, user ? user->GetAccountID() : 0);
	stmt->setString(2, user ? user->GetUsername().c_str() : "User is null.");

	constexpr int32_t bufSize = 4096;
	char buffer[bufSize];
	vsnprintf(buffer, bufSize, messageIfNotSender, args);

	stmt->setString(3, buffer);
	stmt->setString(4, Game::config->GetValue("log_ip_addresses_for_anti_cheat") == "1" ? sysAddr.ToString() : "IP logging disabled.");
	stmt->execute();
	Game::logger->Log("CheatDetection", "Anti-cheat message: %s", buffer);
}

void LogAndSaveFailedAntiCheatCheck(const LWOOBJID& id, const SystemAddress& sysAddr, const CheckType checkType, const char* messageIfNotSender, va_list args) {
	User* toReport = nullptr;
	switch (checkType) {
	case CheckType::Entity: {
		auto* player = Player::GetPlayer(sysAddr);
		auto* entity = GetPossessedEntity(id);
		
		// If player exists and entity exists in world, use both for logging info.
		if (entity && player) {
			Game::logger->Log("CheatDetection", "Player (%s) (%llu) at system address (%s) with sending player (%s) (%llu) does not match their own.",
				player->GetCharacter()->GetName().c_str(), player->GetObjectID(),
				sysAddr.ToString(),
				entity->GetCharacter()->GetName().c_str(), entity->GetObjectID());
			toReport = player->GetParentUser();
		// In the case that the target entity id did not exist, just log the player info.
		} else if (player) {
			Game::logger->Log("CheatDetection", "Player (%s) (%llu) at system address (%s) with sending player (%llu) does not match their own.",
				player->GetCharacter()->GetName().c_str(), player->GetObjectID(),
				sysAddr.ToString(), id);
			toReport = player->GetParentUser();
		// In the rare case that the player does not exist, just log the system address and who the target id was.
		} else {
			Game::logger->Log("CheatDetection", "Player at system address (%s) with sending player (%llu) does not match their own.",
				sysAddr.ToString(), id);
		}
		break;
	}
	case CheckType::User: {
		auto* user = UserManager::Instance()->GetUser(sysAddr);
		
		if (user) {
			Game::logger->Log("CheatDetection", "User at system address (%s) (%s) (%llu) sent a packet as (%i) which is not an id they own.",
				sysAddr.ToString(), user->GetLastUsedChar()->GetName().c_str(), user->GetLastUsedChar()->GetObjectID(), static_cast<int32_t>(id));
		// Can't know sending player. Just log system address for IP banning.
		} else {
			Game::logger->Log("CheatDetection", "No user found for system address (%s).", sysAddr.ToString());
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
			Game::logger->Log("CheatDetection", "No user found for system address (%s).", sysAddr.ToString());
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
	if (invalidPacket) {
		va_list args;
		va_start(args, messageIfNotSender);
		LogAndSaveFailedAntiCheatCheck(id, sysAddr, checkType, messageIfNotSender, args);
		va_end(args);
	}

	return !invalidPacket;
}
