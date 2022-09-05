set(DCOMMON_SOURCES "AMFFormat.cpp"
		"AMFDeserialize.cpp"
		"AMFFormat_BitStream.cpp"
		"BinaryIO.cpp"
		"dConfig.cpp"
		"Diagnostics.cpp"
		"dLogger.cpp"
		"GeneralUtils.cpp"
		"LDFFormat.cpp"
		"MD5.cpp"
		"Metrics.cpp"
		"NiPoint3.cpp"
		"NiQuaternion.cpp"
		"SHA512.cpp"
		"Type.cpp"
		"ZCompression.cpp")

include_directories(${PROJECT_SOURCE_DIR}/dCommon/)

add_library(dCommon STATIC ${DCOMMON_SOURCES})

target_link_libraries(dCommon bcrypt)

if (UNIX)
	find_package(ZLIB REQUIRED)
elseif (WIN32)
	include(FetchContent)

	# TODO Keep an eye on the zlib repository for an update to disable testing.  Don't forget to update CMakePresets
	FetchContent_Declare(
		zlib
		URL https://github.com/madler/zlib/archive/refs/tags/v1.2.11.zip
		URL_HASH MD5=9d6a627693163bbbf3f26403a3a0b0b1
	)

	# Disable warning about no project version.
	set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
	# Disable warning about the minimum version of cmake used for bcrypt being deprecated in the future
	set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

	FetchContent_MakeAvailable(zlib)

	set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
	set_target_properties(zlib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}")
	add_library(ZLIB::ZLIB ALIAS zlib)
else ()
	message(
		FATAL_ERROR
		"This platform does not have a way to use zlib.\nCreate an issue on GitHub with your build system so it can be configured."
	)
endif ()

target_link_libraries(dCommon ZLIB::ZLIB)
