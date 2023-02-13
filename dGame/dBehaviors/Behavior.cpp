#include <sstream>
#include <algorithm>

#include "Behavior.h"
#include "CDActivitiesTable.h"
#include "Game.h"
#include "dLogger.h"
#include "BehaviorTemplates.h"
#include "BehaviorBranchContext.h"
#include <unordered_map>

/*
 * Behavior includes
 */
#include "AirMovementBehavior.h"
#include "EmptyBehavior.h"
#include "MovementSwitchBehavior.h"
#include "AndBehavior.h"
#include "AreaOfEffectBehavior.h"
#include "DurationBehavior.h"
#include "TacArcBehavior.h"
#include "LootBuffBehavior.h"
#include "AttackDelayBehavior.h"
#include "BasicAttackBehavior.h"
#include "ChainBehavior.h"
#include "ChargeUpBehavior.h"
#include "GameMessages.h"
#include "HealBehavior.h"
#include "ImaginationBehavior.h"
#include "KnockbackBehavior.h"
#include "NpcCombatSkillBehavior.h"
#include "StartBehavior.h"
#include "StunBehavior.h"
#include "ProjectileAttackBehavior.h"
#include "RepairBehavior.h"
#include "SwitchBehavior.h"
#include "SwitchMultipleBehavior.h"
#include "TargetCasterBehavior.h"
#include "VerifyBehavior.h"
#include "BuffBehavior.h"
#include "TauntBehavior.h"
#include "SkillCastFailedBehavior.h"
#include "SpawnBehavior.h"
#include "ForceMovementBehavior.h"
#include "RemoveBuffBehavior.h"
#include "ImmunityBehavior.h"
#include "InterruptBehavior.h"
#include "PlayEffectBehavior.h"
#include "DamageAbsorptionBehavior.h"
#include "VentureVisionBehavior.h"
#include "PropertyTeleportBehavior.h"
#include "BlockBehavior.h"
#include "ClearTargetBehavior.h"
#include "PullToPointBehavior.h"
#include "EndBehavior.h"
#include "ChangeOrientationBehavior.h"
#include "OverTimeBehavior.h"
#include "ApplyBuffBehavior.h"
#include "CarBoostBehavior.h"
#include "SkillEventBehavior.h"
#include "SpeedBehavior.h"
#include "DamageReductionBehavior.h"
#include "JetPackBehavior.h"
#include "ChangeIdleFlagsBehavior.h"
#include "DarkInspirationBehavior.h"

 //CDClient includes
#include "CDBehaviorParameterTable.h"
#include "CDClientDatabase.h"
#include "CDClientManager.h"

//Other includes
#include "EntityManager.h"
#include "RenderComponent.h"
#include "DestroyableComponent.h"

std::unordered_map<uint32_t, Behavior*> Behavior::Cache = {};
CDBehaviorParameterTable* Behavior::BehaviorParameterTable = nullptr;

Behavior* Behavior::GetBehavior(const uint32_t behaviorId) {
	if (BehaviorParameterTable == nullptr) {
		BehaviorParameterTable = CDClientManager::Instance()->GetTable<CDBehaviorParameterTable>("BehaviorParameter");
	}

	const auto pair = Cache.find(behaviorId);

	if (pair == Cache.end()) {
		return nullptr;
	}

	return static_cast<Behavior*>(pair->second);
}

