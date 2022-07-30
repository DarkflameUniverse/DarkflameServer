#include "ZoneGfProperty.h"
#include "Entity.h"

void ZoneGfProperty::SetGameVariables(Entity* self) {
	self->SetVar<std::string>(ClaimMarkerGroup, "BehavQB");
	self->SetVar<std::string>(GeneratorGroup, "Generator");
	self->SetVar<std::string>(GuardGroup, "Guard");
	self->SetVar<std::string>(PropertyPlaqueGroup, "PropertyPlaque");
	self->SetVar<std::string>(PropertyVendorGroup, "PropertyVendor");
	self->SetVar<std::string>(SpotsGroup, "Spots");
	self->SetVar<std::string>(MSCloudsGroup, "Clouds");
	self->SetVar<std::string>(EnemiesGroup, "Enemies");
	self->SetVar<std::string>(FXManagerGroup, "FXManager");
	self->SetVar<std::string>(ImagOrbGroup, "Orb");
	self->SetVar<std::string>(GeneratorFXGroup, "GeneratorFX");

	self->SetVar<std::vector<std::string>>(EnemiesSpawner,
		{ "PiratesWander", "PiratesGen", "AdmiralsWander", "AdmiralsGen" });
	self->SetVar<std::string>(ClaimMarkerSpawner, "BehavPlat");
	self->SetVar<std::string>(GeneratorSpawner, "Generator");
	self->SetVar<std::string>(DamageFXSpawner, "Clouds");
	self->SetVar<std::string>(FXSpotsSpawner, "Spots");
	self->SetVar<std::string>(PropertyMGSpawner, "Guard");
	self->SetVar<std::string>(ImageOrbSpawner, "Orb");
	self->SetVar<std::string>(GeneratorFXSpawner, "GeneratorFX");
	self->SetVar<std::string>(SmashablesSpawner, "Smashables");
	self->SetVar<std::string>(FXManagerSpawner, "FXManager");
	self->SetVar<std::string>(PropObjsSpawner, "BankObj");
	self->SetVar<std::vector<std::string>>(AmbientFXSpawner, { "Birds", "Falls", "Sunbeam" });
	self->SetVar<std::vector<std::string>>(BehaviorObjsSpawner, { "TrappedPlatform", "IceBarrier", "FireBeast" });

	self->SetVar<uint32_t>(defeatedProperyFlag, 98);
	self->SetVar<uint32_t>(placedModelFlag, 106);
	self->SetVar<uint32_t>(guardMissionFlag, 873);
	self->SetVar<uint32_t>(brickLinkMissionIDFlag, 949);
	self->SetVar<std::string>(passwordFlag, "s3kratK1ttN");
	self->SetVar<LOT>(generatorIdFlag, 11109);
	self->SetVar<LOT>(orbIDFlag, 10226);
	self->SetVar<LOT>(behaviorQBID, 11001);
}
