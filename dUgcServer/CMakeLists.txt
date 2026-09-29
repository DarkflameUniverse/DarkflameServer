# The UGC server (docs/UgcServer.md), in folders by role:
#   Bricks/      the client's brick data: primitives, Materials.xml, LU Toolbox's palette, the CDClient's module data
#   Model/       player models built from LXFML, car and rocket module combinations, glitter
#   Render/      the software rasterizer, ambient occlusion, hidden-face removal, rays, icon parameters and pose
#   Formats/     the files written for the client and the dashboard (NIF, DDS, PNG, sd0)
#   Processing/  the job queue and workers, the CPU budget, stored files
#   Toolbox/     processor=toolbox-blender: LU Toolbox itself in a headless Blender (an external program), its worker
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
	"Toolbox/UgcToolbox.cpp"
	"Toolbox/UgcToolboxJob.cpp"
	"Toolbox/UgcToolboxProtocol.cpp"
)

add_library(dUgc STATIC ${DUGC_SOURCES})
target_include_directories(dUgc PUBLIC "." "Bricks" "Formats" "Model" "Processing" "Render" "Toolbox"
	"${PROJECT_SOURCE_DIR}/thirdparty/MD5" "${PROJECT_SOURCE_DIR}/thirdparty/nlohmann")
target_link_libraries(dUgc PUBLIC dCommon glm::glm tinyxml2 MD5 PRIVATE embree)
# Intel Open Image Denoise, when built with DLU_OIDN (thirdparty/CMakeLists.txt): the denoise=oidn setting
if(DLU_OIDN)
	target_link_libraries(dUgc PRIVATE OpenImageDenoise)
	target_compile_definitions(dUgc PRIVATE DLU_OIDN)
endif()
# Embree on Intel GPUs through SYCL (ray_backend=embree-gpu), when built with DLU_EMBREE_SYCL: libdlu_embree_sycl
# (EmbreeSycl/, Embree built with its SYCL support and the GPU kernels) is its own project built by a SYCL compiler
# (DLU_SYCL_CXX: Intel oneAPI DPC++'s icpx, or the open source DPC++'s clang++; found through ONEAPI_ROOT or
# DPCPP_ROOT), put next to the servers and loaded by the UGC server when asked for, so nothing else needs oneAPI.
option(DLU_EMBREE_SYCL "Build the UGC server's Intel GPU ray tracer (ray_backend=embree-gpu; needs a SYCL compiler)" OFF)
if(DLU_EMBREE_SYCL)
	find_program(DLU_SYCL_CXX NAMES icpx clang++ PATHS "$ENV{ONEAPI_ROOT}/compiler/latest/bin" "$ENV{DPCPP_ROOT}/bin" NO_DEFAULT_PATH)
	if(NOT DLU_SYCL_CXX)
		find_program(DLU_SYCL_CXX NAMES icpx)
	endif()
	if(NOT DLU_SYCL_CXX)
		message(FATAL_ERROR "DLU_EMBREE_SYCL needs a SYCL compiler: set DLU_SYCL_CXX, ONEAPI_ROOT (Intel oneAPI) or DPCPP_ROOT (open source DPC++)")
	endif()
	get_filename_component(DLU_SYCL_BIN "${DLU_SYCL_CXX}" DIRECTORY)
	find_program(DLU_SYCL_CC NAMES icx clang PATHS "${DLU_SYCL_BIN}" NO_DEFAULT_PATH REQUIRED)
	include(ExternalProject)
	set(DLU_EMBREE_SYCL_LIBRARY "${CMAKE_SHARED_LIBRARY_PREFIX}dlu_embree_sycl${CMAKE_SHARED_LIBRARY_SUFFIX}")
	ExternalProject_Add(dlu_embree_sycl
		SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/EmbreeSycl"
		BINARY_DIR "${CMAKE_BINARY_DIR}/embree_sycl"
		CMAKE_ARGS -DCMAKE_CXX_COMPILER=${DLU_SYCL_CXX} -DCMAKE_C_COMPILER=${DLU_SYCL_CC} -DCMAKE_BUILD_TYPE=Release
		INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_if_different "<BINARY_DIR>/${DLU_EMBREE_SYCL_LIBRARY}" "${CMAKE_BINARY_DIR}/"
		BUILD_ALWAYS TRUE
		USES_TERMINAL_BUILD TRUE)
	target_sources(dUgc PRIVATE "Render/UgcRaysEmbreeGpu.cpp")
	target_compile_definitions(dUgc PRIVATE DLU_EMBREE_SYCL)
	target_link_libraries(dUgc PRIVATE ${CMAKE_DL_LIBS})
	add_dependencies(dUgc dlu_embree_sycl)
	message(STATUS "Intel GPU ray tracing (embree-gpu) built with ${DLU_SYCL_CXX}")
endif()

# HIPRT and Orochi, when built with DLU_HIPRT (thirdparty/CMakeLists.txt): the ray_backend=hiprt setting
if(DLU_HIPRT)
	target_sources(dUgc PRIVATE "Render/UgcRaysHiprt.cpp")
	# hiprtew's function pointers have the names of HIPRT's functions: hidden, or HIPRT's library would call them
	# instead of its own (the servers export their symbols)
	if(NOT MSVC)
		set_source_files_properties("Render/UgcRaysHiprt.cpp" PROPERTIES COMPILE_OPTIONS "-fvisibility=hidden")
	endif()
	target_include_directories(dUgc PRIVATE "${DLU_HIPRT_INCLUDE_DIR}")
	target_link_libraries(dUgc PRIVATE dlu_orochi)
	target_compile_definitions(dUgc PRIVATE DLU_HIPRT DLU_HIPRT_INCLUDE_DIR="${DLU_HIPRT_INCLUDE_DIR}")
endif()

# The script Blender runs for processor=toolbox-blender, next to the servers (Blender is never linked in)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Toolbox/dlu_toolbox_worker.py" "${PROJECT_BINARY_DIR}/ugc-toolbox/dlu_toolbox_worker.py" COPYONLY)

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
