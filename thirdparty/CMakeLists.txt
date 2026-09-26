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
