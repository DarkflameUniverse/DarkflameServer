set(DSCRIPTS_SOURCES_EQUIPMENTTRIGGERSSCRIPTS
	"CoilBackpackBase.cpp"
	"ImaginationBackPack.cpp"
	"TrialFactionArmorServer.cpp")

add_library(dScriptsEquipmentTriggers OBJECT ${DSCRIPTS_SOURCES_EQUIPMENTTRIGGERSSCRIPTS})
target_include_directories(dScriptsEquipmentTriggers PUBLIC ".")
target_precompile_headers(dScriptsEquipmentTriggers REUSE_FROM dScriptsBase)
