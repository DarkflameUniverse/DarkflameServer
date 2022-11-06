#include "ZoneAgMedProperty.h"
#include "Entity.h"

void ZoneAgMedProperty::SetGameVariables(Entity* self) {

	self->SetVar<std::string>(ClaimMarkerGroup, "ClaimMarker");
	self->SetVar<std::string>(GeneratorGroup, "Generator");
	self->SetVar<std::string>(GuardGroup, "Guard");
	self->SetVar<std::string>(PropertyPlaqueGroup, "PropertyPlaque");
	self->SetVar<std::string>(PropertyVendorGroup, "PropertyVendor");
	self->SetVar<std::string>(SpotsGroup, "Spots");
	self->SetVar<std::string>(MSCloudsGroup, "maelstrom");
	self->SetVar<std::string>(EnemiesGroup, "Enemies");
	self->SetVar<std::string>(FXManagerGroup, "FXObject");
	self->SetVar<std::string>(ImagOrbGroup, "Orb");
	self->SetVar<std::string>(GeneratorFXGroup, "GeneratorFX");

	self->SetVar<std::vector<std::string>>(EnemiesSpawner, {
		"StrombieWander", "Strombies", "Mechs", "OtherEnemy"
		});
	self->SetVar<std::string>(ClaimMarkerSpawner, "ClaimMarker");
	self->SetVar<std::string>(GeneratorSpawner, "Generator");
	self->SetVar<std::string>(DamageFXSpawner, "MaelstromFX");
	self->SetVar<std::string>(FXSpotsSpawner, "MaelstromSpots");
	self->SetVar<std::string>(PropertyMGSpawner, "PropertyGuard");
	self->SetVar<std::string>(ImageOrbSpawner, "Orb");
	self->SetVar<std::string>(GeneratorFXSpawner, "GeneratorFX");
	self->SetVar<std::string>(SmashablesSpawner, "Smashables");
	self->SetVar<std::string>(FXManagerSpawner, "FXObject");
	self->SetVar<std::string>(PropObjsSpawner, "BankObj");
	self->SetVar<std::vector<std::string>>(AmbientFXSpawner, { "BirdFX", "SunBeam" });
	self->SetVar<std::vector<std::string>>(BehaviorObjsSpawner, {});

	self->SetVar<uint32_t>(defeatedProperyFlag, 118);
	self->SetVar<uint32_t>(placedModelFlag, 119);
	self->SetVar<uint32_t>(guardMissionFlag, 1293);
	self->SetVar<uint32_t>(brickLinkMissionIDFlag, 1294);
	self->SetVar<std::string>(passwordFlag, "s3kratK1ttN");
	self->SetVar<LOT>(generatorIdFlag, 10118);
	self->SetVar<LOT>(orbIDFlag, 10226);
	self->SetVar<LOT>(behaviorQBID, 10445);
}
