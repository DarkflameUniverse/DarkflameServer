Legend
```
├── Explicit inheritance
├~~ Loaded with Parent
├-> SubComponent
├-? idk lol, but related

originalName -> newName

```


```tree

LWOActivityComponent
├── LWOQuickBuildComponent
├── LWOMiniGameControlComponent
├── LWOShootingGalleryComponent
├── LWOScriptedActivityComponent
|	└── LWOBaseRacingControlComponent -> RacingControlComponent
|		├── LWORacingControlComponent -> VehicleRacingControlComponent
|		└── LWOGateRushControlComponent
LWOBaseCombatAIComponent
├~~ LWOPathfindingControlComponent
├~~ LWOProximityMonitorComponent
LWOProjectilePhysComponent
LWOBasePhysComponent
├── LWORigidBodyPhantomComponent
├── LWOPhantomPhysComponent
├── LWOVehiclePhysicsComponent
├── LWOSimplePhysComponent
|	├~~ LWOPhantomPhysicsComponent
├── LWOPhysicsSystemComponent
├── LWOHavokVehiclePhysicsComponent
├── LWOControllablePhysComponent
LWOBaseRenderComponent
├── LWOSkinnedRenderComponent
├~~ LWOSkinnedRenderComponent
├~~ LWOFXComponent
LWOBaseVendorComponent
├── LWOVendorComponent
|	├~~ LWOProximityMonitorComponent
├── LWODonationVendorComponent
|	├~~ LWOProximityMonitorComponent
├── LWOAchievementVendorComponent
|	├~~ LWOProximityMonitorComponent
LWOBBBComponent_Common
├── LWOBBBComponent_Client
LWOBlueprintComponent
LWOBouncerComponent
LWOBuildControllerComponentCommon
├── LWOBuildControllerComponent
LWOChangelingBuildComponent
LWOCharacterComponent
├── LWOMinifigComponent
├~~ LWOPossessionControlComponent
├~~ LWOMountControlComponent
├~~ LWOPetCreatorComponent
├~~ LWOLevelProgressionComponent
├~~ LWOPlayerForcedMovementComponent
|	├~? LWOPathfindingControlComponent
LWOChestComponent
LWOChoiceBuildComponent
LWOCollectibleComponent
LWOCustomBuildAssemblyComponent
LWODestroyableComponent
├~~ LWOBuffComponent
├~~ LWOStatusEffectComponent
LWODropEffectComponent
LWODroppedLootComponent
LWOExhibitComponent
LWOGenericActivatorComponent
LWOGhostComponent
LWOHFLightDirectionGadgetComponent
LWOInventoryComponent_Common
├── LWOInventoryComponent_Client
|	└── LWOInventoryComponent_EquippedItem
LWOItemComponent
LWOLUPExhibitComponent
LWOMissionOfferComponent
LWOModelBehaviorComponent
├~~ LWOSimplePhysComponent
├~~ LWOControllablePhysComponent
├~~ LWOPathfindingControlComponent
├~~ LWOMutableModelBehaviorComponent
LWOModelBuilderComponent
LWOModularBuildComponentCommon
├── LWOModularBuildComponent
LWOModuleAssemblyComponent
├── LWOModuleAssemblyComponentCommon
LWOModuleComponentCommon
├── LWOModuleComponent
LWOMovementAIComponent
LWOMultiZoneEntranceComponent
LWOOverheadIconComponent
LWOPetComponent
├~~ LWOPathfindingControlComponent
├~? LWOItemComponent
├~? LWOModelBehaviorComponent
|	├~~ ...
LWOPlatformBoundaryComponent
LWOPlatformComponent
├-> LWOMoverPlatformSubComponent
├-> LWOSimpleMoverPlatformSubComponent
├-> LWORotaterPlatformSubComponent
LWOPropertyComponent
LWOPropertyEntranceComponent
LWOPropertyManagementComponent
LWOPropertyVendorComponent
LWOProximityMonitorComponent
LWOSoundTriggerComponent
├── LWORacingSoundTriggerComponent
LWORacingStatsComponentCommon
├── LWORacingStatsComponent
LWORocketAnimationControlComponentCommon
├── LWORocketAnimationControlComponent
LWORocketLaunchpadControlComponentCommon
├── LWORocketLaunchpadControlComponent
LWOScriptComponent
├~~ LWOPathfindingControlComponent
├~~ LWOProximityMonitorComponent
LWOShowcaseModelHandlerComponent
LWOSkillComponent
LWOSoundAmbient2DComponent
LWOSoundAmbient3DComponent
LWOSoundRepeaterComponent
LWOSpawnComponent
LWOSpringpadComponent
LWOSwitchComponent
LWOTriggerComponent
LocalPlayer - This is a function call in the client which, if the generated Entity is a player, the below components are added. This **must** be done for all players.
├~~ LWOInteractionManagerComponent
├~~ LWOUserControlComponent
├~~ LWOFriendsListComponent
├~~ LWOIgnoreListComponent
├~~ LWOTextEffectComponent
├~~ LWOChatBubbleComponent
├~~ LWOGuildComponent
├~~ LWOPlayerPetTamingComponent
├~~ LWOLocalSystemsComponent
├~~ LWOSlashCommandComponent
├~~ LWOMissionComponent
├~~ LWOPropertyEditorComponent
├~~ LWOComponent115
├~~ LWOTeamsComponent
├~~ LWOChatComponent
├~~ LWOPetControlComponent
├~~ LWOTradeComponent
├~~ LWOPreconditionComponent
├~~ LWOFlagComponent
├~~ LWOFactionTriggerComponent

```
