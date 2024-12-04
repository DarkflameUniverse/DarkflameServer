set(DLOCAL_SERVER_SOURCES
	"dllmain.cpp"
)

add_library(dLocalServer OBJECT ${DLOCAL_SERVER_SOURCES})

target_link_libraries(dLocalServer)
