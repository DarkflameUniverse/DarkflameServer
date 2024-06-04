#include "GMGreaterThanZeroCommands.h"

// Classes
#include "Character.h"
#include "ChatPackets.h"
#include "dServer.h"
#include "PlayerManager.h"
#include "User.h"

// Database
#include "Database.h"

// Components
#include "DestroyableComponent.h"
#include "PropertyManagementComponent.h"

// Enums
#include "eChatMessageType.h"
#include "eServerDisconnectIdentifiers.h"
#include "eObjectBits.h"

namespace GMGreaterThanZeroCommands {

	void Kick(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.size() == 1) {
			auto* player = PlayerManager::GetPlayer(splitArgs[0]);

			std::u16string username = GeneralUtils::UTF8ToUTF16(splitArgs[0]);
			if (player == nullptr) {
				ChatPackets::SendSystemMessage(sysAddr, u"Count not find player of name: " + username);
				return;
			}

			Game::server->Disconnect(player->GetSystemAddress(), eServerDisconnectIdentifiers::KICK);

			ChatPackets::SendSystemMessage(sysAddr, u"Kicked: " + username);
		} else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /kick <username>");
		}
	}

	void Ban(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');

		if (splitArgs.size() == 1) {
			auto* player = PlayerManager::GetPlayer(splitArgs[0]);

			uint32_t accountId = 0;

			if (player == nullptr) {
				auto characterInfo = Database::Get()->GetCharacterInfo(splitArgs[0]);

				if (characterInfo) {
					accountId = characterInfo->accountId;
				}

				if (accountId == 0) {
					ChatPackets::SendSystemMessage(sysAddr, u"Count not find player of name: " + GeneralUtils::UTF8ToUTF16(splitArgs[0]));

					return;
				}
			} else {
				auto* character = player->GetCharacter();
				auto* user = character != nullptr ? character->GetParentUser() : nullptr;
				if (user) accountId = user->GetAccountID();
			}

			if (accountId != 0) Database::Get()->UpdateAccountBan(accountId, true);

			if (player != nullptr) {
				Game::server->Disconnect(player->GetSystemAddress(), eServerDisconnectIdentifiers::FREE_TRIAL_EXPIRED);
			}

			ChatPackets::SendSystemMessage(sysAddr, u"Banned: " + GeneralUtils::ASCIIToUTF16(splitArgs[0]));
		} else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /ban <username>");
		}
	}

	void MailItem(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.size() < 2) return;

		const auto& playerName = splitArgs[0];

		auto playerInfo = Database::Get()->GetCharacterInfo(playerName);

		uint32_t receiverID = 0;
		if (!playerInfo) {
			ChatPackets::SendSystemMessage(sysAddr, u"Failed to find that player");

			return;
		}

		receiverID = playerInfo->id;

		const auto lot = GeneralUtils::TryParse<LOT>(splitArgs.at(1));

		if (!lot) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid item lot.");
			return;
		}

		IMail::MailInfo mailInsert;
		mailInsert.senderId = entity->GetObjectID();
		mailInsert.senderUsername = "Darkflame Universe";
		mailInsert.receiverId = receiverID;
		mailInsert.recipient = playerName;
		mailInsert.subject = "Lost item";
		mailInsert.body = "This is a replacement item for one you lost.";
		mailInsert.itemID = LWOOBJID_EMPTY;
		mailInsert.itemLOT = lot.value();
		mailInsert.itemSubkey = LWOOBJID_EMPTY;
		mailInsert.itemCount = 1;
		Database::Get()->InsertNewMail(mailInsert);

		ChatPackets::SendSystemMessage(sysAddr, u"Mail sent");
	}

	void ApproveProperty(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		if (PropertyManagementComponent::Instance() != nullptr) {
			PropertyManagementComponent::Instance()->UpdateApprovedStatus(true);
		}
	}

	void Mute(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');

		if (splitArgs.size() >= 1) {
			auto* player = PlayerManager::GetPlayer(splitArgs[0]);

			uint32_t accountId = 0;
			LWOOBJID characterId = 0;

			if (player == nullptr) {
				auto characterInfo = Database::Get()->GetCharacterInfo(splitArgs[0]);

				if (characterInfo) {
					accountId = characterInfo->accountId;
					characterId = characterInfo->id;

					GeneralUtils::SetBit(characterId, eObjectBits::CHARACTER);
					GeneralUtils::SetBit(characterId, eObjectBits::PERSISTENT);
				}

				if (accountId == 0) {
					ChatPackets::SendSystemMessage(sysAddr, u"Count not find player of name: " + GeneralUtils::UTF8ToUTF16(splitArgs[0]));

					return;
				}
			} else {
				auto* character = player->GetCharacter();
				auto* user = character != nullptr ? character->GetParentUser() : nullptr;
				if (user) accountId = user->GetAccountID();
				characterId = player->GetObjectID();
			}

			time_t expire = 1; // Default to indefinate mute

			if (splitArgs.size() >= 2) {
				const auto days = GeneralUtils::TryParse<uint32_t>(splitArgs[1]);
				if (!days) {
					ChatPackets::SendSystemMessage(sysAddr, u"Invalid days.");

					return;
				}

				std::optional<uint32_t> hours;
				if (splitArgs.size() >= 3) {
					hours = GeneralUtils::TryParse<uint32_t>(splitArgs[2]);
					if (!hours) {
						ChatPackets::SendSystemMessage(sysAddr, u"Invalid hours.");

						return;
					}
				}

				expire = time(NULL);
				expire += 24 * 60 * 60 * days.value();
				expire += 60 * 60 * hours.value_or(0);
			}

			if (accountId != 0) Database::Get()->UpdateAccountUnmuteTime(accountId, expire);

			char buffer[32] = "brought up for review.\0";

			if (expire != 1) {
				std::tm* ptm = std::localtime(&expire);
				// Format: Mo, 15.06.2009 20:20:00
				std::strftime(buffer, 32, "%a, %d.%m.%Y %H:%M:%S", ptm);
			}

			const auto timeStr = GeneralUtils::ASCIIToUTF16(std::string(buffer));

			ChatPackets::SendSystemMessage(sysAddr, u"Muted: " + GeneralUtils::UTF8ToUTF16(splitArgs[0]) + u" until " + timeStr);

			//Notify chat about it
			CBITSTREAM;
			BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, eChatMessageType::GM_MUTE);

			bitStream.Write(characterId);
			bitStream.Write(expire);

			Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
		} else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /mute <username> <days (optional)> <hours (optional)>");
		}
	}

	void Fly(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		auto* character = entity->GetCharacter();

		if (character) {
			bool isFlying = character->GetIsFlying();

			if (isFlying) {
				GameMessages::SendSetJetPackMode(entity, false);

				character->SetIsFlying(false);
			} else {
				float speedScale = 1.0f;

				if (splitArgs.size() >= 1) {
					const auto tempScaleStore = GeneralUtils::TryParse<float>(splitArgs.at(0));

					if (tempScaleStore) {
						speedScale = tempScaleStore.value();
					} else {
						ChatPackets::SendSystemMessage(sysAddr, u"Failed to parse speed scale argument.");
					}
				}

				float airSpeed = 20 * speedScale;
				float maxAirSpeed = 30 * speedScale;
				float verticalVelocity = 1.5 * speedScale;

				GameMessages::SendSetJetPackMode(entity, true, true, false, 167, airSpeed, maxAirSpeed, verticalVelocity);

				character->SetIsFlying(true);
			}
		}
	}

	void AttackImmune(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

		const auto state = GeneralUtils::TryParse<int32_t>(splitArgs[0]);

		if (!state) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid state.");
			return;
		}

		if (destroyableComponent) destroyableComponent->SetIsImmune(state.value());
	}

	void GmImmune(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

		const auto state = GeneralUtils::TryParse<int32_t>(splitArgs[0]);

		if (!state) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid state.");
			return;
		}

		if (destroyableComponent) destroyableComponent->SetIsGMImmune(state.value());
	}

	void GmInvis(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		GameMessages::SendToggleGMInvis(entity->GetObjectID(), true, UNASSIGNED_SYSTEM_ADDRESS);
	}

	void SetName(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		GameMessages::SendSetName(entity->GetObjectID(), GeneralUtils::UTF8ToUTF16(args), UNASSIGNED_SYSTEM_ADDRESS);
	}

	void Title(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		std::string name = entity->GetCharacter()->GetName() + " - " + args;
		GameMessages::SendSetName(entity->GetObjectID(), GeneralUtils::UTF8ToUTF16(name), UNASSIGNED_SYSTEM_ADDRESS);
	}

	void ShowAll(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		bool displayZoneData = true;
		bool displayIndividualPlayers = true;
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		
		if (!splitArgs.empty() && !splitArgs.at(0).empty()) displayZoneData = splitArgs.at(0) == "1";
		if (splitArgs.size() > 1) displayIndividualPlayers = splitArgs.at(1) == "1";

		ShowAllRequest request {
			.requestor = entity->GetObjectID(),
			.displayZoneData = displayZoneData,
			.displayIndividualPlayers = displayIndividualPlayers
		};

		CBITSTREAM;
		request.Serialize(bitStream);
		Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
	}

	void FindPlayer(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		if (args.empty()) {
			GameMessages::SendSlashCommandFeedbackText(entity, u"No player Given");
			return;
		}

		FindPlayerRequest request {
			.requestor = entity->GetObjectID(),
			.playerName = LUWString(args)
		};

		CBITSTREAM;
		request.Serialize(bitStream);
		Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
	}

	void Spectate(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		if (args.empty()) {
			GameMessages::SendForceCameraTargetCycle(entity, false, eCameraTargetCyclingMode::DISALLOW_CYCLING, entity->GetObjectID());
			return;
		}

		auto player = PlayerManager::GetPlayer(args);
		if (!player) {
			GameMessages::SendSlashCommandFeedbackText(entity, u"Player not found");
			return;
		}
		GameMessages::SendSlashCommandFeedbackText(entity, u"Spectating Player");
		GameMessages::SendForceCameraTargetCycle(entity, false, eCameraTargetCyclingMode::DISALLOW_CYCLING, player->GetObjectID());
	}
}
