set(DGAME_DPROPERTYBEHAVIORS_SOURCES
	"BlockDefinition.cpp"
	"ControlBehaviors.cpp"
)

add_subdirectory(ControlBehaviorMessages)

foreach(file ${DGAME_DPROPERTYBEHAVIORS_CONTROLBEHAVIORMESSAGES})
	set(DGAME_DPROPERTYBEHAVIORS_SOURCES ${DGAME_DPROPERTYBEHAVIORS_SOURCES} "ControlBehaviorMessages/${file}")
endforeach()

add_library(dPropertyBehaviors STATIC ${DGAME_DPROPERTYBEHAVIORS_SOURCES})
target_precompile_headers(dPropertyBehaviors REUSE_FROM dGameBase)
