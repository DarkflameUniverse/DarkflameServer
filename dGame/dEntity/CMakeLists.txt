set(DGAME_DENTITY_SOURCES
	"EntityCallbackTimer.cpp"
	"EntityTimer.cpp")

add_library(dEntity STATIC ${DGAME_DENTITY_SOURCES})
target_include_directories(dEntity PUBLIC ".")
target_precompile_headers(dEntity REUSE_FROM dGameBase)
