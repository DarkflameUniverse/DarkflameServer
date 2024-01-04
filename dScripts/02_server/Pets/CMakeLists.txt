set(DSCRIPTS_SOURCES_02_SERVER_PETS 
	"PetFromDigServer.cpp"
	"PetFromObjectServer.cpp"
	"DamagingPets.cpp")

add_library(dScriptsServerPets OBJECT ${DSCRIPTS_SOURCES_02_SERVER_PETS})
target_include_directories(dScriptsServerPets PUBLIC ".")
target_precompile_headers(dScriptsServerPets REUSE_FROM dScriptsBase)

