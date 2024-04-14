#include <sstream>
#include <algorithm>

#include "Behavior.h"
#include "CDActivitiesTable.h"
#include "Game.h"
#include "Logger.h"
#include "BehaviorTemplate.h"
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
#include "FallSpeedBehavior.h"
#include "ChangeIdleFlagsBehavior.h"
#include "DarkInspirationBehavior.h"
#include "ConsumeItemBehavior.h"

 //CDClient includes
#include "CDBehaviorParameterTable.h"
#include "CDClientDatabase.h"
#include "CDClientManager.h"

//Other includes
#include "EntityManager.h"
#include "RenderComponent.h"
#include "DestroyableComponent.h"
#include "CDBehaviorTemplateTable.h"

std::unordered_map<uint32_t, Behavior*> Behavior::Cache = {};
CDBehaviorParameterTable* Behavior::BehaviorParameterTable = nullptr;

Behavior* Behavior::GetBehavior(const uint32_t behaviorId) {
	if (BehaviorParameterTable == nullptr) {
		BehaviorParameterTable = CDClientManager::GetTable<CDBehaviorParameterTable>();
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
	case BehaviorTemplate::EMPTY: break;
	case BehaviorTemplate::BASIC_ATTACK:
		behavior = new BasicAttackBehavior(behaviorId);
		break;
	case BehaviorTemplate::TAC_ARC:
		behavior = new TacArcBehavior(behaviorId);
		break;
	case BehaviorTemplate::AND:
		behavior = new AndBehavior(behaviorId);
		break;
	case BehaviorTemplate::PROJECTILE_ATTACK:
		behavior = new ProjectileAttackBehavior(behaviorId);
		break;
	case BehaviorTemplate::HEAL:
		behavior = new HealBehavior(behaviorId);
		break;
	case BehaviorTemplate::MOVEMENT_SWITCH:
		behavior = new MovementSwitchBehavior(behaviorId);
		break;
	case BehaviorTemplate::AREA_OF_EFFECT:
		behavior = new AreaOfEffectBehavior(behaviorId);
		break;
	case BehaviorTemplate::PLAY_EFFECT:
		behavior = new PlayEffectBehavior(behaviorId);
		break;
	case BehaviorTemplate::IMMUNITY:
		behavior = new ImmunityBehavior(behaviorId);
		break;
	case BehaviorTemplate::DAMAGE_BUFF: break;
	case BehaviorTemplate::DAMAGE_ABSORBTION:
		behavior = new DamageAbsorptionBehavior(behaviorId);
		break;
	case BehaviorTemplate::OVER_TIME:
		behavior = new OverTimeBehavior(behaviorId);
		break;
	case BehaviorTemplate::IMAGINATION:
		behavior = new ImaginationBehavior(behaviorId);
		break;
	case BehaviorTemplate::TARGET_CASTER:
		behavior = new TargetCasterBehavior(behaviorId);
		break;
	case BehaviorTemplate::STUN:
		behavior = new StunBehavior(behaviorId);
		break;
	case BehaviorTemplate::DURATION:
		behavior = new DurationBehavior(behaviorId);
		break;
	case BehaviorTemplate::KNOCKBACK:
		behavior = new KnockbackBehavior(behaviorId);
		break;
	case BehaviorTemplate::ATTACK_DELAY:
		behavior = new AttackDelayBehavior(behaviorId);
		break;
	case BehaviorTemplate::CAR_BOOST:
		behavior = new CarBoostBehavior(behaviorId);
		break;
	case BehaviorTemplate::FALL_SPEED:
		behavior = new FallSpeedBehavior(behaviorId);
		break;
	case BehaviorTemplate::SHIELD: break;
	case BehaviorTemplate::REPAIR_ARMOR:
		behavior = new RepairBehavior(behaviorId);
		break;
	case BehaviorTemplate::SPEED:
		behavior = new SpeedBehavior(behaviorId);
		break;
	case BehaviorTemplate::DARK_INSPIRATION:
		behavior = new DarkInspirationBehavior(behaviorId);
		break;
	case BehaviorTemplate::LOOT_BUFF:
		behavior = new LootBuffBehavior(behaviorId);
		break;
	case BehaviorTemplate::VENTURE_VISION:
		behavior = new VentureVisionBehavior(behaviorId);
		break;
	case BehaviorTemplate::SPAWN_OBJECT:
		behavior = new SpawnBehavior(behaviorId);
		break;
	case BehaviorTemplate::LAY_BRICK: break;
	case BehaviorTemplate::SWITCH:
		behavior = new SwitchBehavior(behaviorId);
		break;
	case BehaviorTemplate::BUFF:
		behavior = new BuffBehavior(behaviorId);
		break;
	case BehaviorTemplate::JETPACK:
		behavior = new JetPackBehavior(behaviorId);
		break;
	case BehaviorTemplate::SKILL_EVENT:
		behavior = new SkillEventBehavior(behaviorId);
		break;
	case BehaviorTemplate::CONSUME_ITEM:
		behavior = new ConsumeItemBehavior(behaviorId);
		break;
	case BehaviorTemplate::SKILL_CAST_FAILED:
		behavior = new SkillCastFailedBehavior(behaviorId);
		break;
	case BehaviorTemplate::IMITATION_SKUNK_STINK: break;
	case BehaviorTemplate::CHANGE_IDLE_FLAGS:
		behavior = new ChangeIdleFlagsBehavior(behaviorId);
		break;
	case BehaviorTemplate::APPLY_BUFF:
		behavior = new ApplyBuffBehavior(behaviorId);
		break;
	case BehaviorTemplate::CHAIN:
		behavior = new ChainBehavior(behaviorId);
		break;
	case BehaviorTemplate::CHANGE_ORIENTATION:
		behavior = new ChangeOrientationBehavior(behaviorId);
		break;
	case BehaviorTemplate::FORCE_MOVEMENT:
		behavior = new ForceMovementBehavior(behaviorId);
		break;
	case BehaviorTemplate::INTERRUPT:
		behavior = new InterruptBehavior(behaviorId);
		break;
	case BehaviorTemplate::ALTER_COOLDOWN: break;
	case BehaviorTemplate::CHARGE_UP:
		behavior = new ChargeUpBehavior(behaviorId);
		break;
	case BehaviorTemplate::SWITCH_MULTIPLE:
		behavior = new SwitchMultipleBehavior(behaviorId);
		break;
	case BehaviorTemplate::START:
		behavior = new StartBehavior(behaviorId);
		break;
	case BehaviorTemplate::END:
		behavior = new EndBehavior(behaviorId);
		break;
	case BehaviorTemplate::ALTER_CHAIN_DELAY: break;
	case BehaviorTemplate::CAMERA: break;
	case BehaviorTemplate::REMOVE_BUFF:
		behavior = new RemoveBuffBehavior(behaviorId);
		break;
	case BehaviorTemplate::GRAB: break;
	case BehaviorTemplate::MODULAR_BUILD: break;
	case BehaviorTemplate::NPC_COMBAT_SKILL:
		behavior = new NpcCombatSkillBehavior(behaviorId);
		break;
	case BehaviorTemplate::BLOCK:
		behavior = new BlockBehavior(behaviorId);
		break;
	case BehaviorTemplate::VERIFY:
		behavior = new VerifyBehavior(behaviorId);
		break;
	case BehaviorTemplate::TAUNT:
		behavior = new TauntBehavior(behaviorId);
		break;
	case BehaviorTemplate::AIR_MOVEMENT:
		behavior = new AirMovementBehavior(behaviorId);
		break;
	case BehaviorTemplate::SPAWN_QUICKBUILD:
		behavior = new SpawnBehavior(behaviorId);
		break;
	case BehaviorTemplate::PULL_TO_POINT:
		behavior = new PullToPointBehavior(behaviorId);
		break;
	case BehaviorTemplate::PROPERTY_ROTATE: break;
	case BehaviorTemplate::DAMAGE_REDUCTION:
		behavior = new DamageReductionBehavior(behaviorId);
		break;
	case BehaviorTemplate::PROPERTY_TELEPORT:
		behavior = new PropertyTeleportBehavior(behaviorId);
		break;
	case BehaviorTemplate::PROPERTY_CLEAR_TARGET:
		behavior = new ClearTargetBehavior(behaviorId);
		break;
	case BehaviorTemplate::TAKE_PICTURE: break;
	case BehaviorTemplate::MOUNT: break;
	case BehaviorTemplate::SKILL_SET: break;
	default:
		//LOG("Failed to load behavior with invalid template id (%i)!", templateId);
		break;
	}

	if (behavior == nullptr) {
		//LOG("Failed to load unimplemented template id (%i)!", templateId);

		behavior = new EmptyBehavior(behaviorId);
	}

	behavior->Load();

	return behavior;
}

