#include "BuffComponent.h"
#include "BitStream.h"
#include "CDClientDatabase.h"
#include <stdexcept>
#include "DestroyableComponent.h"
#include "Game.h"
#include "Logger.h"
#include "GameMessages.h"
#include "SkillComponent.h"
#include "ControllablePhysicsComponent.h"
#include "EntityManager.h"
#include "CDClientManager.h"
#include "CDSkillBehaviorTable.h"
#include "TeamManager.h"

std::unordered_map<int32_t, std::vector<BuffParameter>> BuffComponent::m_Cache{};

namespace {
	std::map<std::string, std::string> BuffFx = {
		{ "overtime", "OTB_" },
		{ "max_health", "HEALTH_" },
		{ "max_imagination", "IMAGINATION_" },
		{ "max_armor", "ARMOR_" },
		{ "speed", "SPEED_" },
		{ "loot", "LOOT_" }
	};
}

BuffComponent::BuffComponent(Entity* parent) : Component(parent) {
}

BuffComponent::~BuffComponent() {
}

void BuffComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	if (!bIsInitialUpdate) return;
	outBitStream.Write(!m_Buffs.empty());
	if (!m_Buffs.empty()) {
		outBitStream.Write<uint32_t>(m_Buffs.size());

		for (const auto& [id, buff] : m_Buffs) {
			outBitStream.Write<uint32_t>(id);
			outBitStream.Write(buff.time != 0.0f);
			if (buff.time != 0.0f) outBitStream.Write<uint32_t>(buff.time * 1000.0f);
			outBitStream.Write(buff.cancelOnDeath);
			outBitStream.Write(buff.cancelOnZone);
			outBitStream.Write(buff.cancelOnDamaged);
			outBitStream.Write(buff.cancelOnRemoveBuff);
			outBitStream.Write(buff.cancelOnUi);
			outBitStream.Write(buff.cancelOnLogout);
			outBitStream.Write(buff.cancelOnUnequip);
			outBitStream.Write0(); // Cancel on Damage Absorb Ran Out. Generally false from what I can tell

			auto* team = TeamManager::Instance()->GetTeam(buff.source);
			bool addedByTeammate = false;
			if (team) {
				addedByTeammate = std::count(team->members.begin(), team->members.end(), m_Parent->GetObjectID()) > 0;
			}

			outBitStream.Write(addedByTeammate); // Added by teammate. If source is in the same team as the target, this is true. Otherwise, false.
			outBitStream.Write(buff.applyOnTeammates);
			if (addedByTeammate) outBitStream.Write(buff.source);

			outBitStream.Write<uint32_t>(buff.refCount);
		}
	}

	outBitStream.Write0(); // something to do with immunity buffs?
}

void BuffComponent::Update(float deltaTime) {
	/**
	 * Loop through all buffs and apply deltaTime to ther time.
	 * If they have expired, remove the buff and break.
	 */
	for (auto& buff : m_Buffs) {
		// For damage buffs
		if (buff.second.tick != 0.0f && buff.second.stacks > 0) {
			buff.second.tickTime -= deltaTime;

			if (buff.second.tickTime <= 0.0f) {
				buff.second.tickTime = buff.second.tick;
				buff.second.stacks--;

				SkillComponent::HandleUnmanaged(buff.second.behaviorID, m_Parent->GetObjectID(), buff.second.source);
			}
		}

		// These are indefinate buffs, don't update them.
		if (buff.second.time == 0.0f) {
			continue;
		}

		buff.second.time -= deltaTime;

		if (buff.second.time <= 0.0f) {
			RemoveBuff(buff.first);
		}
	}

	if (m_BuffsToRemove.empty()) return;

	for (const auto& buff : m_BuffsToRemove) {
		m_Buffs.erase(buff);
	}

	m_BuffsToRemove.clear();
}

const std::string& GetFxName(const std::string& buffname) {
	const auto& toReturn = BuffFx[buffname];
	if (toReturn.empty()) {
		LOG_DEBUG("No fx name for %s", buffname.c_str());
	}
	return toReturn;
}

void BuffComponent::ApplyBuffFx(uint32_t buffId, const BuffParameter& buff) {
	std::string fxToPlay;
	const auto& buffName = GetFxName(buff.name);

	if (buffName.empty()) return;

	fxToPlay += std::to_string(buffId);
	LOG_DEBUG("Playing %s %i", fxToPlay.c_str(), buff.effectId);
	GameMessages::SendPlayFXEffect(m_Parent->GetObjectID(), buff.effectId, u"cast", fxToPlay, LWOOBJID_EMPTY, 1.07f, 1.0f, false);
}