Behavior* Behavior::CreateBehavior(const uint32_t behaviorId) {
	auto* cached = GetBehavior(behaviorId);

	if (cached != nullptr) {
		return cached;
	}

	if (behaviorId == 0) {
		return new EmptyBehavior(0);
	}

	const auto templateId = GetBehaviorTemplate(behaviorId);

	Behavior* behavior = nullptr;

	switch (templateId) {
	case BehaviorTemplates::BEHAVIOR_EMPTY: break;
	case BehaviorTemplates::BEHAVIOR_BASIC_ATTACK:
		behavior = new BasicAttackBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_TAC_ARC:
		behavior = new TacArcBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_AND:
		behavior = new AndBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_PROJECTILE_ATTACK:
		behavior = new ProjectileAttackBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_HEAL:
		behavior = new HealBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_MOVEMENT_SWITCH:
		behavior = new MovementSwitchBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_AREA_OF_EFFECT:
		behavior = new AreaOfEffectBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_PLAY_EFFECT:
		behavior = new PlayEffectBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_IMMUNITY:
		behavior = new ImmunityBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_DAMAGE_BUFF: break;
	case BehaviorTemplates::BEHAVIOR_DAMAGE_ABSORBTION:
		behavior = new DamageAbsorptionBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_OVER_TIME:
		behavior = new OverTimeBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_IMAGINATION:
		behavior = new ImaginationBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_TARGET_CASTER:
		behavior = new TargetCasterBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_STUN:
		behavior = new StunBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_DURATION:
		behavior = new DurationBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_KNOCKBACK:
		behavior = new KnockbackBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_ATTACK_DELAY:
		behavior = new AttackDelayBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_CAR_BOOST:
		behavior = new CarBoostBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_FALL_SPEED: break;
	case BehaviorTemplates::BEHAVIOR_SHIELD: break;
	case BehaviorTemplates::BEHAVIOR_REPAIR_ARMOR:
		behavior = new RepairBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_SPEED:
		behavior = new SpeedBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_DARK_INSPIRATION: 
		behavior = new DarkInspirationBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_LOOT_BUFF:
		behavior = new LootBuffBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_VENTURE_VISION:
		behavior = new VentureVisionBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_SPAWN_OBJECT:
		behavior = new SpawnBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_LAY_BRICK: break;
	case BehaviorTemplates::BEHAVIOR_SWITCH:
		behavior = new SwitchBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_BUFF:
		behavior = new BuffBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_JETPACK:
		behavior = new JetPackBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_SKILL_EVENT:
		behavior = new SkillEventBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_CONSUME_ITEM: break;
	case BehaviorTemplates::BEHAVIOR_SKILL_CAST_FAILED:
		behavior = new SkillCastFailedBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_IMITATION_SKUNK_STINK: break;
	case BehaviorTemplates::BEHAVIOR_CHANGE_IDLE_FLAGS:
		behavior = new ChangeIdleFlagsBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_APPLY_BUFF:
		behavior = new ApplyBuffBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_CHAIN:
		behavior = new ChainBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_CHANGE_ORIENTATION:
		behavior = new ChangeOrientationBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_FORCE_MOVEMENT:
		behavior = new ForceMovementBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_INTERRUPT:
		behavior = new InterruptBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_ALTER_COOLDOWN: break;
	case BehaviorTemplates::BEHAVIOR_CHARGE_UP:
		behavior = new ChargeUpBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_SWITCH_MULTIPLE:
		behavior = new SwitchMultipleBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_START:
		behavior = new StartBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_END:
		behavior = new EndBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_ALTER_CHAIN_DELAY: break;
	case BehaviorTemplates::BEHAVIOR_CAMERA: break;
	case BehaviorTemplates::BEHAVIOR_REMOVE_BUFF:
		behavior = new RemoveBuffBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_GRAB: break;
	case BehaviorTemplates::BEHAVIOR_MODULAR_BUILD: break;
	case BehaviorTemplates::BEHAVIOR_NPC_COMBAT_SKILL:
		behavior = new NpcCombatSkillBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_BLOCK:
		behavior = new BlockBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_VERIFY:
		behavior = new VerifyBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_TAUNT:
		behavior = new TauntBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_AIR_MOVEMENT:
		behavior = new AirMovementBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_SPAWN_QUICKBUILD:
		behavior = new SpawnBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_PULL_TO_POINT:
		behavior = new PullToPointBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_PROPERTY_ROTATE: break;
	case BehaviorTemplates::BEHAVIOR_DAMAGE_REDUCTION:
		behavior = new DamageReductionBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_PROPERTY_TELEPORT:
		behavior = new PropertyTeleportBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_PROPERTY_CLEAR_TARGET:
		behavior = new ClearTargetBehavior(behaviorId);
		break;
	case BehaviorTemplates::BEHAVIOR_TAKE_PICTURE: break;
	case BehaviorTemplates::BEHAVIOR_MOUNT: break;
	case BehaviorTemplates::BEHAVIOR_SKILL_SET: break;
	default:
		//Game::logger->Log("Behavior", "Failed to load behavior with invalid template id (%i)!", templateId);
		break;
	}

	if (behavior == nullptr) {
		//Game::logger->Log("Behavior", "Failed to load unimplemented template id (%i)!", templateId);

		behavior = new EmptyBehavior(behaviorId);
	}

	behavior->Load();

	return behavior;
}

