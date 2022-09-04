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
	target_link_libraries(dCommon ZLIB::ZLIB)
endif()

# Fetch External (Non-Submodule) Libraries
if(WIN32)
	include(FetchContent)

	FetchContent_Declare(
		zlib
		URL https://github.com/madler/zlib/archive/refs/tags/v1.2.11.zip
		URL_HASH MD5=9d6a627693163bbbf3f26403a3a0b0b1
	)

	FetchContent_MakeAvailable(zlib)

	set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
	set_target_properties(zlib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}") # Why?
	add_library(ZLIB::ZLIB ALIAS zlib) # You're welcome
	target_link_libraries(dCommon zlib)

endif(WIN32)