void BuffComponent::RemoveBuffFx(uint32_t buffId, const BuffParameter& buff) {
	std::string fxToPlay;
	const auto& buffName = GetFxName(buff.name);

	if (buffName.empty()) return;

	fxToPlay += std::to_string(buffId);
	LOG_DEBUG("Stopping %s", fxToPlay.c_str());
	GameMessages::SendStopFXEffect(m_Parent, false, fxToPlay);
}

void BuffComponent::ApplyBuff(const int32_t id, const float duration, const LWOOBJID source, bool addImmunity,
	bool cancelOnDamaged, bool cancelOnDeath, bool cancelOnLogout, bool cancelOnRemoveBuff,
	bool cancelOnUi, bool cancelOnUnequip, bool cancelOnZone, bool applyOnTeammates) {
	// Prevent buffs from stacking.
	if (HasBuff(id)) {
		m_Buffs[id].refCount++;
		m_Buffs[id].time = duration;
		return;
	}

	auto* team = TeamManager::Instance()->GetTeam(source);
	bool addedByTeammate = false;
	if (team) {
		addedByTeammate = std::count(team->members.begin(), team->members.end(), m_Parent->GetObjectID()) > 0;
	}

	GameMessages::SendAddBuff(const_cast<LWOOBJID&>(m_Parent->GetObjectID()), source, static_cast<uint32_t>(id),
		static_cast<uint32_t>(duration) * 1000, addImmunity, cancelOnDamaged, cancelOnDeath,
		cancelOnLogout, cancelOnRemoveBuff, cancelOnUi, cancelOnUnequip, cancelOnZone, addedByTeammate, applyOnTeammates);

	float tick = 0;
	float stacks = 0;
	int32_t behaviorID = 0;

	const auto& parameters = GetBuffParameters(id);
	for (const auto& parameter : parameters) {
		if (parameter.name == "overtime") {
			auto* behaviorTemplateTable = CDClientManager::GetTable<CDSkillBehaviorTable>();

			behaviorID = behaviorTemplateTable->GetSkillByID(parameter.values[0]).behaviorID;
			stacks = static_cast<int32_t>(parameter.values[1]);
			tick = parameter.values[2];
			const auto unknown2 = parameter.values[3]; // Always 0
		}
	}

	ApplyBuffEffect(id);

	Buff buff;
	buff.id = id;
	buff.time = duration;
	buff.tick = tick;
	buff.tickTime = tick;
	buff.stacks = stacks;
	buff.source = source;
	buff.behaviorID = behaviorID;
	buff.cancelOnDamaged = cancelOnDamaged;
	buff.cancelOnDeath = cancelOnDeath;
	buff.cancelOnLogout = cancelOnLogout;
	buff.cancelOnRemoveBuff = cancelOnRemoveBuff;
	buff.cancelOnUi = cancelOnUi;
	buff.cancelOnUnequip = cancelOnUnequip;
	buff.cancelOnZone = cancelOnZone;
	buff.refCount = 1;

	m_Buffs.emplace(id, buff);

	auto* parent = GetParent();
	if (!cancelOnDeath) return;

	m_Parent->AddDieCallback([parent, id]() {
		LOG_DEBUG("Removing buff %i because parent died", id);
		if (!parent) return;
		auto* buffComponent = parent->GetComponent<BuffComponent>();
		if (buffComponent) buffComponent->RemoveBuff(id, false, false, true);
		});
}

void BuffComponent::RemoveBuff(int32_t id, bool fromUnEquip, bool removeImmunity, bool ignoreRefCount) {
	const auto& iter = m_Buffs.find(id);

	// If the buff is already scheduled to be removed, don't do it again
	if (iter == m_Buffs.end() || m_BuffsToRemove.contains(id)) return;

	if (!ignoreRefCount && !iter->second.cancelOnRemoveBuff) {
		iter->second.refCount--;
		LOG_DEBUG("refCount for buff %i is now %i", id, iter->second.refCount);
		if (iter->second.refCount > 0) {
			return;
		}
	}

	GameMessages::SendRemoveBuff(m_Parent, fromUnEquip, removeImmunity, id);

	m_BuffsToRemove.insert(id);

	RemoveBuffEffect(id);
}

bool BuffComponent::HasBuff(int32_t id) {
	return m_Buffs.find(id) != m_Buffs.end();
}

