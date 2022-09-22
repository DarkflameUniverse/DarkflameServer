#include "CharacterComponent.h"
#include <BitStream.h>
#include "tinyxml2.h"
#include "Game.h"
#include "dLogger.h"
#include "GeneralUtils.h"
#include "dServer.h"
#include "dZoneManager.h"
#include "CDClientManager.h"
#include "InventoryComponent.h"
#include "ControllablePhysicsComponent.h"
#include "EntityManager.h"
#include "VehiclePhysicsComponent.h"
#include "GameMessages.h"
#include "Item.h"

CharacterComponent::CharacterComponent(Entity* parent, Character* character) : Component(parent) {
	m_Character = character;

	m_IsRacing = false;
	m_IsGM = false;
	m_IsLanding = false;
	m_IsLEGOClubMember = true;

	m_DirtyCurrentActivity = false;
	m_DirtyGMInfo = false;
	m_DirtySocialInfo = false;

	m_PvpEnabled = false;
	m_GMLevel = character->GetGMLevel();

	m_EditorEnabled = false;
	m_EditorLevel = m_GMLevel;
	m_Reputation = 0;

	m_CurrentActivity = 0;
	m_CountryCode = 0;
	m_LastUpdateTimestamp = std::time(nullptr);
}

bool CharacterComponent::LandingAnimDisabled(int zoneID) {
	switch (zoneID) {
	case 0:
	case 556:
	case 1001:
	case 1101:
	case 1202:
	case 1203:
	case 1204:
	case 1301:
	case 1302:
	case 1303:
	case 1401:
	case 1402:
	case 1403:
	case 1603:
	case 2001:
		return true;

	default:
		return false;
	}

	return false;
}

CharacterComponent::~CharacterComponent() {
}

void CharacterComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {

	if (bIsInitialUpdate) {
		outBitStream->Write0();
		outBitStream->Write0();
		outBitStream->Write0();
		outBitStream->Write0();

		outBitStream->Write(m_Character->GetHairColor());
		outBitStream->Write(m_Character->GetHairStyle());
		outBitStream->Write<uint32_t>(0); //Default "head"
		outBitStream->Write(m_Character->GetShirtColor());
		outBitStream->Write(m_Character->GetPantsColor());
		outBitStream->Write(m_Character->GetShirtStyle());
		outBitStream->Write<uint32_t>(0); //Default "head color"
		outBitStream->Write(m_Character->GetEyebrows());
		outBitStream->Write(m_Character->GetEyes());
		outBitStream->Write(m_Character->GetMouth());
		outBitStream->Write<uint64_t>(0); //AccountID, trying out if 0 works.
		outBitStream->Write(m_Character->GetLastLogin()); //Last login
		outBitStream->Write<uint64_t>(0); //"prop mod last display time"
		outBitStream->Write<uint64_t>(m_Uscore); //u-score
		outBitStream->Write0(); //Not free-to-play (disabled in DLU)

		//Stats:
		outBitStream->Write(m_CurrencyCollected);
		outBitStream->Write(m_BricksCollected);
		outBitStream->Write(m_SmashablesSmashed);
		outBitStream->Write(m_QuickBuildsCompleted);
		outBitStream->Write(m_EnemiesSmashed);
		outBitStream->Write(m_RocketsUsed);
		outBitStream->Write(m_MissionsCompleted);
		outBitStream->Write(m_PetsTamed);
		outBitStream->Write(m_ImaginationPowerUpsCollected);
		outBitStream->Write(m_LifePowerUpsCollected);
		outBitStream->Write(m_ArmorPowerUpsCollected);
		outBitStream->Write(m_MetersTraveled);
		outBitStream->Write(m_TimesSmashed);
		outBitStream->Write(m_TotalDamageTaken);
		outBitStream->Write(m_TotalDamageHealed);
		outBitStream->Write(m_TotalArmorRepaired);
		outBitStream->Write(m_TotalImaginationRestored);
		outBitStream->Write(m_TotalImaginationUsed);
		outBitStream->Write(m_DistanceDriven);
		outBitStream->Write(m_TimeAirborneInCar);
		outBitStream->Write(m_RacingImaginationPowerUpsCollected);
		outBitStream->Write(m_RacingImaginationCratesSmashed);
		outBitStream->Write(m_RacingCarBoostsActivated);
		outBitStream->Write(m_RacingTimesWrecked);
		outBitStream->Write(m_RacingSmashablesSmashed);
		outBitStream->Write(m_RacesFinished);
		outBitStream->Write(m_FirstPlaceRaceFinishes);

		outBitStream->Write0();
		outBitStream->Write(m_IsLanding);
		if (m_IsLanding) {
			outBitStream->Write(uint16_t(m_LastRocketConfig.size()));
			for (uint16_t character : m_LastRocketConfig) {
				outBitStream->Write(character);
			}
		}
	}

	outBitStream->Write(m_DirtyGMInfo);
	if (m_DirtyGMInfo) {
		outBitStream->Write(m_PvpEnabled);
		outBitStream->Write(m_IsGM);
		outBitStream->Write(m_GMLevel);
		outBitStream->Write(m_EditorEnabled);
		outBitStream->Write(m_EditorLevel);
	}

	outBitStream->Write(m_DirtyCurrentActivity);
	if (m_DirtyCurrentActivity) outBitStream->Write(m_CurrentActivity);

	outBitStream->Write(m_DirtySocialInfo);
	if (m_DirtySocialInfo) {
		outBitStream->Write(m_GuildID);
		outBitStream->Write<unsigned char>(static_cast<unsigned char>(m_GuildName.size()));
		if (!m_GuildName.empty())
			outBitStream->WriteBits(reinterpret_cast<const unsigned char*>(m_GuildName.c_str()), static_cast<unsigned char>(m_GuildName.size()) * sizeof(wchar_t) * 8);

		outBitStream->Write(m_IsLEGOClubMember);
		outBitStream->Write(m_CountryCode);
	}
}