BehaviorTemplates Behavior::GetBehaviorTemplate(const uint32_t behaviorId) {
	auto behaviorTemplateTable = CDClientManager::Instance()->GetTable<CDBehaviorTemplateTable>("BehaviorTemplate");

	BehaviorTemplates templateID = BehaviorTemplates::BEHAVIOR_EMPTY;
	// Find behavior template by its behavior id.  Default to 0.
	if (behaviorTemplateTable) {
		auto templateEntry = behaviorTemplateTable->GetByBehaviorID(behaviorId);
		if (templateEntry.behaviorID == behaviorId) {
			templateID = static_cast<BehaviorTemplates>(templateEntry.templateID);
		}
	}

	if (templateID == BehaviorTemplates::BEHAVIOR_EMPTY && behaviorId != 0) {
		Game::logger->Log("Behavior", "Failed to load behavior template with id (%i)!", behaviorId);
	}

	return templateID;
}

// For use with enemies, to display the correct damage animations on the players
void Behavior::PlayFx(std::u16string type, const LWOOBJID target, const LWOOBJID secondary) {
	auto* targetEntity = EntityManager::Instance()->GetEntity(target);

	if (targetEntity == nullptr) {
		return;
	}

	const auto effectId = this->m_effectId;

	if (effectId == 0) {
		GameMessages::SendPlayFXEffect(targetEntity, -1, type, "", secondary, 1, 1, true);

		return;
	}

	auto* renderComponent = targetEntity->GetComponent<RenderComponent>();

	const auto typeString = GeneralUtils::UTF16ToWTF8(type);

	if (m_effectNames == nullptr) {
		m_effectNames = new std::unordered_map<std::string, std::string>();
	} else {
		const auto pair = m_effectNames->find(typeString);

		if (type.empty()) {
			type = GeneralUtils::ASCIIToUTF16(*m_effectType);
		}

		if (pair != m_effectNames->end()) {
			if (renderComponent == nullptr) {
				GameMessages::SendPlayFXEffect(targetEntity, effectId, type, pair->second, secondary, 1, 1, true);

				return;
			}

			renderComponent->PlayEffect(effectId, type, pair->second, secondary);

			return;
		}
	}

	// The SQlite result object becomes invalid if the query object leaves scope.
	// So both queries are defined before the if statement
	CppSQLite3Query result;
	auto typeQuery = CDClientDatabase::CreatePreppedStmt(
		"SELECT effectName FROM BehaviorEffect WHERE effectType = ? AND effectID = ?;");

	auto idQuery = CDClientDatabase::CreatePreppedStmt(
		"SELECT effectName, effectType FROM BehaviorEffect WHERE effectID = ?;");

	if (!type.empty()) {
		typeQuery.bind(1, typeString.c_str());
		typeQuery.bind(2, (int)effectId);

		result = typeQuery.execQuery();
	} else {
		idQuery.bind(1, (int)effectId);

		result = idQuery.execQuery();
	}

	if (result.eof() || result.fieldIsNull(0)) {
		return;
	}

	const auto name = std::string(result.getStringField(0));

	if (type.empty()) {
		const auto typeResult = result.getStringField(1);

		type = GeneralUtils::ASCIIToUTF16(typeResult);

		m_effectType = new std::string(typeResult);
	}

	result.finalize();

	m_effectNames->insert_or_assign(typeString, name);

	if (renderComponent == nullptr) {
		GameMessages::SendPlayFXEffect(targetEntity, effectId, type, name, secondary, 1, 1, true);

		return;
	}

	renderComponent->PlayEffect(effectId, type, name, secondary);
}

