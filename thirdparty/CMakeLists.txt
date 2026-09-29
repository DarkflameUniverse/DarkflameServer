# Source Code for recast
add_subdirectory(recastnavigation)

# Turn off tinyxml2 testing
set(tinyxml2_BUILD_TESTING OFF)
# Source Code for tinyxml2
add_subdirectory(tinyxml2)

# Source Code for libbcrypt.  Uses a file glob instead to get around Windows build issues.
file(
	GLOB SOURCES_LIBBCRYPT
	LIST_DIRECTORIES false
	RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
	${CMAKE_CURRENT_SOURCE_DIR}/libbcrypt/src/*.c
)

add_library(bcrypt ${SOURCES_LIBBCRYPT})

# Because we are not using the libbcrypt CMakeLists.txt, we need to include these headers for the library to use.
# fortunately they are only needed for building the libbcrypt directory and nothing else, so these are marked private.

if(NOT WIN32)
	target_include_directories(bcrypt PRIVATE "libbcrypt/include/bcrypt")
endif()

# Need to define this on Clang and GNU for 'strdup' support
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
	target_compile_definitions(bcrypt PRIVATE "_POSIX_C_SOURCE=200809L")
endif()

target_include_directories(bcrypt INTERFACE "libbcrypt/include")
target_include_directories(bcrypt PRIVATE "libbcrypt/src")

# Source code for sqlite
add_subdirectory(SQLite)

# Source code for magic_enum
add_subdirectory(magic_enum)

# Create our third party library objects
add_subdirectory(raknet)

include(FetchContent)

# Download Backtrace if configured
if(UNIX AND NOT APPLE)
	if (${INCLUDE_BACKTRACE} AND ${COMPILE_BACKTRACE})
		FetchContent_Declare(
			backtrace
			GIT_REPOSITORY https://github.com/ianlancetaylor/libbacktrace.git
			GIT_PROGRESS TRUE
			GIT_SHALLOW 1
		)

		FetchContent_MakeAvailable(backtrace)

		if (NOT EXISTS ${backtrace_SOURCE_DIR}/.libs)
			set(backtrace_make_cmd "${backtrace_SOURCE_DIR}/configure --prefix=\"/usr\" --enable-shared --with-system-libunwind")

			execute_process(
				COMMAND bash -c "cd ${backtrace_SOURCE_DIR} && ${backtrace_make_cmd} && make && cd ${CMAKE_SOURCE_DIR}"
			)
		endif()

		link_directories(${backtrace_SOURCE_DIR}/.libs/)
		include_directories(${backtrace_SOURCE_DIR})
	endif()
endif()

FetchContent_Declare(
	glm
	GIT_REPOSITORY	https://github.com/g-truc/glm.git
	GIT_TAG 	8d1fd52e5ab5590e2c81768ace50c72bae28f2ed #refs/tags/1.0.3
	GIT_PROGRESS TRUE
	GIT_SHALLOW 1
)

FetchContent_MakeAvailable(glm)

add_subdirectory(MD5)

add_subdirectory(mongoose)

# libcurl, used by the dashboard to send email over SMTP. Built statically with only SMTP (and HTTP, for
# future outgoing HTTPS requests) enabled. TLS uses the OS certificate store: Schannel on Windows, OpenSSL elsewhere.
# Options are set inside a function so they don't leak into our own build (e.g. BUILD_TESTING from CTest).
function(dlu_fetch_curl)
	set(BUILD_SHARED_LIBS OFF)
	set(BUILD_TESTING OFF)
	set(BUILD_CURL_EXE OFF)
	set(BUILD_LIBCURL_DOCS OFF)
	set(BUILD_MISC_DOCS OFF)
	set(ENABLE_CURL_MANUAL OFF)
	set(BUILD_EXAMPLES OFF)
	set(CURL_DISABLE_INSTALL ON)
	set(CURL_ENABLE_EXPORT_TARGET OFF)

	foreach(protocol DICT FILE FTP GOPHER IMAP LDAP LDAPS MQTT POP3 RTSP SMB TELNET TFTP WEBSOCKETS IPFS)
		set(CURL_DISABLE_${protocol} ON)
	endforeach()

	set(CURL_USE_LIBPSL OFF)
	set(CURL_USE_LIBSSH2 OFF)
	set(CURL_USE_LIBSSH OFF)
	set(CURL_USE_GSSAPI OFF)
	set(CURL_ZLIB OFF)
	set(CURL_BROTLI OFF)
	set(CURL_ZSTD OFF)
	set(USE_NGHTTP2 OFF)
	set(USE_LIBIDN2 OFF)
	set(USE_WIN32_IDN OFF)
	set(ENABLE_ARES OFF)

	if(WIN32)
		set(CURL_USE_SCHANNEL ON)
		set(CURL_USE_OPENSSL OFF)
	else()
		set(CURL_USE_OPENSSL ON)
	endif()

	FetchContent_Declare(
		curl
		GIT_REPOSITORY https://github.com/curl/curl.git
		GIT_TAG curl-8_22_0
		GIT_PROGRESS TRUE
		GIT_SHALLOW 1
	)
	FetchContent_MakeAvailable(curl)
endfunction()
dlu_fetch_curl()

# Embree 4 (Apache-2.0), the ray queries of the UGC server's hidden face removal and ambient occlusion. A static
# library with its own task scheduler (no TBB), triangles only, single rays only, no ISPC, tutorials or tests.
# Options are set inside a function so they don't leak into our own build.
function(dlu_fetch_embree)
	# Embree asks for CMake 3.5's behaviour: let the variables below win over its option() and set(CACHE) defaults
	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)
	set(BUILD_TESTING OFF)
	set(EMBREE_STATIC_LIB ON)
	set(EMBREE_TASKING_SYSTEM "INTERNAL")
	set(EMBREE_ISPC_SUPPORT OFF)
	set(EMBREE_TUTORIALS OFF)
	set(EMBREE_ZIP_MODE OFF)
	set(EMBREE_INSTALL_DEPENDENCIES OFF)
	set(EMBREE_SYCL_SUPPORT OFF)
	set(EMBREE_FILTER_FUNCTION ON)
	set(EMBREE_RAY_MASK OFF)
	set(EMBREE_RAY_PACKETS OFF)
	set(EMBREE_BACKFACE_CULLING OFF)
	set(EMBREE_GEOMETRY_TRIANGLE ON)
	foreach(geometry QUAD CURVE SUBDIVISION USER INSTANCE INSTANCE_ARRAY GRID POINT)
		set(EMBREE_GEOMETRY_${geometry} OFF)
	endforeach()
	# Kernels for SSE2 (any x86-64), AVX and AVX2 (picked at run time by what the CPU has; AVX2 needs AVX's 8 wide
	# hierarchy). SSE4.2 and AVX-512 would only add build time. ARM builds use NEON whatever these say.
	set(EMBREE_MAX_ISA "NONE")
	set(EMBREE_ISA_SSE2 ON)
	set(EMBREE_ISA_SSE42 OFF)
	set(EMBREE_ISA_AVX ON)
	set(EMBREE_ISA_AVX2 ON)
	set(EMBREE_ISA_AVX512 OFF)

	FetchContent_Declare(
		embree
		GIT_REPOSITORY https://github.com/RenderKit/embree.git
		GIT_TAG ff9381774dc99fea81a932ad276677aad6a3d4dd #refs/tags/v4.4.0
		GIT_PROGRESS TRUE
		GIT_SHALLOW 1
	)
	FetchContent_MakeAvailable(embree)

	# Embree's Debug build is optimized but turns its assertions on, which slows it down a lot: off in every build
	foreach(target embree embree_avx embree_avx2 sys math simd tasking lexers)
		if(TARGET ${target})
			target_compile_options(${target} PRIVATE "$<$<CONFIG:Debug>:$<IF:$<CXX_COMPILER_ID:MSVC>,/UDEBUG,-UDEBUG>>"
				"$<$<CONFIG:Debug>:$<IF:$<CXX_COMPILER_ID:MSVC>,/UTBB_USE_DEBUG,-UTBB_USE_DEBUG>>")
		endif()
	endforeach()
endfunction()
dlu_fetch_embree()

# Intel Open Image Denoise 2 (Apache-2.0), the UGC server's denoise=oidn: optional, off by default (it brings TBB and
# large neural network weights). An installed one is used when CMake finds it, else Intel's release package (prebuilt,
# with its TBB) is downloaded and its libraries are copied next to the servers.
option(DLU_OIDN "Build the UGC server with Intel Open Image Denoise (the denoise=oidn setting)" OFF)
if(DLU_OIDN)
	# Its imported targets are used by the UGC server, in another directory
	set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL ON)
	find_package(OpenImageDenoise 2 CONFIG QUIET)
	if(NOT OpenImageDenoise_FOUND)
		if(WIN32)
			set(DLU_OIDN_URL "https://github.com/RenderKit/oidn/releases/download/v2.4.1/oidn-2.4.1.x64.windows.zip")
			set(DLU_OIDN_SHA256 "682d94ba57525ed177d73412e0ed903f576867bd048f830a5c6f63c56b25e8b8")
			set(DLU_OIDN_LIBRARIES "bin/*.dll")
		elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
			set(DLU_OIDN_URL "https://github.com/RenderKit/oidn/releases/download/v2.4.1/oidn-2.4.1.x86_64.linux.tar.gz")
			set(DLU_OIDN_SHA256 "b69ca2443a226ef692ca46bdc4f89995b1e99091f2665b906cbe07e9673e48cc")
			set(DLU_OIDN_LIBRARIES "lib/*.so*")
		else()
			message(FATAL_ERROR "DLU_OIDN: install Open Image Denoise 2 (no release package for this platform)")
		endif()
		FetchContent_Declare(oidn URL ${DLU_OIDN_URL} URL_HASH SHA256=${DLU_OIDN_SHA256} DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
		FetchContent_MakeAvailable(oidn)
		find_package(OpenImageDenoise 2 CONFIG REQUIRED PATHS "${oidn_SOURCE_DIR}" NO_DEFAULT_PATH)
		file(GLOB DLU_OIDN_FILES "${oidn_SOURCE_DIR}/${DLU_OIDN_LIBRARIES}")
		file(COPY ${DLU_OIDN_FILES} DESTINATION "${CMAKE_BINARY_DIR}")
	endif()
	message(STATUS "Open Image Denoise ${OpenImageDenoise_VERSION}: the UGC server can denoise icons")
endif()