bool CharacterComponent::GetPvpEnabled() const {
	return m_PvpEnabled;
}

void CharacterComponent::SetPvpEnabled(const bool value) {
	m_DirtyGMInfo = true;

	m_PvpEnabled = value;
}

void CharacterComponent::SetGMLevel(int gmlevel) {
	m_DirtyGMInfo = true;
	if (gmlevel > 0) m_IsGM = true;
	else m_IsGM = false;
	m_GMLevel = gmlevel;
}

void CharacterComponent::LoadFromXml(tinyxml2::XMLDocument* doc) {

	tinyxml2::XMLElement* character = doc->FirstChildElement("obj")->FirstChildElement("char");
	if (!character) {
		Game::logger->Log("CharacterComponent", "Failed to find char tag while loading XML!");
		return;
	}
	if (character->QueryAttribute("rpt", &m_Reputation) == tinyxml2::XML_NO_ATTRIBUTE) {
		SetReputation(0);
	}

	character->QueryInt64Attribute("ls", &m_Uscore);

	// Load the statistics
	const auto* statisticsAttribute = character->FindAttribute("stt");
	if (statisticsAttribute) {
		InitializeStatisticsFromString(std::string(statisticsAttribute->Value()));
	} else {
		InitializeEmptyStatistics();
	}

	// Load the zone statistics
	m_ZoneStatistics = {};
	auto zoneStatistics = character->FirstChildElement("zs");

	if (zoneStatistics) {
		auto child = zoneStatistics->FirstChildElement();
		while (child) {
			ZoneStatistics statistics = {};

			child->QueryUnsigned64Attribute("ac", &statistics.m_AchievementsCollected);
			child->QueryInt64Attribute("bc", &statistics.m_BricksCollected);
			child->QueryUnsigned64Attribute("cc", &statistics.m_CoinsCollected);
			child->QueryUnsigned64Attribute("es", &statistics.m_EnemiesSmashed);
			child->QueryUnsigned64Attribute("qbc", &statistics.m_QuickBuildsCompleted);

			uint32_t mapID;
			child->QueryAttribute("map", &mapID);

			m_ZoneStatistics.insert({ (LWOMAPID)mapID, statistics });

			child = child->NextSiblingElement();
		}
	}

	const tinyxml2::XMLAttribute* rocketConfig = character->FindAttribute("lcbp");

	if (rocketConfig) {
		m_LastRocketConfig = GeneralUtils::ASCIIToUTF16(rocketConfig->Value());
	} else {
		m_LastRocketConfig = u"";
	}

	//
	// Begin custom attributes
	//

	// Load the last rocket item ID
	const tinyxml2::XMLAttribute* lastRocketItemID = character->FindAttribute("lrid");
	if (lastRocketItemID) {
		m_LastRocketItemID = lastRocketItemID->Int64Value();
	}

	//
	// End custom attributes
	//

	if (m_GMLevel > 0) {
		m_IsGM = true;
		m_DirtyGMInfo = true;
		m_EditorLevel = m_GMLevel;
		m_EditorEnabled = false; //We're not currently in HF if we're loading in
	}

	//Annoying guild bs:
	const tinyxml2::XMLAttribute* guildName = character->FindAttribute("gn");
	if (guildName) {
		const char* gn = guildName->Value();
		int64_t gid = 0;
		character->QueryInt64Attribute("gid", &gid);
		if (gid != 0) {
			std::string guildname(gn);
			m_GuildName = GeneralUtils::UTF8ToUTF16(guildname);
			m_GuildID = gid;
			m_DirtySocialInfo = true;
		}
	}

	if (character->FindAttribute("time")) {
		character->QueryUnsigned64Attribute("time", &m_TotalTimePlayed);
	} else {
		m_TotalTimePlayed = 0;
	}

	if (!m_Character) return;

	//Check to see if we're landing:
	if (m_Character->GetZoneID() != Game::server->GetZoneID()) {
		m_IsLanding = true;
	}

	if (LandingAnimDisabled(m_Character->GetZoneID()) || LandingAnimDisabled(Game::server->GetZoneID()) || m_LastRocketConfig.empty()) {
		m_IsLanding = false; //Don't make us land on VE/minigames lol
	}
}

