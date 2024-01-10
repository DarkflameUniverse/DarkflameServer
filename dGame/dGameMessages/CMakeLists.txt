set(DGAME_DGAMEMESSAGES_SOURCES
	"GameMessageHandler.cpp"
	"GameMessages.cpp"
	"PropertyDataMessage.cpp"
	"PropertySelectQueryProperty.cpp"
	"GameMessage.cpp")

add_library(dGameMessages STATIC ${DGAME_DGAMEMESSAGES_SOURCES})
target_link_libraries(dGameMessages PUBLIC dDatabase)
target_precompile_headers(dGameMessages REUSE_FROM dGameBase)
