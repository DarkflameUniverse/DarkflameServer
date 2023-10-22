import ComponentDefinitions as cd
import ComponentFactories as cf

# If we have any of these components, we need pathfinding control
def CheckIfNeedsPathfindingControl(components_to_load: dict):
	add_pathfinding_control =  cd.components['COMPONENT_TYPE_CONTROLLABLE_PHYSICS'] in components_to_load
	add_pathfinding_control |= cd.components['COMPONENT_TYPE_PROJECTILE_PHYSICS'] in components_to_load
	add_pathfinding_control |= cd.components['COMPONENT_TYPE_HAVOK_VEHICLE_PHYSICS'] in components_to_load
	add_pathfinding_control |= cd.components['COMPONENT_TYPE_VEHICLE_PHYSICS'] in components_to_load
	return add_pathfinding_control

def DoObjectPreLoad(components_to_load: dict):
	isHfActive = False
	cf.ApplyComponentWhitelist()
	objectLoadState = cd.ObjectLoadState()
	loadObject = cd.LoadObject()

	for componentType in cd.component_order_as_ints.keys():
		try:
			componentId = components_to_load[componentType]
		except KeyError:
			continue
		try:
			match cd.component_int_to_name[componentType]:
				case 'COMPONENT_TYPE_CONTROLLABLE_PHYSICS':
					if objectLoadState.hasDroppedLootComponent: continue
					cf.ControllablePhysFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_RENDER':
					cf.RenderFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_SIMPLE_PHYSICS':
					if objectLoadState.hasDroppedLootComponent: continue
					cf.SimplePhysFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_CHARACTER':
					cf.CharacterFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_SCRIPT':
					cf.ScriptFactory(loadObject, objectLoadState, componentId, CheckIfNeedsPathfindingControl(components_to_load))

				case 'COMPONENT_TYPE_BOUNCER':
					cf.BouncerFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_DESTROYABLE':
					cf.DestroyableFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_SKILL':
					cf.SkillFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_SPAWNER':
					cf.SpawnerFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_ITEM':
					cf.ItemFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_MODULAR_BUILD':
					cf.ModularBuildFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_BUILD_CONTROLLER':
					cf.BuildControllerFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_VENDOR':
					cf.VendorFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_INVENTORY':
					cf.InventoryFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_PROJECTILE_PHYSICS':
					if objectLoadState.hasDroppedLootComponent: continue
					cf.ProjectilePhysicsFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_SHOOTING_GALLERY':
					cf.ShootingGalleryFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_RIGID_BODY_PHANTOM_PHYSICS':
					cf.RigibBodyPhantomPhysicsFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_CHEST':
					cf.ChestFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_COLLECTIBLE':
					cf.CollectibleFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_BLUEPRINT':
					cf.BlueprintFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_MOVING_PLATFORM':
					cf.MovingPlatformFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_PET':
					cf.PetFactory(loadObject, objectLoadState, componentId)
					print('Deleting COMPONENT_TYPE_MODEL and COMPONENT_TYPE_ITEM due to COMPONENT_TYPE_PET being loaded')
					toDel = cd.components['COMPONENT_TYPE_MODEL']
					del components_to_load[toDel]
					toDel = cd.components['COMPONENT_TYPE_ITEM']
					del components_to_load[toDel]

				case 'COMPONENT_TYPE_PLATFORM_BOUNDARY':
					cf.PlatformBoundaryFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_MODULE':
					cf.ModuleFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_ARCADE':
					cf.ArcadeFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_HAVOK_VEHICLE_PHYSICS':
					cf.HavokVehiclePhysicsFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_MOVEMENT_AI':
					cf.MovementAiFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_EXHIBIT':
					cf.ExhibitFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_MINIFIG':
					cf.MinifigFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_PROPERTY':
					cf.PropertyFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_MODEL_BUILDER':
					cf.ModelBuilderFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_SCRIPTED_ACTIVITY':
					cf.ScriptedActivityFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_PHANTOM_PHYSICS':
					if objectLoadState.hasDroppedLootComponent: continue
					cf.PhantomPhysicsFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_SPRING_PAD':
					cf.SpringPadFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_MODEL':
					cf.ModelFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_PROPERTY_ENTRANCE':
					cf.PropertyEntranceFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_PROPERTY_MANAGEMENT':
					cf.PropertyManagementFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_VEHICLE_PHYSICS':
					cf.VehiclePhysicsFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_PHYSICS_SYSTEM':
					cf.PhysicsSystemFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_REBUILD':
					cf.RebuildFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_SWITCH':
					cf.SwitchFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_ZONE_CONTROL':
					cf.ZoneControlFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_CHANGELING':
					cf.ChanglingFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_CHOICE_BUILD':
					cf.ChoiceBuildFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_PACKAGE':
					cf.PackageFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_SOUND_REPEATER':
					cf.SoundRepeaterFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_SOUND_AMBIENT_2D':
					cf.SoundAmbient2DFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_SOUND_AMBIENT_3D':
					cf.SoundAmbient3DFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_CUSTOM_BUILD_ASSEMBLY':
					cf.CustomBuildAssemblyFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_BASE_COMBAT_AI':
					cf.BaseCombatAiFactory(loadObject, objectLoadState, componentId, CheckIfNeedsPathfindingControl(components_to_load))

				case 'COMPONENT_TYPE_MODULE_ASSEMBLY':
					cf.ModuleAssemblyFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_SHOWCASE_MODEL_HANDLER':
					cf.ShowcaseModelHandlerFactory(loadObject, objectLoadState, componentId)
						
				case 'COMPONENT_TYPE_GENERIC_ACTIVATOR':
					cf.GenericActivatorFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_PROPERTY_VENDOR':
					cf.PropertyVendorFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_ROCKET_LAUNCH':
					cf.RocketLaunchFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_ROCKET_LANDING_COMPONENT':
					cf.RocketLandingComponentFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_RACING_CONTROL':
					cf.RacingControlFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_FACTION_TRIGGER':
					cf.FactionTriggerFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_MISSION_OFFER':
					cf.MissionOfferFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_RACING_STATS':
					cf.RacingStatsFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_LUP_EXHIBIT':
					cf.LupExhibitFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_BBB':
					cf.BBBFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_SOUND_TRIGGER':
					cf.SoundTriggerFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_PROXIMITY_MONITOR':
					cf.ProximityMonitorFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_RACING_SOUND_TRIGGER':
					cf.RacingSoundTriggerFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_ROCKET_LAUNCH_LUP':
					cf.RocketLaunchLupFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_DONATION_VENDOR':
					cf.DonationVendorFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_ACHIEVEMENT_VENDOR':
					cf.AchievmentVendorFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_GATE_RUSH':
					cf.GateRushFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_RAIL_ACTIVATOR':
					cf.RailActivatorFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_ROLLER':
					cf.RollerFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_PLAYER_FORCED_MOVEMENT':
					cf.PlayerForcedMovementFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_CRAFTING':
					cf.CraftingFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_POSSESSABLE':
					cf.PossessableFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_POSSESSOR':
					cf.PossessorFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_PROPERTY_PLAQUE':
					cf.PropertyPlaqueFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_BUILD_BORDER':
					cf.BuildBorderFactory(loadObject, objectLoadState, componentId)

				case 'COMPONENT_TYPE_CULLING_PLANE':
					cf.CullingPlaneFactory(loadObject, objectLoadState, componentId)
					
		except KeyError:
			print(f'Unknown component: {componentType}')

	if not 'is_smashable' in loadObject.configData and loadObject.bIsSmashable:
		loadObject.configData['is_smashable'] = True
			
	is_smashable = loadObject.configData['is_smashable']
	if is_smashable and not objectLoadState.hasDestroyable and loadObject.lotToSpawn != 176:
		objectLoadState.components.append('DESTROYABLE')

	if objectLoadState.hasMissionOffer and not objectLoadState.hasProximityMonitorComponent:
		objectLoadState.components.append('PROXIMITY_MONITOR')
		objectLoadState.hasProximityMonitorComponent = True
	
	if loadObject.field8_0x20 and loadObject.owner != 0:
		objectLoadState.components.append('OVERHEAD_ICON')

	trigger_id = loadObject.configData.get('trigger_id', 0)
	if trigger_id != 0:
		objectLoadState.components.append('TRIGGER')

	return objectLoadState

components_to_load = {}

# Testing code to load all components
for i in range(110):
	components_to_load[i] = i

objectLoadState = DoObjectPreLoad(components_to_load)

# Convert the components to ints
components_to_int = []
for component in objectLoadState.components:
	components_to_int.append([cd.components[f'COMPONENT_TYPE_{component}'], component])

# Print out the components in a more readable format
for comp in components_to_int:
	print(comp[0], comp[1])