void CharacterComponent::UpdateXml(tinyxml2::XMLDocument* doc) {
	tinyxml2::XMLElement* minifig = doc->FirstChildElement("obj")->FirstChildElement("mf");
	if (!minifig) {
		Game::logger->Log("CharacterComponent", "Failed to find mf tag while updating XML!");
		return;
	}

	// write minifig information that might have been changed by commands

	minifig->SetAttribute("es", m_Character->GetEyebrows());
	minifig->SetAttribute("ess", m_Character->GetEyes());
	minifig->SetAttribute("hc", m_Character->GetHairColor());
	minifig->SetAttribute("hs", m_Character->GetHairStyle());
	minifig->SetAttribute("l", m_Character->GetPantsColor());
	minifig->SetAttribute("lh", m_Character->GetLeftHand());
	minifig->SetAttribute("ms", m_Character->GetMouth());
	minifig->SetAttribute("rh", m_Character->GetRightHand());
	minifig->SetAttribute("t", m_Character->GetShirtColor());

	// done with minifig

	tinyxml2::XMLElement* character = doc->FirstChildElement("obj")->FirstChildElement("char");
	if (!character) {
		Game::logger->Log("CharacterComponent", "Failed to find char tag while updating XML!");
		return;
	}

	character->SetAttribute("ls", m_Uscore);
	// Custom attribute to keep track of reputation.
	character->SetAttribute("rpt", GetReputation());
	character->SetAttribute("stt", StatisticsToString().c_str());

	// Set the zone statistics of the form <zs><s/> ... <s/></zs>
	auto zoneStatistics = character->FirstChildElement("zs");
	if (!zoneStatistics) zoneStatistics = doc->NewElement("zs");
	zoneStatistics->DeleteChildren();

	for (auto pair : m_ZoneStatistics) {
		auto zoneStatistic = doc->NewElement("s");

		zoneStatistic->SetAttribute("map", pair.first);
		zoneStatistic->SetAttribute("ac", pair.second.m_AchievementsCollected);
		zoneStatistic->SetAttribute("bc", pair.second.m_BricksCollected);
		zoneStatistic->SetAttribute("cc", pair.second.m_CoinsCollected);
		zoneStatistic->SetAttribute("es", pair.second.m_EnemiesSmashed);
		zoneStatistic->SetAttribute("qbc", pair.second.m_QuickBuildsCompleted);

		zoneStatistics->LinkEndChild(zoneStatistic);
	}

	character->LinkEndChild(zoneStatistics);

	if (!m_LastRocketConfig.empty()) {
		std::string config = GeneralUtils::UTF16ToWTF8(m_LastRocketConfig);
		character->SetAttribute("lcbp", config.c_str());
	} else {
		character->DeleteAttribute("lcbp");
	}

	//
	// Begin custom attributes
	//

	// Store last rocket item ID
	character->SetAttribute("lrid", m_LastRocketItemID);

	//
	// End custom attributes
	//

	auto newUpdateTimestamp = std::time(nullptr);
	Game::logger->Log("TotalTimePlayed", "Time since last save: %d", newUpdateTimestamp - m_LastUpdateTimestamp);

	m_TotalTimePlayed += newUpdateTimestamp - m_LastUpdateTimestamp;
	character->SetAttribute("time", m_TotalTimePlayed);

	m_LastUpdateTimestamp = newUpdateTimestamp;
}

