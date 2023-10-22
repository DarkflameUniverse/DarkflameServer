import ComponentDefinitions as cd

def ApplyComponentWhitelist():
	# Dont care to actually do this, just want the call for now.
	pass

def ControllablePhysFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	if not objectLoadState.hasModelbehaviors:
		if not loadObject.hasDropEffect:
			if not loadObject.useSimplePhysics:
				objectLoadState.components.append('CONTROLLABLE_PHYSICS')
			else:
				objectLoadState.components.append('SIMPLE_PHYSICS')
	else:
		loadObject.physicsComponentId = componentId

def RenderFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	if loadObject.lotToSpawn == 176:
		return
	gmlevel = loadObject.configData.get('gmlevel', 0)
	if gmlevel > 0 and gmlevel < 6:
		componentId = 9940 # Override the componentId of the render component to be 9940, the Mythran Render Component
	renderDisabled = loadObject.configData.get('renderDisabled', False)
	if renderDisabled:
		return
	if objectLoadState.createdGameObject.type == 'BlockingVolume':
		return
	rejectedLots = [5937, 5938, 9741, 9742, 9862, 9863]
	if loadObject.lotToSpawn in rejectedLots:
		return
	if loadObject.lotToSpawn == 6368:
		FxFactory(loadObject, objectLoadState, componentId)
		return
	FxFactory(loadObject, objectLoadState, componentId)
	defaultWrapperAsset = ""
	hasNifAsset = False
	hasKfmAsset = False
	allowRenderWrapping = False
	nifName = loadObject.configData.get('nif_name', '')
	if nifName == '':
		print('TODO query cdc for this data in the Render component')
		nifName = 'mesh/something.nif' # if empty get nifname as the render_asset column of the RenderComponent table from the database
	if nifName[-1] == 'f':
		hasNifAsset = True
	elif nifName[-1] == 'm':
		hasKfmAsset = True
	if defaultWrapperAsset != '':
		if defaultWrapperAsset[-1] == 'm':
			allowRenderWrapping = not loadObject.configData.get('preventRenderWrapping', False)
	if allowRenderWrapping or hasKfmAsset or (not defaultWrapperAsset == 'PrimitiveModels' and not hasNifAsset and not loadObject.configData.get('CreateNULLRender', False)):
		objectLoadState.components.append('SKINNED_RENDER')
	else:
		objectLoadState.components.append('RENDER')

def FxFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('FX')

def SimplePhysFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	if not objectLoadState.hasModelbehaviors:
		objectLoadState.physicsComponentId = componentId
		return
	if not loadObject.hasDropEffect:
		return
	markedAsPhantom = loadObject.configData.get('markedAsPhantom', False)
	if markedAsPhantom:
		objectLoadState.components.append('PHANTOM_PHYSICS')
	else:
		objectLoadState.components.append('SIMPLE_PHYSICS')
	objectLoadState.hasPhysicsComponent = True

def CharacterFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('POSSESSOR')
	objectLoadState.components.append('MOUNT_CONTROL')
	objectLoadState.components.append('PET_CREATOR')
	objectLoadState.components.append('LEVEL_PROGRESSION')
	objectLoadState.components.append('PLAYER_FORCED_MOVEMENT')
	objectLoadState.components.append('CHARACTER')

def ScriptFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int, add_pathfinding_control: bool):
	custom_script_client = loadObject.configData.get('custom_script_client', '')
	custom_script_server = loadObject.configData.get('custom_script_server', '')
	if componentId == 0 and custom_script_client == '' and custom_script_server == '':
		return
	objectLoadState.components.append('SCRIPT')

	if not objectLoadState.hasProximityMonitorComponent:
		objectLoadState.components.append('PROXIMITY_MONITOR')
		objectLoadState.hasProximityMonitorComponent = True

	if not objectLoadState.hasPathfindingComponent and add_pathfinding_control:
		objectLoadState.components.append('PATHFINDING_CONTROL')
		objectLoadState.hasPathfindingComponent = True

def BouncerFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('BOUNCER')

def DestroyableFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('STATUS_EFFECT')
	objectLoadState.components.append('BUFF')
	objectLoadState.components.append('DESTROYABLE')
	objectLoadState.hasDestroyable = True

def SkillFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('SKILL')

def SpawnerFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('SPAWNER')
	# If game state is happy flower
	spawntemplate = loadObject.configData.get('spawntemplate', '')

	print('TODO query cdc for this data in the Spawner component')
	render_asset = loadObject.configData.get('render_asset', 'test.nif') # Get this from RenderComponent table
	render_id = loadObject.configData.get('render_id', '') # Get this from RenderComponent table

	if render_asset[-1] == 'f':
		objectLoadState.components.append('SKINNED_RENDER')
	else:
		objectLoadState.components.append('RENDER')

def ItemFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('ITEM')

def ModularBuildFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('MODULAR_BUILD')

def BuildControllerFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('BUILD_CONTROLLER')

def VendorFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	if not objectLoadState.hasProximityMonitorComponent:
		objectLoadState.components.append('PROXIMITY_MONITOR')
		objectLoadState.hasProximityMonitorComponent = True
	objectLoadState.components.append('VENDOR')

def InventoryFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('INVENTORY')

def ProjectilePhysicsFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	if not loadObject.hasDropEffect:
		objectLoadState.components.append('PROJECTILE_PHYSICS')
		objectLoadState.hasPhysicsComponent = True

def ShootingGalleryFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('SHOOTING_GALLERY')

def RigibBodyPhantomPhysicsFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('RIGID_BODY_PHANTOM_PHYSICS')
	objectLoadState.hasPhysicsComponent = True

def ChestFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('CHEST')

def CollectibleFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('COLLECTIBLE')
	if not objectLoadState.hasDestroyable:
		objectLoadState.components.append('DESTROYABLE')
		objectLoadState.hasDestroyable = True

def BlueprintFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('BLUEPRINT')

def MovingPlatformFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('MOVING_PLATFORM')

def PetFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	inInventory = loadObject.configData.get('inInventory', False)
	if inInventory:
		return
	
	if not objectLoadState.hasPathfindingComponent:
		objectLoadState.components.append('PATHFINDING_CONTROL')
		objectLoadState.hasPathfindingComponent = True
	objectLoadState.components.append('PET')

def PlatformBoundaryFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('PLATFORM_BOUNDARY')

def ModuleFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('MODULE')

def ArcadeFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('ARCADE')

def HavokVehiclePhysicsFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	use_simple_physics = loadObject.configData.get('use_simple_physics', False)
	if use_simple_physics:
		objectLoadState.components.append('SIMPLE_PHYSICS')
		objectLoadState.hasPhysicsComponent = True
	else:
		objectWorldState = loadObject.configData.get('objectWorldState', '')
		if objectWorldState == 0 or objectWorldState != 2:
			objectLoadState.components.append('HAVOK_VEHICLE_PHYSICS')
			objectLoadState.hasPhysicsComponent = True

def MovementAiFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('MOVEMENT_AI')

def ExhibitFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	# Sorry aron, doesnt do anything :(
	pass

def MinifigFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('MINIFIG')

def PropertyFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('PROPERTY')

def ModelBuilderFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('MODEL_BUILDER')

def ScriptedActivityFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('SCRIPTED_ACTIVITY')

def PhantomPhysicsFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	if not loadObject.hasDropEffect:
		objectLoadState.components.append('PHANTOM_PHYSICS')
		objectLoadState.hasPhysicsComponent = True

def SpringPadFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('SPRING_PAD')

def ModelFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	modelType = loadObject.configData.get('modelType', 0)
	if not objectLoadState.hasModelbehaviors:
		if not objectLoadState.hasPhysicsComponent:
			objectLoadState.components.append('SIMPLE_PHYSICS')
			objectLoadState.hasPhysicsComponent = True
	elif not objectLoadState.hasPhysicsComponent:
		if modelType == 0:
			objectLoadState.components.append('CONTROLLABLE_PHYSICS')
		else:
			objectLoadState.components.append('SIMPLE_PHYSICS')
		objectLoadState.hasPhysicsComponent = True
	if modelType != 0 and not objectLoadState.hasPathfindingComponent:
		objectLoadState.components.append('PATHFINDING_CONTROL')
		objectLoadState.hasPathfindingComponent = True

	hasPropertyObjectID = loadObject.configData.get('propertyObjectID', 0) != 0
	inInventory = loadObject.configData.get('inInventory', False)
	if hasPropertyObjectID and inInventory:
		objectLoadState.components.append('MODEL_BEHAVIORS')
	else:
		objectLoadState.components.append('MUTABLE_MODEL_BEHAVIORS')

def PropertyEntranceFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('PROPERTY_ENTRANCE')

def PropertyManagementFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('PROPERTY_MANAGEMENT')

def VehiclePhysicsFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('VEHICLE_PHYSICS')
	objectLoadState.hasPhysicsComponent = True

def PhysicsSystemFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	if not objectLoadState.hasModelbehaviors:
		if not loadObject.hasDropEffect:
			objectLoadState.components.append('PHYSICS_SYSTEM')
			objectLoadState.hasPhysicsComponent = True

def RebuildFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('REBUILD')
	if not objectLoadState.hasDestroyable:
		objectLoadState.components.append('DESTROYABLE')
		objectLoadState.hasDestroyable = True
	loadObject.bIsSmashable = True

def BaseCombatAiFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int, add_pathfinding_control: bool):
	if not objectLoadState.hasProximityMonitorComponent:
		objectLoadState.components.append('PROXIMITY_MONITOR')
		objectLoadState.hasProximityMonitorComponent = True
	
	if not objectLoadState.hasPathfindingComponent and add_pathfinding_control:
		objectLoadState.components.append('PATHFINDING_CONTROL')
		objectLoadState.hasPathfindingComponent = True

	objectLoadState.components.append('BASE_COMBAT_AI')

def ProximityMonitorFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	print('TODO query cdc for this data in prox mon')
	# There must be a row for this component in the ProximityMonitorComponent table to get this component
	LoadOnClient = loadObject.configData.get('LoadOnClient', False) # Get this from ProximityMonitorComponent table if you know there is a column for it
	if LoadOnClient:
		objectLoadState.components.append('PROXIMITY_MONITOR')
		objectLoadState.hasProximityMonitorComponent = True

def DonationVendorFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	if not objectLoadState.hasProximityMonitorComponent:
		objectLoadState.components.append('PROXIMITY_MONITOR')
		objectLoadState.hasProximityMonitorComponent = True
	objectLoadState.components.append('DONATION_VENDOR')

def AchievmentVendorFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	if not objectLoadState.hasProximityMonitorComponent:
		objectLoadState.components.append('PROXIMITY_MONITOR')
		objectLoadState.hasProximityMonitorComponent = True

	objectLoadState.components.append('ACHIEVEMENT_VENDOR')

def CraftingFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	# Game verson must be above 999 for this to work!
	gameVersion = 1000
	if gameVersion > 999:
		objectLoadState.components.append('CRAFTING')

# Technically Possessable and Possessor are the exact same thing
def PossessableFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('POSSESSABLE')

def PossessorFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('POSSESSOR')

def PropertyPlaqueFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('PROPERTY_PLAQUE')

def BuildBorderFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('BUILD_BORDER')

def CullingPlaneFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('CULLING_PLANE')

def GateRushFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('GATE_RUSH')

def RailActivatorFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('RAIL_ACTIVATOR')

def RollerFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('ROLLER')

def PlayerForcedMovementFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('PLAYER_FORCED_MOVEMENT')

def RacingSoundTriggerFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('RACING_SOUND_TRIGGER')

def RocketLaunchLupFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('ROCKET_LAUNCH_LUP')


def ModuleAssemblyFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('MODULE_ASSEMBLY')

def ShowcaseModelHandlerFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('SHOWCASE_MODEL_HANDLER')

def GenericActivatorFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('GENERIC_ACTIVATOR')

def PropertyVendorFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('PROPERTY_VENDOR')

def RocketLaunchFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('ROCKET_LAUNCH')

def RocketLandingComponentFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('ROCKET_LANDING_COMPONENT')

def RacingControlFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('RACING_CONTROL')

def FactionTriggerFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('FACTION_TRIGGER')

def MissionOfferFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('MISSION_OFFER')
	objectLoadState.hasMissionOffer = True

def RacingStatsFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('RACING_STATS')

def LupExhibitFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('LUP_EXHIBIT')

def BBBFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('BBB')

def SoundTriggerFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('SOUND_TRIGGER')

def SwitchFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('SWITCH')

def ZoneControlFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('ZONE_CONTROL')

def ChanglingFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('CHANGELING')

def ChoiceBuildFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('CHOICE_BUILD')

def PackageFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	# Literally returns nothing
	pass

def SoundRepeaterFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('SOUND_REPEATER')

def SoundAmbient2DFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	if loadObject.isLocalPlayer:
		objectLoadState.components.append('SOUND_AMBIENT_2D')

def SoundAmbient3DFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('SOUND_AMBIENT_3D')

def CustomBuildAssemblyFactory(loadObject: cd.LoadObject, objectLoadState: cd.ObjectLoadState, componentId: int):
	objectLoadState.components.append('CUSTOM_BUILD_ASSEMBLY')
