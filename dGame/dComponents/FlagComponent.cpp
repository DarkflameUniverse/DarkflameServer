#include "FlagComponent.h"

#include "CDPlayerFlagsTable.h"

#include "eMissionTaskType.h"
#include "ePlayerFlag.h"

#include "MissionComponent.h"
#include "Amf3.h"

FlagComponent::FlagComponent(Entity* parent) : Component(parent) {
	RegisterMsg(MessageType::Game::SET_FLAG, this, &FlagComponent::OnSetFlag);
	RegisterMsg(MessageType::Game::GET_FLAG, this, &FlagComponent::OnGetFlag);
	RegisterMsg(MessageType::Game::CLEAR_SESSION_FLAGS, this, &FlagComponent::OnClearSessionFlags);
	RegisterMsg(MessageType::Game::SET_RETROACTIVE_FLAGS, this, &FlagComponent::OnSetRetroactiveFlags);
	RegisterMsg(MessageType::Game::GET_OBJECT_REPORT_INFO, this, &FlagComponent::OnGetObjectReportInfo);
}

bool FlagComponent::OnSetFlag(GameMessages::GameMsg& msg) {
	auto& setFlag = static_cast<GameMessages::SetFlag&>(msg);
	SetPlayerFlag(setFlag.iFlagId, setFlag.bFlag);

	// This is always set the first time a player loads into a world from character select
	// and is used to know when to refresh the players inventory items so they show up.
	if (setFlag.iFlagId == ePlayerFlag::IS_NEWS_SCREEN_VISIBLE && setFlag.bFlag) {
		m_Parent->SetVar<bool>(u"dlu_first_time_load", true);
	}

	return true;
}

bool FlagComponent::OnGetFlag(GameMessages::GameMsg& msg) {
	auto& getFlag = static_cast<GameMessages::GetFlag&>(msg);
	getFlag.bFlag = GetPlayerFlag(getFlag.iFlagId);
	return true;
}

void FlagComponent::UpdateXml(tinyxml2::XMLDocument& doc) {
	if (!doc.FirstChildElement("obj")) return;
	auto& obj = *doc.FirstChildElement("obj");

	if (!obj.FirstChildElement("flag")) {
		obj.InsertNewChildElement("flag");
	}

	auto& flags = *obj.FirstChildElement("flag");

	flags.DeleteChildren(); //Clear it if we have anything, so that we can fill it up again without dupes

	// Save our flags
	for (const auto& [index, flagBucket] : m_PlayerFlags) {
		auto& f = *flags.InsertNewChildElement("f");
		f.SetAttribute("id", index);
		f.SetAttribute("v", flagBucket);
	}

	// Save our session flags
	for (const auto& sessionFlag : m_SessionFlags) {
		auto& s = *flags.InsertNewChildElement("s");
		LOG("Saving session flag %i", sessionFlag);
		s.SetAttribute("si", sessionFlag);
	}
}

void FlagComponent::LoadFromXml(const tinyxml2::XMLDocument& doc) {
	if (!doc.FirstChildElement("obj")) return;
	auto& obj = *doc.FirstChildElement("obj");

	if (!obj.FirstChildElement("flag")) return;
	auto& flags = *obj.FirstChildElement("flag");

	const auto* currentChild = flags.FirstChildElement("f");
	while (currentChild) {
		const auto* temp = currentChild->Attribute("v");
		const auto* id = currentChild->Attribute("id");
		if (temp && id) {
			uint32_t index = 0;
			uint64_t value = 0;

			index = std::stoul(id);
			value = std::stoull(temp);

			m_PlayerFlags.insert(std::make_pair(index, value));
		}
		currentChild = currentChild->NextSiblingElement("f");
	}

	// Now load our session flags
	currentChild = flags.FirstChildElement("s");
	while (currentChild) {
		const auto* temp = currentChild->Attribute("si");
		if (temp) {
			uint32_t sessionIndex = 0;
			sessionIndex = std::stoul(temp);
			m_SessionFlags.insert(sessionIndex);
		}
		currentChild = currentChild->NextSiblingElement("s");
	}
}