void CharacterComponent::SetLastRocketConfig(std::u16string config) {
	m_IsLanding = !config.empty();
	m_LastRocketConfig = config;
}

Item* CharacterComponent::GetRocket(Entity* player) {
	Item* rocket = nullptr;

	auto* inventoryComponent = player->GetComponent<InventoryComponent>();

	if (!inventoryComponent) return rocket;

	// Select the rocket
	if (!rocket) {
		rocket = inventoryComponent->FindItemById(GetLastRocketItemID());
	}

	if (!rocket) {
		rocket = inventoryComponent->FindItemByLot(6416);
	}

	if (!rocket) {
		Game::logger->Log("CharacterComponent", "Unable to find rocket to equip!");
		return rocket;
	}
	return rocket;
}

Item* CharacterComponent::RocketEquip(Entity* player) {
	Item* rocket = GetRocket(player);
	if (!rocket) return rocket;

	// build and define the rocket config
	for (LDFBaseData* data : rocket->GetConfig()) {
		if (data->GetKey() == u"assemblyPartLOTs") {
			std::string newRocketStr = data->GetValueAsString() + ";";
			GeneralUtils::ReplaceInString(newRocketStr, "+", ";");
			SetLastRocketConfig(GeneralUtils::ASCIIToUTF16(newRocketStr));
		}
	}

	// Store the last used rocket item's ID
	SetLastRocketItemID(rocket->GetId());
	// carry the rocket
	rocket->Equip(true);
	return rocket;
}

void CharacterComponent::RocketUnEquip(Entity* player) {
	Item* rocket = GetRocket(player);
	if (!rocket) return;
	// We don't want to carry it anymore
	rocket->UnEquip();
}

void CharacterComponent::TrackMissionCompletion(bool isAchievement) {
	UpdatePlayerStatistic(MissionsCompleted);

	// Achievements are tracked separately for the zone
	if (isAchievement) {
		const auto mapID = dZoneManager::Instance()->GetZoneID().GetMapID();
		GetZoneStatisticsForMap(mapID).m_AchievementsCollected++;
	}
}

void CharacterComponent::TrackLOTCollection(LOT lot) {
	switch (lot) {
		// Handle all the imagination powerup lots
	case 935:   // 1 point
	case 4035:  // 2 points
	case 11910: // 3 points
	case 11911: // 5 points
	case 11918: // 10 points
		UpdatePlayerStatistic(ImaginationPowerUpsCollected);
		break;
		// Handle all the armor powerup lots
	case 6431:  // 1 point
	case 11912: // 2 points
	case 11913: // 3 points
	case 11914: // 5 points
	case 11919: // 10 points
		UpdatePlayerStatistic(ArmorPowerUpsCollected);
		break;
		// Handle all the life powerup lots
	case 177:   // 1 point
	case 11915: // 2 points
	case 11916: // 3 points
	case 11917: // 5 points
	case 11920: // 10 points
		UpdatePlayerStatistic(LifePowerUpsCollected);
		break;
	default:
		break;
	}
}

void CharacterComponent::TrackHealthDelta(int32_t health) {
	if (health > 0) {
		UpdatePlayerStatistic(TotalDamageHealed, health);
	} else {
		UpdatePlayerStatistic(TotalDamageTaken, -health);
	}
}

