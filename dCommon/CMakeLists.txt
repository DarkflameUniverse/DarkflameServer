set(DCOMMON_SOURCES
		"AMFDeserialize.cpp"
		"ClientSysInfo.cpp"
		"AmfSerialize.cpp"
		"BinaryIO.cpp"
		"dConfig.cpp"
		"Diagnostics.cpp"
		"TrafficStats.cpp"
		"Locale.cpp"
		"Logger.cpp"
		"Game.cpp"
		"GeneralUtils.cpp"
		"LDFFormat.cpp"
		"Metrics.cpp"
		"NiPoint3.cpp"
		"NiQuaternion.cpp"
		"Demangler.cpp"
		"ZCompression.cpp"
		"BrickByBrickFix.cpp"
		"BinaryPathFinder.cpp"
		"FdbToSqlite.cpp"
		"JSONUtils.cpp"
		"TinyXmlUtils.cpp"
		"Sd0.cpp"
		"Lxfml.cpp"
		"LxfmlBugged.cpp"
		"VanityXml.cpp"
		"ScheduleRules.cpp"
		"EventParts.cpp"
		"VanityEvents.cpp"
		"ZoneFile.cpp"
		"LevelFile.cpp"
		"Raw.cpp"
		"ZoneScenes.cpp"
		"Process.cpp"
		"Permissions.cpp"
		"PermissionGrants.cpp"
		"NifFile.cpp"
)

# Workaround for compiler bug where the optimized code could result in a memcpy of 0 bytes, even though that isnt possible.
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=97185
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set_source_files_properties("FdbToSqlite.cpp" PROPERTIES COMPILE_FLAGS "-Wno-stringop-overflow")
endif()

add_subdirectory(dClient)

foreach(file ${DCOMMON_DCLIENT_SOURCES})
	set(DCOMMON_SOURCES ${DCOMMON_SOURCES} "dClient/${file}")
endforeach()

add_library(dCommon STATIC ${DCOMMON_SOURCES})
target_include_directories(dCommon
	PUBLIC "." "dClient" "dEnums"
	PRIVATE
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase"
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase/ITables"
	"${PROJECT_SOURCE_DIR}/dDatabase/CDClientDatabase"
)

if (UNIX)
	find_package(ZLIB REQUIRED)
elseif (WIN32)
	include(FetchContent)

	FetchContent_Declare(
		zlib
		URL https://github.com/madler/zlib/archive/refs/tags/v1.3.2.zip
		URL_HASH MD5=adbba6eef8960c3412818b2e241f46dc
		GIT_PROGRESS TRUE
		GIT_SHALLOW 1
	)

	# Disable warning about no project version.
	set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
	# Disable warning about the minimum version of cmake used for bcrypt being deprecated in the future
	set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
	# Disable zlib tests
	set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)

	FetchContent_MakeAvailable(zlib)

	set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
else ()
	message(
		FATAL_ERROR
		"This platform does not have a way to use zlib.\nCreate an issue on GitHub with your build system so it can be configured."
	)
endif ()

# Build identifier (BuildInfo.h). The generator runs on every build but only rewrites BuildInfo.cpp when the
# commit, branch, dirty state or build kind changed, so only that one file recompiles.
set(BUILDINFO_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/BuildInfo.cpp")
add_custom_target(BuildInfoGenerate
	COMMAND "${CMAKE_COMMAND}"
		"-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
		"-DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/BuildInfo.cpp.in"
		"-DOUTPUT=${BUILDINFO_SOURCE}"
		"-DVERSION_MAJOR=${PROJECT_VERSION_MAJOR}"
		"-DVERSION_MINOR=${PROJECT_VERSION_MINOR}"
		"-DVERSION_PATCH=${PROJECT_VERSION_PATCH}"
		-P "${PROJECT_SOURCE_DIR}/cmake/BuildInfo.cmake"
	BYPRODUCTS "${BUILDINFO_SOURCE}"
	COMMENT "Checking build identifier"
	VERBATIM
)
add_library(dBuildInfo STATIC "${BUILDINFO_SOURCE}")
target_include_directories(dBuildInfo PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_dependencies(dBuildInfo BuildInfoGenerate)

target_link_libraries(dCommon
	PUBLIC glm::glm dBuildInfo
	PRIVATE ZLIB::ZLIB bcrypt tinyxml2
	INTERFACE dDatabase)