void BuffComponent::ApplyBuffEffect(int32_t id) {
	const auto& parameters = GetBuffParameters(id);
	for (const auto& parameter : parameters) {
		ApplyBuffFx(id, parameter);
		if (parameter.name == "max_health") {
			const auto maxHealth = parameter.value;

			auto* destroyable = this->GetParent()->GetComponent<DestroyableComponent>();

			if (destroyable == nullptr) return;

			destroyable->SetMaxHealth(destroyable->GetMaxHealth() + maxHealth);
		} else if (parameter.name == "max_armor") {
			const auto maxArmor = parameter.value;

			auto* destroyable = this->GetParent()->GetComponent<DestroyableComponent>();

			if (destroyable == nullptr) return;

			destroyable->SetMaxArmor(destroyable->GetMaxArmor() + maxArmor);
		} else if (parameter.name == "max_imagination") {
			const auto maxImagination = parameter.value;

			auto* destroyable = this->GetParent()->GetComponent<DestroyableComponent>();

			if (destroyable == nullptr) return;

			destroyable->SetMaxImagination(destroyable->GetMaxImagination() + maxImagination);
		} else if (parameter.name == "speed") {
			auto* controllablePhysicsComponent = this->GetParent()->GetComponent<ControllablePhysicsComponent>();
			if (!controllablePhysicsComponent) return;
			const auto speed = parameter.value;
			controllablePhysicsComponent->AddSpeedboost(speed);
		}
	}
}

void BuffComponent::RemoveBuffEffect(int32_t id) {
	const auto& parameters = GetBuffParameters(id);
	for (const auto& parameter : parameters) {
		RemoveBuffFx(id, parameter);
		if (parameter.name == "max_health") {
			const auto maxHealth = parameter.value;

			auto* destroyable = this->GetParent()->GetComponent<DestroyableComponent>();

			if (destroyable == nullptr) return;

			destroyable->SetMaxHealth(destroyable->GetMaxHealth() - maxHealth);
		} else if (parameter.name == "max_armor") {
			const auto maxArmor = parameter.value;

			auto* destroyable = this->GetParent()->GetComponent<DestroyableComponent>();

			if (destroyable == nullptr) return;

			destroyable->SetMaxArmor(destroyable->GetMaxArmor() - maxArmor);
		} else if (parameter.name == "max_imagination") {
			const auto maxImagination = parameter.value;

			auto* destroyable = this->GetParent()->GetComponent<DestroyableComponent>();

			if (destroyable == nullptr) return;

			destroyable->SetMaxImagination(destroyable->GetMaxImagination() - maxImagination);
		} else if (parameter.name == "speed") {
			auto* controllablePhysicsComponent = this->GetParent()->GetComponent<ControllablePhysicsComponent>();
			if (!controllablePhysicsComponent) return;
			const auto speed = parameter.value;
			controllablePhysicsComponent->RemoveSpeedboost(speed);
		}
	}
}

void BuffComponent::RemoveAllBuffs() {
	for (const auto& buff : m_Buffs) {
		RemoveBuffEffect(buff.first);
	}

	m_Buffs.clear();
}

void BuffComponent::Reset() {
	RemoveAllBuffs();
}

void BuffComponent::ReApplyBuffs() {
	for (const auto& buff : m_Buffs) {
		ApplyBuffEffect(buff.first);
	}
}

Entity* BuffComponent::GetParent() const {
	return m_Parent;
}

void BuffComponent::LoadFromXml(const tinyxml2::XMLDocument& doc) {
	// Load buffs
	auto* dest = doc.FirstChildElement("obj")->FirstChildElement("dest");

	// Make sure we have a clean buff element.
	auto* buffElement = dest->FirstChildElement("buff");

	// Old character, no buffs to load
	if (buffElement == nullptr) {
		return;
	}

	auto* buffEntry = buffElement->FirstChildElement("b");

	while (buffEntry) {
		int32_t id = buffEntry->IntAttribute("id");
		float t = buffEntry->FloatAttribute("t");
		float tk = buffEntry->FloatAttribute("tk");
		float tt = buffEntry->FloatAttribute("tt");
		int32_t s = buffEntry->FloatAttribute("s");
		LWOOBJID sr = buffEntry->Int64Attribute("sr");
		int32_t b = buffEntry->IntAttribute("b");
		int32_t refCount = buffEntry->IntAttribute("refCount");

		bool cancelOnDamaged = buffEntry->BoolAttribute("cancelOnDamaged");
		bool cancelOnDeath = buffEntry->BoolAttribute("cancelOnDeath");
		bool cancelOnLogout = buffEntry->BoolAttribute("cancelOnLogout");
		bool cancelOnRemoveBuff = buffEntry->BoolAttribute("cancelOnRemoveBuff");
		bool cancelOnUi = buffEntry->BoolAttribute("cancelOnUi");
		bool cancelOnUnequip = buffEntry->BoolAttribute("cancelOnUnequip");
		bool cancelOnZone = buffEntry->BoolAttribute("cancelOnZone");
		bool applyOnTeammates = buffEntry->BoolAttribute("applyOnTeammates");


		Buff buff;
		buff.id = id;
		buff.time = t;
		buff.tick = tk;
		buff.stacks = s;
		buff.source = sr;
		buff.behaviorID = b;
		buff.refCount = refCount;
		buff.tickTime = tt;

		buff.cancelOnDamaged = cancelOnDamaged;
		buff.cancelOnDeath = cancelOnDeath;
		buff.cancelOnLogout = cancelOnLogout;
		buff.cancelOnRemoveBuff = cancelOnRemoveBuff;
		buff.cancelOnUi = cancelOnUi;
		buff.cancelOnUnequip = cancelOnUnequip;
		buff.cancelOnZone = cancelOnZone;
		buff.applyOnTeammates = applyOnTeammates;


		m_Buffs.emplace(id, buff);

		buffEntry = buffEntry->NextSiblingElement("b");
	}
}

