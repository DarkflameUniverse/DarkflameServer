set(DGAME_DPROPERTYBEHAVIORS_SOURCES
	"PropertyBehavior.cpp"
	"State.cpp"
	"Strip.cpp"
	"BlockDefinition.cpp"
	"ControlBehaviors.cpp"
)

add_subdirectory(ControlBehaviorMessages)

foreach(file ${DGAME_DPROPERTYBEHAVIORS_CONTROLBEHAVIORMESSAGES})
	set(DGAME_DPROPERTYBEHAVIORS_SOURCES ${DGAME_DPROPERTYBEHAVIORS_SOURCES} "ControlBehaviorMessages/${file}")
endforeach()

add_library(dPropertyBehaviors STATIC ${DGAME_DPROPERTYBEHAVIORS_SOURCES})
target_link_libraries(dPropertyBehaviors PRIVATE dDatabaseCDClient)
target_include_directories(dPropertyBehaviors PUBLIC "." "ControlBehaviorMessages"
	PRIVATE
	"${PROJECT_SOURCE_DIR}/dCommon/dClient" # ControlBehaviors.cpp uses AssetManager
	"${PROJECT_SOURCE_DIR}/dGame/dGameMessages" # GameMessages.h
	"${PROJECT_SOURCE_DIR}/dGame/dComponents" # ModelComponent.h
)
target_precompile_headers(dPropertyBehaviors REUSE_FROM dGameBase)
