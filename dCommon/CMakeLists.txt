set(DCOMMON_SOURCES
		"AMFDeserialize.cpp"
		"AmfSerialize.cpp"
		"BinaryIO.cpp"
		"dConfig.cpp"
		"Diagnostics.cpp"
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
	"${PROJECT_SOURCE_DIR}/thirdparty/mariadb-connector-cpp/include"
)

if (UNIX)
	find_package(ZLIB REQUIRED)
elseif (WIN32)
	include(FetchContent)

    set(WITH_GTEST OFF CACHE BOOL "" FORCE)
    set(ZLIB_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(ZLIB_COMPAT ON CACHE BOOL "Enable ZLIB compatibility mode" FORCE)

	# TODO Keep an eye on the zlib repository for an update to disable testing.  Don't forget to update CMakePresets
	FetchContent_Declare(
		zlib
		URL https://github.com/zlib-ng/zlib-ng/archive/refs/tags/2.2.2.zip
		URL_HASH MD5=2cf9199fb785ea579a2a9905a75c38b3
	)

	# Disable warning about no project version.
	set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
	# Disable warning about the minimum version of cmake used for bcrypt being deprecated in the future
	set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

	FetchContent_MakeAvailable(zlib)

	set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
	add_library(ZLIB::ZLIB ALIAS zlib)
else ()
	message(
		FATAL_ERROR
		"This platform does not have a way to use zlib.\nCreate an issue on GitHub with your build system so it can be configured."
	)
endif ()

target_link_libraries(dCommon
	PRIVATE ZLIB::ZLIB bcrypt tinyxml2
	INTERFACE dDatabase)
