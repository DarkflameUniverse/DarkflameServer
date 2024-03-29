set(DGAME_DBEHAVIORS_SOURCES "AirMovementBehavior.cpp"
	"AndBehavior.cpp"
	"ApplyBuffBehavior.cpp"
	"AreaOfEffectBehavior.cpp"
	"AttackDelayBehavior.cpp"
	"BasicAttackBehavior.cpp"
	"Behavior.cpp"
	"BehaviorBranchContext.cpp"
	"BehaviorContext.cpp"
	"BlockBehavior.cpp"
	"BuffBehavior.cpp"
	"CarBoostBehavior.cpp"
	"ChainBehavior.cpp"
	"ChangeIdleFlagsBehavior.cpp"
	"ChangeOrientationBehavior.cpp"
	"ChargeUpBehavior.cpp"
	"ClearTargetBehavior.cpp"
	"ConsumeItemBehavior.cpp"
	"DamageAbsorptionBehavior.cpp"
	"DamageReductionBehavior.cpp"
	"DarkInspirationBehavior.cpp"
	"DurationBehavior.cpp"
	"EndBehavior.cpp"
	"FallSpeedBehavior.cpp"
	"ForceMovementBehavior.cpp"
	"HealBehavior.cpp"
	"ImaginationBehavior.cpp"
	"ImmunityBehavior.cpp"
	"InterruptBehavior.cpp"
	"JetPackBehavior.cpp"
	"KnockbackBehavior.cpp"
	"LootBuffBehavior.cpp"
	"MovementSwitchBehavior.cpp"
	"NpcCombatSkillBehavior.cpp"
	"OverTimeBehavior.cpp"
	"PlayEffectBehavior.cpp"
	"ProjectileAttackBehavior.cpp"
	"PropertyTeleportBehavior.cpp"
	"PullToPointBehavior.cpp"
	"RemoveBuffBehavior.cpp"
	"RepairBehavior.cpp"
	"SkillCastFailedBehavior.cpp"
	"SkillEventBehavior.cpp"
	"SpawnBehavior.cpp"
	"SpawnQuickbuildBehavior.cpp"
	"SpeedBehavior.cpp"
	"StartBehavior.cpp"
	"StunBehavior.cpp"
	"SwitchBehavior.cpp"
	"SwitchMultipleBehavior.cpp"
	"TacArcBehavior.cpp"
	"TargetCasterBehavior.cpp"
	"TauntBehavior.cpp"
	"VentureVisionBehavior.cpp"
	"VerifyBehavior.cpp")

add_library(dBehaviors OBJECT ${DGAME_DBEHAVIORS_SOURCES})
target_link_libraries(dBehaviors PUBLIC dDatabaseCDClient dPhysics)
target_include_directories(dBehaviors PUBLIC "."
	"${PROJECT_SOURCE_DIR}/dGame/dGameMessages" # via BehaviorContext.h
	PRIVATE
	"${PROJECT_SOURCE_DIR}/dGame/dComponents" # direct BuffComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dUtilities" # Preconditions.h via QuickBuildComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dEntity" # via dZoneManager.h, Spawner.h
	"${PROJECT_SOURCE_DIR}/dGame/dInventory" # via CharacterComponent.h
	"${PROJECT_SOURCE_DIR}/dZoneManager" # via BasicAttackBehavior.cpp
)
target_precompile_headers(dBehaviors REUSE_FROM dGameBase)
