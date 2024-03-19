add_library(MD5 "MD5.cpp")

# Disable deprecation warnings on MD5.cpp for Apple Clang
if (APPLE)
	set_source_files_properties("MD5.cpp" PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations")
endif()
