set(DCOMMON_SOURCES "AMFFormat.cpp"
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

add_library(dCommon ${DCOMMON_SOURCES})

target_link_libraries(dCommon libbcrypt)

if (UNIX)
    target_link_libraries(dCommon zlib)
endif()