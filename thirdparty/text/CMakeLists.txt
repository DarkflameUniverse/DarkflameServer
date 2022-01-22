add_library(ztd.text ${PROJECT_SOURCE_DIR}/thirdparty/text/source/ztd/idk/bit.intrinsic.c
${PROJECT_SOURCE_DIR}/thirdparty/text/source/ztd/idk/bit.load_store.c
${PROJECT_SOURCE_DIR}/thirdparty/text/source/ztd/idk/bit.memreverse.c
${PROJECT_SOURCE_DIR}/thirdparty/text/source/ztd/idk/encoding_detection.cpp)

set_property(TARGET ztd.text PROPERTY CXX_STANDARD 17)

target_include_directories(ztd.text PUBLIC "${PROJECT_SOURCE_DIR}/thirdparty/text/include")