BehaviorTemplate Behavior::GetBehaviorTemplate(const uint32_t behaviorId) {
	auto behaviorTemplateTable = CDClientManager::GetTable<CDBehaviorTemplateTable>();

	BehaviorTemplate templateID = BehaviorTemplate::EMPTY;
	// Find behavior template by its behavior id.  Default to 0.
	if (behaviorTemplateTable) {
		auto templateEntry = behaviorTemplateTable->GetByBehaviorID(behaviorId);
		if (templateEntry.behaviorID == behaviorId) {
			templateID = static_cast<BehaviorTemplate>(templateEntry.templateID);
		}
	}

	if (templateID == BehaviorTemplate::EMPTY && behaviorId != 0) {
		LOG("Failed to load behavior template with id (%i)!", behaviorId);
	}

	return templateID;
}

// For use with enemies, to display the correct damage animations on the players
void Behavior::PlayFx(std::u16string type, const LWOOBJID target, const LWOOBJID secondary) {
	auto* targetEntity = Game::entityManager->GetEntity(target);

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

	const auto itr = m_effectNames.find(typeString);

	if (type.empty()) {
		type = GeneralUtils::ASCIIToUTF16(m_effectType);
	}

	if (itr != m_effectNames.end()) {
		if (renderComponent == nullptr) {
			GameMessages::SendPlayFXEffect(targetEntity, effectId, type, itr->second, secondary, 1, 1, true);

			return;
		}

		renderComponent->PlayEffect(effectId, type, itr->second, secondary);

		return;
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
		typeQuery.bind(2, static_cast<int>(effectId));

		result = typeQuery.execQuery();
	} else {
		idQuery.bind(1, static_cast<int>(effectId));

		result = idQuery.execQuery();
	}

	if (result.eof() || result.fieldIsNull(0)) {
		return;
	}

	const auto name = std::string(result.getStringField("effectName"));

	if (type.empty()) {
		const auto typeResult = result.getStringField("effectType");

		type = GeneralUtils::ASCIIToUTF16(typeResult);

		m_effectType = typeResult;
	}

	result.finalize();

	m_effectNames.insert_or_assign(typeString, name);

	if (renderComponent == nullptr) {
		GameMessages::SendPlayFXEffect(targetEntity, effectId, type, name, secondary, 1, 1, true);

		return;
	}

	renderComponent->PlayEffect(effectId, type, name, secondary);
}

