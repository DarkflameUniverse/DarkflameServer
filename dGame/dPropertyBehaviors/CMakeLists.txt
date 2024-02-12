set(DGAME_DPROPERTYBEHAVIORS_SOURCES
#header
	"BehaviorStates.h"
	"BlockDefinition.h"
	"ControlBehaviors.h"
	"PropertyBehavior.h"
	"State.h"
	"Strip.h"

#source
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
target_include_directories(dPropertyBehaviors PUBLIC
.
./ControlBehaviorMessages
)

target_link_libraries(dPropertyBehaviors PUBLIC dCommon dGame)
target_precompile_headers(dPropertyBehaviors REUSE_FROM dGameBase)
