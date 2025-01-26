set(DWEB_SOURCES
	"Web.cpp")

add_library(dWeb STATIC ${DWEB_SOURCES})

target_include_directories(dWeb PUBLIC ".")
target_link_libraries(dWeb dCommon mongoose)