Behavior::Behavior(const uint32_t behaviorId) {
	auto behaviorTemplateTable = CDClientManager::GetTable<CDBehaviorTemplateTable>();

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
		this->m_templateId = BehaviorTemplate::EMPTY;
	}

	// Make sure we do not proceed if we are trying to load an invalid behavior
	if (templateInDatabase.behaviorID == 0) {
		LOG("Failed to load behavior with id (%i)!", behaviorId);

		this->m_effectId = 0;
		this->m_templateId = BehaviorTemplate::EMPTY;

		return;
	}

	this->m_templateId = static_cast<BehaviorTemplate>(templateInDatabase.templateID);

	this->m_effectId = templateInDatabase.effectID;

	this->m_effectHandle = *templateInDatabase.effectHandle;
}


float Behavior::GetFloat(const std::string& name, const float defaultValue) const {
	// Get the behavior parameter entry and return its value.
	if (!BehaviorParameterTable) BehaviorParameterTable = CDClientManager::GetTable<CDBehaviorParameterTable>();
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
	if (!BehaviorParameterTable) BehaviorParameterTable = CDClientManager::GetTable<CDBehaviorParameterTable>();
	if (BehaviorParameterTable) {
		templatesInDatabase = BehaviorParameterTable->GetParametersByBehaviorID(this->m_behaviorId);
	}

	return templatesInDatabase;
}

void Behavior::Load() {
}

void Behavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
}

void Behavior::Sync(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
}

void Behavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
}

void Behavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
}

void Behavior::End(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
}

void Behavior::Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
}

void Behavior::SyncCalculation(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
}