Behavior::Behavior(const uint32_t behaviorId) {
	auto behaviorTemplateTable = CDClientManager::Instance()->GetTable<CDBehaviorTemplateTable>("BehaviorTemplate");

	CDBehaviorTemplate templateInDatabase{};

	if (behaviorTemplateTable) {
		auto templateEntry = behaviorTemplateTable->GetByBehaviorID(behaviorId);
		if (templateEntry.behaviorID == behaviorId) {
			templateInDatabase = templateEntry;
		}
	}

	this->m_behaviorId = behaviorId;

	// Add to cache
	Cache.insert_or_assign(behaviorId, this);

	if (behaviorId == 0) {
		this->m_effectId = 0;
		this->m_effectHandle = nullptr;
		this->m_templateId = BehaviorTemplates::BEHAVIOR_EMPTY;
	}

	// Make sure we do not proceed if we are trying to load an invalid behavior
	if (templateInDatabase.behaviorID == 0) {
		Game::logger->Log("Behavior", "Failed to load behavior with id (%i)!", behaviorId);

		this->m_effectId = 0;
		this->m_effectHandle = nullptr;
		this->m_templateId = BehaviorTemplates::BEHAVIOR_EMPTY;

		return;
	}

	this->m_templateId = static_cast<BehaviorTemplates>(templateInDatabase.templateID);

	this->m_effectId = templateInDatabase.effectID;

	this->m_effectHandle = *templateInDatabase.effectHandle != "" ? new std::string(*templateInDatabase.effectHandle) : nullptr;
}


float Behavior::GetFloat(const std::string& name, const float defaultValue) const {
	// Get the behavior parameter entry and return its value.
	if (!BehaviorParameterTable) BehaviorParameterTable = CDClientManager::Instance()->GetTable<CDBehaviorParameterTable>("BehaviorParameter");
	return BehaviorParameterTable->GetValue(this->m_behaviorId, name, defaultValue);
}


bool Behavior::GetBoolean(const std::string& name, const bool defaultValue) const {
	return GetFloat(name, defaultValue) > 0;
}


int32_t Behavior::GetInt(const std::string& name, const int defaultValue) const {
	return static_cast<int32_t>(GetFloat(name, defaultValue));
}


Behavior* Behavior::GetAction(const std::string& name) const {
	const auto id = GetInt(name);

	return CreateBehavior(id);
}

Behavior* Behavior::GetAction(float value) const {
	return CreateBehavior(static_cast<int32_t>(value));
}

std::map<std::string, float> Behavior::GetParameterNames() const {
	std::map<std::string, float> templatesInDatabase;
	// Find behavior template by its behavior id.
	if (!BehaviorParameterTable) BehaviorParameterTable = CDClientManager::Instance()->GetTable<CDBehaviorParameterTable>("BehaviorParameter");
	if (BehaviorParameterTable) {
		templatesInDatabase = BehaviorParameterTable->GetParametersByBehaviorID(this->m_behaviorId);
	}

	return templatesInDatabase;
}

void Behavior::Load() {
}

void Behavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
}

void Behavior::Sync(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
}

void Behavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
}

void Behavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
}

void Behavior::End(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
}

void Behavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
}

void Behavior::SyncCalculation(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
}

Behavior::~Behavior() {
	delete m_effectNames;
	delete m_effectType;
	delete m_effectHandle;
}