void BuffComponent::UpdateXml(tinyxml2::XMLDocument& doc) {
	// Save buffs
	auto* dest = doc.FirstChildElement("obj")->FirstChildElement("dest");

	// Make sure we have a clean buff element.
	auto* buffElement = dest->FirstChildElement("buff");

	if (buffElement == nullptr) {
		buffElement = doc.NewElement("buff");

		dest->LinkEndChild(buffElement);
	} else {
		buffElement->DeleteChildren();
	}

	for (const auto& [id, buff] : m_Buffs) {
		auto* buffEntry = doc.NewElement("b");
		// TODO: change this if to if (buff.cancelOnZone || buff.cancelOnLogout) handling at some point.  No current way to differentiate between zone transfer and logout.
		if (buff.cancelOnZone) continue;

		buffEntry->SetAttribute("id", id);
		buffEntry->SetAttribute("t", buff.time);
		buffEntry->SetAttribute("tk", buff.tick);
		buffEntry->SetAttribute("tt", buff.tickTime);
		buffEntry->SetAttribute("s", buff.stacks);
		buffEntry->SetAttribute("sr", buff.source);
		buffEntry->SetAttribute("b", buff.behaviorID);
		buffEntry->SetAttribute("refCount", buff.refCount);

		buffEntry->SetAttribute("cancelOnDamaged", buff.cancelOnDamaged);
		buffEntry->SetAttribute("cancelOnDeath", buff.cancelOnDeath);
		buffEntry->SetAttribute("cancelOnLogout", buff.cancelOnLogout);
		buffEntry->SetAttribute("cancelOnRemoveBuff", buff.cancelOnRemoveBuff);
		buffEntry->SetAttribute("cancelOnUi", buff.cancelOnUi);
		buffEntry->SetAttribute("cancelOnUnequip", buff.cancelOnUnequip);
		buffEntry->SetAttribute("cancelOnZone", buff.cancelOnZone);
		buffEntry->SetAttribute("applyOnTeammates", buff.applyOnTeammates);

		buffElement->LinkEndChild(buffEntry);
	}
}

const std::vector<BuffParameter>& BuffComponent::GetBuffParameters(int32_t buffId) {
	const auto& pair = m_Cache.find(buffId);

	if (pair != m_Cache.end()) {
		return pair->second;
	}

	auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM BuffParameters WHERE BuffID = ?;");
	query.bind(1, static_cast<int>(buffId));

	auto result = query.execQuery();

	std::vector<BuffParameter> parameters{};

	while (!result.eof()) {
		BuffParameter param;

		param.buffId = buffId;
		param.name = result.getStringField("ParameterName");
		param.value = result.getFloatField("NumberValue");
		param.effectId = result.getIntField("EffectID");

		if (!result.fieldIsNull("StringValue")) {
			std::istringstream stream(result.getStringField("StringValue"));
			std::string token;

			while (std::getline(stream, token, ',')) {
				try {
					const auto value = std::stof(token);

					param.values.push_back(value);
				} catch (std::invalid_argument& exception) {
					LOG("Failed to parse value (%s): (%s)!", token.c_str(), exception.what());
				}
			}
		}

		parameters.push_back(param);

		result.nextRow();
	}

	m_Cache.insert_or_assign(buffId, parameters);

	return m_Cache.find(buffId)->second;
}