void CharacterComponent::TrackImaginationDelta(int32_t imagination) {
	if (imagination > 0) {
		UpdatePlayerStatistic(TotalImaginationRestored, imagination);
	} else {
		UpdatePlayerStatistic(TotalImaginationUsed, -imagination);
	}
}

void CharacterComponent::TrackArmorDelta(int32_t armor) {
	if (armor > 0) {
		UpdatePlayerStatistic(TotalArmorRepaired, armor);
	}
}

void CharacterComponent::TrackRebuildComplete() {
	UpdatePlayerStatistic(QuickBuildsCompleted);

	const auto mapID = dZoneManager::Instance()->GetZoneID().GetMapID();
	GetZoneStatisticsForMap(mapID).m_QuickBuildsCompleted++;
}

void CharacterComponent::TrackRaceCompleted(bool won) {
	m_RacesFinished++;
	if (won)
		m_FirstPlaceRaceFinishes++;
}

void CharacterComponent::TrackPositionUpdate(const NiPoint3& newPosition) {
	const auto distance = NiPoint3::Distance(newPosition, m_Parent->GetPosition());

	if (m_IsRacing) {
		UpdatePlayerStatistic(DistanceDriven, (uint64_t)distance);
	} else {
		UpdatePlayerStatistic(MetersTraveled, (uint64_t)distance);
	}
}

void CharacterComponent::HandleZoneStatisticsUpdate(LWOMAPID zoneID, const std::u16string& name, int32_t value) {
	auto zoneStatistics = &GetZoneStatisticsForMap(zoneID);

	if (name == u"BricksCollected") {
		m_BricksCollected += value;
		zoneStatistics->m_BricksCollected += value;
	} else if (name == u"CoinsCollected") {
		m_CurrencyCollected += value;
		zoneStatistics->m_CoinsCollected += value;
	} else if (name == u"EnemiesSmashed") {
		m_EnemiesSmashed += value;
		zoneStatistics->m_EnemiesSmashed += value;
	}
}

void CharacterComponent::UpdatePlayerStatistic(StatisticID updateID, uint64_t updateValue) {
	switch (updateID) {
	case CurrencyCollected:
		m_CurrencyCollected += updateValue;
		break;
	case BricksCollected:
		m_BricksCollected += updateValue;
		break;
	case SmashablesSmashed:
		m_SmashablesSmashed += updateValue;
		break;
	case QuickBuildsCompleted:
		m_QuickBuildsCompleted += updateValue;
		break;
	case EnemiesSmashed:
		m_EnemiesSmashed += updateValue;
		break;
	case RocketsUsed:
		m_RocketsUsed += updateValue;
		break;
	case MissionsCompleted:
		m_MissionsCompleted += updateValue;
		break;
	case PetsTamed:
		m_PetsTamed += updateValue;
		break;
	case ImaginationPowerUpsCollected:
		m_ImaginationPowerUpsCollected += updateValue;
		break;
	case LifePowerUpsCollected:
		m_LifePowerUpsCollected += updateValue;
		break;
	case ArmorPowerUpsCollected:
		m_ArmorPowerUpsCollected += updateValue;
		break;
	case MetersTraveled:
		m_MetersTraveled += updateValue;
		break;
	case TimesSmashed:
		m_TimesSmashed += updateValue;
		break;
	case TotalDamageTaken:
		m_TotalDamageTaken += updateValue;
		break;
	case TotalDamageHealed:
		m_TotalDamageHealed += updateValue;
		break;
	case TotalArmorRepaired:
		m_TotalArmorRepaired += updateValue;
		break;
	case TotalImaginationRestored:
		m_TotalImaginationRestored += updateValue;
		break;
	case TotalImaginationUsed:
		m_TotalImaginationUsed += updateValue;
		break;
	case DistanceDriven:
		m_DistanceDriven += updateValue;
		break;
	case TimeAirborneInCar:
		m_TimeAirborneInCar += updateValue;
		break;
	case RacingImaginationPowerUpsCollected:
		m_RacingImaginationPowerUpsCollected += updateValue;
		break;
	case RacingImaginationCratesSmashed:
		m_RacingImaginationCratesSmashed += updateValue;
		break;
	case RacingCarBoostsActivated:
		m_RacingCarBoostsActivated += updateValue;
		break;
	case RacingTimesWrecked:
		m_RacingTimesWrecked += updateValue;
		break;
	case RacingSmashablesSmashed:
		m_RacingSmashablesSmashed += updateValue;
		break;
	case RacesFinished:
		m_RacesFinished += updateValue;
		break;
	case FirstPlaceRaceFinishes:
		m_FirstPlaceRaceFinishes += updateValue;
		break;
	default:
		break;
	}
}