void FlagComponent::SetPlayerFlag(const uint32_t flagId, const bool value) {
	// If the flag is already set, we don't have to recalculate it
	if (GetPlayerFlag(flagId) == value) return;

	if (value) {
		// Update the mission component:
		auto* missionComponent = m_Parent->GetComponent<MissionComponent>();

		if (missionComponent != nullptr) {
			missionComponent->Progress(eMissionTaskType::PLAYER_FLAG, flagId);
		}
	}

	const auto flagEntry = CDPlayerFlagsTable::GetEntry(flagId);

	if (flagEntry && flagEntry->sessionOnly) {
		if (value) m_SessionFlags.insert(flagId);
		else m_SessionFlags.erase(flagId);
	} else {
		// Calculate the index first
		auto flagIndex = uint32_t(std::floor(flagId / 64));

		const auto shiftedValue = 1ULL << flagId % 64;

		auto it = m_PlayerFlags.find(flagIndex);

		// Check if flag index exists
		if (it != m_PlayerFlags.end()) {
			// Update the value
			if (value) {
				it->second |= shiftedValue;
			} else {
				it->second &= ~shiftedValue;
			}
		} else {
			if (value) {
				// Otherwise, insert the value
				uint64_t flagValue = 0;

				flagValue |= shiftedValue;

				m_PlayerFlags.insert(std::make_pair(flagIndex, flagValue));
			}
		}
	}
	// Notify the client that a flag has changed server-side
	GameMessages::SendNotifyClientFlagChange(m_Parent->GetObjectID(), flagId, value, m_Parent->GetSystemAddress());
}

bool FlagComponent::GetPlayerFlag(const uint32_t flagId) const {
	bool toReturn = false; //by def, return false.

	const auto flagEntry = CDPlayerFlagsTable::GetEntry(flagId);
	if (flagEntry && flagEntry->sessionOnly) {
		toReturn = m_SessionFlags.contains(flagId);
	} else {
		// Calculate the index first
		const auto flagIndex = uint32_t(std::floor(flagId / 64));

		const auto shiftedValue = 1ULL << flagId % 64;

		auto it = m_PlayerFlags.find(flagIndex);
		if (it != m_PlayerFlags.end()) {
			// Don't set the data if we don't have to
			toReturn = (it->second & shiftedValue) != 0;
		}
	}

	return toReturn;
}

bool FlagComponent::OnSetRetroactiveFlags(GameMessages::GameMsg& msg) {
	// Retroactive check for if player has joined a faction to set their 'joined a faction' flag to true.
	if (GetPlayerFlag(ePlayerFlag::VENTURE_FACTION) ||
		GetPlayerFlag(ePlayerFlag::ASSEMBLY_FACTION) ||
		GetPlayerFlag(ePlayerFlag::PARADOX_FACTION) ||
		GetPlayerFlag(ePlayerFlag::SENTINEL_FACTION)) {
		SetPlayerFlag(ePlayerFlag::JOINED_A_FACTION, true);
	}
	return true;
}

bool FlagComponent::OnGetObjectReportInfo(GameMessages::GameMsg& msg) {
	auto& request = static_cast<GameMessages::GetObjectReportInfo&>(msg);

	auto& cmptType = request.info->PushDebug("Player Flag");
	cmptType.PushDebug<AMFIntValue>("Component ID") = GeneralUtils::ToUnderlying(ComponentType);

	auto& allFlags = cmptType.PushDebug("All flags");
	for (const auto& [id, flagChunk] : m_PlayerFlags) {
		const auto base = id * 64;
		auto flagChunkCopy = flagChunk;
		for (int i = 0; i < 64; i++) {
			if (static_cast<bool>(flagChunkCopy & 1)) {
				const int32_t flagId = base + i;
				std::stringstream stream;
				stream << "Flag: " << flagId;
				allFlags.PushDebug(stream.str().c_str());
			}
			flagChunkCopy >>= 1;
		}
	}

	return true;
}

void FlagComponent::ClearSessionFlags(tinyxml2::XMLDocument& doc) {
	if (!doc.FirstChildElement("obj")) return;
	auto& obj = *doc.FirstChildElement("obj");

	if (!obj.FirstChildElement("flag")) return;
	auto& flags = *obj.FirstChildElement("flag");

	auto* currentChild = flags.FirstChildElement();
	while (currentChild) {
		auto* nextChild = currentChild->NextSiblingElement();
		if (currentChild->Attribute("si")) {
			flags.DeleteChild(currentChild);
		}
		currentChild = nextChild;
	}
}
