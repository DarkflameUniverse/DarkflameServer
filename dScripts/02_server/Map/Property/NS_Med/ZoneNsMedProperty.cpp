#include "ZoneNsMedProperty.h"
#include "Entity.h"

void ZoneNsMedProperty::SetGameVariables(Entity* self) {
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

	self->SetVar<std::vector<std::string>>(EnemiesSpawner,
		{ "Admirals", "AdmiralsWander", "Mechs", "Ronin", "RoninWander" });
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
	self->SetVar<std::vector<std::string>>(AmbientFXSpawner, { "Rockets" });
	self->SetVar<std::vector<std::string>>(BehaviorObjsSpawner, { });

	self->SetVar<uint32_t>(defeatedProperyFlag, 122);
	self->SetVar<uint32_t>(placedModelFlag, 123);
	self->SetVar<uint32_t>(guardMissionFlag, 1322);
	self->SetVar<uint32_t>(brickLinkMissionIDFlag, 1294);
	self->SetVar<std::string>(passwordFlag, "s3kratK1ttN");
	self->SetVar<LOT>(generatorIdFlag, 11031);
	self->SetVar<LOT>(orbIDFlag, 10226);
	self->SetVar<LOT>(behaviorQBID, 10445);
}