void CharacterComponent::InitializeStatisticsFromString(const std::string& statisticsString) {
	auto split = GeneralUtils::SplitString(statisticsString, ';');

	m_CurrencyCollected = GetStatisticFromSplit(split, 0);
	m_BricksCollected = GetStatisticFromSplit(split, 1);
	m_SmashablesSmashed = GetStatisticFromSplit(split, 2);
	m_QuickBuildsCompleted = GetStatisticFromSplit(split, 3);
	m_EnemiesSmashed = GetStatisticFromSplit(split, 4);
	m_RocketsUsed = GetStatisticFromSplit(split, 5);
	m_MissionsCompleted = GetStatisticFromSplit(split, 6);
	m_PetsTamed = GetStatisticFromSplit(split, 7);
	m_ImaginationPowerUpsCollected = GetStatisticFromSplit(split, 8);
	m_LifePowerUpsCollected = GetStatisticFromSplit(split, 9);
	m_ArmorPowerUpsCollected = GetStatisticFromSplit(split, 10);
	m_MetersTraveled = GetStatisticFromSplit(split, 11);
	m_TimesSmashed = GetStatisticFromSplit(split, 12);
	m_TotalDamageTaken = GetStatisticFromSplit(split, 13);
	m_TotalDamageHealed = GetStatisticFromSplit(split, 14);
	m_TotalArmorRepaired = GetStatisticFromSplit(split, 15);
	m_TotalImaginationRestored = GetStatisticFromSplit(split, 16);
	m_TotalImaginationUsed = GetStatisticFromSplit(split, 17);
	m_DistanceDriven = GetStatisticFromSplit(split, 18);
	m_TimeAirborneInCar = GetStatisticFromSplit(split, 19); // WONTFIX
	m_RacingImaginationPowerUpsCollected = GetStatisticFromSplit(split, 20);
	m_RacingImaginationCratesSmashed = GetStatisticFromSplit(split, 21);
	m_RacingCarBoostsActivated = GetStatisticFromSplit(split, 22);
	m_RacingTimesWrecked = GetStatisticFromSplit(split, 23);
	m_RacingSmashablesSmashed = GetStatisticFromSplit(split, 24);
	m_RacesFinished = GetStatisticFromSplit(split, 25);
	m_FirstPlaceRaceFinishes = GetStatisticFromSplit(split, 26);
}

void CharacterComponent::InitializeEmptyStatistics() {
	m_CurrencyCollected = 0;
	m_BricksCollected = 0;
	m_SmashablesSmashed = 0;
	m_QuickBuildsCompleted = 0;
	m_EnemiesSmashed = 0;
	m_RocketsUsed = 0;
	m_MissionsCompleted = 0;
	m_PetsTamed = 0;
	m_ImaginationPowerUpsCollected = 0;
	m_LifePowerUpsCollected = 0;
	m_ArmorPowerUpsCollected = 0;
	m_MetersTraveled = 0;
	m_TimesSmashed = 0;
	m_TotalDamageTaken = 0;
	m_TotalDamageHealed = 0;
	m_TotalArmorRepaired = 0;
	m_TotalImaginationRestored = 0;
	m_TotalImaginationUsed = 0;
	m_DistanceDriven = 0;
	m_TimeAirborneInCar = 0;
	m_RacingImaginationPowerUpsCollected = 0;
	m_RacingImaginationCratesSmashed = 0;
	m_RacingCarBoostsActivated = 0;
	m_RacingTimesWrecked = 0;
	m_RacingSmashablesSmashed = 0;
	m_RacesFinished = 0;
	m_FirstPlaceRaceFinishes = 0;
}

