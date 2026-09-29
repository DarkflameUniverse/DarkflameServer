# The UGC server (docs/UgcServer.md), in folders by role:
#   Bricks/      the client's brick data: primitives, Materials.xml, LU Toolbox's palette, the CDClient's module data
#   Model/       player models built from LXFML, car and rocket module combinations, glitter
#   Render/      the software rasterizer, ambient occlusion, hidden-face removal, rays, icon parameters and pose
#   Formats/     the files written for the client and the dashboard (NIF, DDS, PNG, sd0)
#   Processing/  the job queue and workers, the CPU budget, stored files
#   UgcServer.cpp  the server: settings, the master link, HTTP routes

# The pure part of the UGC server (brick data, meshes, rendering, file formats, storage), shared with the tests
set(DUGC_SOURCES
	"Bricks/UgcBricks.cpp"
	"Bricks/UgcPalette.cpp"
	"Formats/UgcFormats.cpp"
	"Model/UgcGlitter.cpp"
	"Model/UgcModel.cpp"
	"Model/UgcModular.cpp"
	"Processing/UgcJobs.cpp"
	"Processing/UgcStorage.cpp"
	"Processing/UgcThrottle.cpp"
	"Render/UgcHsr.cpp"
	"Render/UgcIconParams.cpp"
	"Render/UgcIconPose.cpp"
	"Render/UgcRays.cpp"
	"Render/UgcRender.cpp"
)

add_library(dUgc STATIC ${DUGC_SOURCES})
target_include_directories(dUgc PUBLIC "." "Bricks" "Formats" "Model" "Processing" "Render"
	"${PROJECT_SOURCE_DIR}/thirdparty/MD5" "${PROJECT_SOURCE_DIR}/thirdparty/nlohmann")
target_link_libraries(dUgc PUBLIC dCommon glm::glm tinyxml2 MD5 PRIVATE embree)

add_executable(UgcServer "UgcServer.cpp" "Processing/UgcProcessor.cpp" "Bricks/UgcCdClient.cpp")

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
	set_source_files_properties("Render/UgcRender.cpp" "Render/UgcHsr.cpp" "Render/UgcRays.cpp" "Model/UgcModel.cpp" "Bricks/UgcPalette.cpp" "Model/UgcGlitter.cpp" PROPERTIES COMPILE_OPTIONS "-O2")
endif()
