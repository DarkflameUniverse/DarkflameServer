# The pure part of the UGC server (brick data, meshes, rendering, file formats, storage), shared with the tests
set(DUGC_SOURCES
	"UgcBricks.cpp"
	"UgcFormats.cpp"
	"UgcGlitter.cpp"
	"UgcIconParams.cpp"
	"UgcIconPose.cpp"
	"UgcJobs.cpp"
	"UgcModel.cpp"
	"UgcModular.cpp"
	"UgcPalette.cpp"
	"UgcRender.cpp"
	"UgcStorage.cpp"
	"UgcThrottle.cpp"
)

add_library(dUgc STATIC ${DUGC_SOURCES})
target_include_directories(dUgc PUBLIC "." "${PROJECT_SOURCE_DIR}/thirdparty/MD5" "${PROJECT_SOURCE_DIR}/thirdparty/nlohmann")
target_link_libraries(dUgc PUBLIC dCommon glm::glm tinyxml2 MD5)

add_executable(UgcServer "UgcServer.cpp" "UgcProcessor.cpp" "UgcCdClient.cpp")

target_include_directories(UgcServer PRIVATE
	"${PROJECT_SOURCE_DIR}/dCommon"
	"${PROJECT_SOURCE_DIR}/dCommon/dClient"
	"${PROJECT_SOURCE_DIR}/dCommon/dEnums"
	"${PROJECT_SOURCE_DIR}/dDatabase"
	"${PROJECT_SOURCE_DIR}/dDatabase/CDClientDatabase"
	"${PROJECT_SOURCE_DIR}/dDatabase/CDClientDatabase/CDClientTables"
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase"
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase/ITables"
	"${PROJECT_SOURCE_DIR}/dNet"
	"${PROJECT_SOURCE_DIR}/dWeb"
	"${PROJECT_SOURCE_DIR}/dServer"
	"${PROJECT_SOURCE_DIR}/thirdparty"
	"${PROJECT_SOURCE_DIR}/thirdparty/nlohmann"
)

target_compile_definitions(UgcServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")
target_link_libraries(UgcServer ${COMMON_LIBRARIES} dWeb dServer dUgc)

# Rendering is far too slow unoptimized (minutes a model instead of seconds), so it is optimized in every build type
if(NOT MSVC)
	set_source_files_properties("UgcRender.cpp" "UgcModel.cpp" "UgcPalette.cpp" "UgcGlitter.cpp" PROPERTIES COMPILE_OPTIONS "-O2")
endif()
