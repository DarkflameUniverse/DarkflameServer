set(DDASHBOARDSERVER_BLUEPRINTS
	"AuthBlueprint.cpp"
	"ApiBlueprint.cpp"
	"PageBlueprint.cpp"
	"PlayKeysBlueprint.cpp"
	"CharactersBlueprint.cpp"
	"MailBlueprint.cpp"
	"BugReportsBlueprint.cpp"
	"ModerationBlueprint.cpp"
)

foreach(file ${DDASHBOARDSERVER_BLUEPRINTS})
	set(DDASHBOARDSERVER_BLUEPRINTS_SOURCES ${DDASHBOARDSERVER_BLUEPRINTS_SOURCES} "blueprints/${file}" PARENT_SCOPE)
endforeach()