std::string CharacterComponent::StatisticsToString() const {
	std::stringstream result;
	result << std::to_string(m_CurrencyCollected) << ';'
		<< std::to_string(m_BricksCollected) << ';'
		<< std::to_string(m_SmashablesSmashed) << ';'
		<< std::to_string(m_QuickBuildsCompleted) << ';'
		<< std::to_string(m_EnemiesSmashed) << ';'
		<< std::to_string(m_RocketsUsed) << ';'
		<< std::to_string(m_MissionsCompleted) << ';'
		<< std::to_string(m_PetsTamed) << ';'
		<< std::to_string(m_ImaginationPowerUpsCollected) << ';'
		<< std::to_string(m_LifePowerUpsCollected) << ';'
		<< std::to_string(m_ArmorPowerUpsCollected) << ';'
		<< std::to_string(m_MetersTraveled) << ';'
		<< std::to_string(m_TimesSmashed) << ';'
		<< std::to_string(m_TotalDamageTaken) << ';'
		<< std::to_string(m_TotalDamageHealed) << ';'
		<< std::to_string(m_TotalArmorRepaired) << ';'
		<< std::to_string(m_TotalImaginationRestored) << ';'
		<< std::to_string(m_TotalImaginationUsed) << ';'
		<< std::to_string(m_DistanceDriven) << ';'
		<< std::to_string(m_TimeAirborneInCar) << ';'
		<< std::to_string(m_RacingImaginationPowerUpsCollected) << ';'
		<< std::to_string(m_RacingImaginationCratesSmashed) << ';'
		<< std::to_string(m_RacingCarBoostsActivated) << ';'
		<< std::to_string(m_RacingTimesWrecked) << ';'
		<< std::to_string(m_RacingSmashablesSmashed) << ';'
		<< std::to_string(m_RacesFinished) << ';'
		<< std::to_string(m_FirstPlaceRaceFinishes) << ';';

	return result.str();
}

uint64_t CharacterComponent::GetStatisticFromSplit(std::vector<std::string> split, uint32_t index) {
	return split.size() > index ? std::stoul(split.at(index)) : 0;
}

ZoneStatistics& CharacterComponent::GetZoneStatisticsForMap(LWOMAPID mapID) {
	auto stats = m_ZoneStatistics.find(mapID);
	if (stats == m_ZoneStatistics.end())
		m_ZoneStatistics.insert({ mapID, {0, 0, 0, 0, 0 } });
	return m_ZoneStatistics.at(mapID);
}

void CharacterComponent::AddVentureVisionEffect(std::string ventureVisionType) {
	const auto ventureVisionTypeIterator = m_ActiveVentureVisionEffects.find(ventureVisionType);

	if (ventureVisionTypeIterator != m_ActiveVentureVisionEffects.end()) {
		ventureVisionTypeIterator->second = ++ventureVisionTypeIterator->second;
	} else {
		// If the effect it not found, insert it into the active effects.
		m_ActiveVentureVisionEffects.insert(std::make_pair(ventureVisionType, 1U));
	}

	UpdateClientMinimap(true, ventureVisionType);
}

void CharacterComponent::RemoveVentureVisionEffect(std::string ventureVisionType) {
	const auto ventureVisionTypeIterator = m_ActiveVentureVisionEffects.find(ventureVisionType);

	if (ventureVisionTypeIterator != m_ActiveVentureVisionEffects.end()) {
		ventureVisionTypeIterator->second = --ventureVisionTypeIterator->second;
		UpdateClientMinimap(ventureVisionTypeIterator->second != 0U, ventureVisionType);
	}
}

void CharacterComponent::UpdateClientMinimap(bool showFaction, std::string ventureVisionType) const {
	if (!m_Parent) return;
	AMFArrayValue arrayToSend;
	arrayToSend.InsertValue(ventureVisionType, showFaction ? static_cast<AMFValue*>(new AMFTrueValue()) : static_cast<AMFValue*>(new AMFFalseValue()));
	GameMessages::SendUIMessageServerToSingleClient(m_Parent, m_Parent ? m_Parent->GetSystemAddress() : UNASSIGNED_SYSTEM_ADDRESS, "SetFactionVisibility", &arrayToSend);
}
