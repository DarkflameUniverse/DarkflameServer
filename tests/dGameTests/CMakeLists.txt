add_subdirectory(dComponentsTests)
set(DGAMETEST_SOURCES
	"GameDependencies.cpp"
)

foreach(file ${DCOMPONENTS_TESTS})
	list(APPEND DGAMETEST_SOURCES "dComponentsTests/${file}")
endforeach()

set(DGAMETEST_SOURCES ${DGAMETEST_SOURCES} PARENT_SCOPE)
