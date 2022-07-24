add_subdirectory(dComponentsTests)
set(DGAMETEST_SOURCES)

foreach(file ${DCOMPONENTS_TESTS})
	set(DGAMETEST_SOURCES ${DGAMETEST_SOURCES} "dComponentsTests/${file}")
endforeach()

set(DGAMETEST_SOURCES ${DGAMETEST_SOURCES} PARENT_SCOPE)
