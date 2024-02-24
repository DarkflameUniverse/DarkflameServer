set(DGAME_DCINEMA_SOURCES "Recorder.cpp"
    "Prefab.cpp"
    "Scene.cpp"
    "Play.cpp")

add_library(dCinema STATIC ${DGAME_DCINEMA_SOURCES})
target_precompile_headers(dCinema REUSE_FROM dGameBase)
target_link_libraries(dCinema 
    PUBLIC dPhysics dDatabase
    INTERFACE dUtilities dCommon dBehaviors dChatFilter dMission dInventory dComponents
)
