set (SQLITE3_SOURCES
	"CppSQLite3.cpp"
	"sqlite3.c"
)

add_library(sqlite3 ${SQLITE3_SOURCES})

if(UNIX)
	# Add warning disable flags and link Unix libraries to sqlite3
	target_link_libraries(sqlite3 pthread dl m)

	#  -Wno-unused-result -Wno-unknown-pragmas -fpermissive
	target_compile_options(sqlite3 PRIVATE)
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		target_compile_options(sqlite3 PRIVATE "-Wno-return-local-addr" "-Wno-maybe-uninitialized")
	else()
		target_compile_options(sqlite3 PRIVATE "-Wno-return-stack-address" "-Wno-uninitialized" "-Wno-deprecated-declarations")
	endif()
endif()
