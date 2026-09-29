# Writes BuildInfo.cpp (the data behind dCommon/BuildInfo.h) at build time.
# Run as a script:
#   cmake -DSOURCE_DIR=... -DTEMPLATE=... -DOUTPUT=... -DVERSION_MAJOR=... -DVERSION_MINOR=... -DVERSION_PATCH=... -P BuildInfo.cmake
# The output is only rewritten when its contents change, so an unchanged tree recompiles nothing.
#
# Build kind comes from the environment of the build: DLU_BUILD_KIND=release|ci|local wins, otherwise CI=true
# (set by GitHub Actions and most CI systems) means ci, otherwise local.

find_package(Git QUIET)

set(BUILD_COMMIT "")
set(BUILD_BRANCH "")
set(BUILD_DIRTY "false")

if(GIT_FOUND)
	execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse HEAD
		OUTPUT_VARIABLE BUILD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE result)
	if(NOT result EQUAL 0)
		set(BUILD_COMMIT "")
	endif()
endif()

if(BUILD_COMMIT)
	execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse --abbrev-ref HEAD
		OUTPUT_VARIABLE BUILD_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
	if(BUILD_BRANCH STREQUAL "HEAD")
		# Detached HEAD (CI checkouts): use the branch the CI names, if any.
		if(NOT "$ENV{GITHUB_HEAD_REF}" STREQUAL "")
			set(BUILD_BRANCH "$ENV{GITHUB_HEAD_REF}")
		else()
			set(BUILD_BRANCH "$ENV{GITHUB_REF_NAME}")
		endif()
	endif()

	execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" update-index -q --refresh OUTPUT_QUIET ERROR_QUIET)
	execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" diff-index --quiet HEAD --
		RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
	if(NOT result EQUAL 0)
		set(BUILD_DIRTY "true")
	endif()
endif()

string(TOLOWER "$ENV{DLU_BUILD_KIND}" kind)
string(TOLOWER "$ENV{CI}" ci)
if(kind STREQUAL "release")
	set(BUILD_KIND "RELEASE")
elseif(kind STREQUAL "ci" OR (kind STREQUAL "" AND (ci STREQUAL "true" OR ci STREQUAL "1")))
	set(BUILD_KIND "CI")
else()
	set(BUILD_KIND "LOCAL")
endif()

# Readable identifier, e.g. 3.0.0-experimental+g1a2b3c4d-dirty. Release builds leave out the branch.
set(BUILD_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
if(BUILD_BRANCH AND NOT BUILD_KIND STREQUAL "RELEASE")
	string(REGEX REPLACE ".*/" "" branchName "${BUILD_BRANCH}")
	string(REGEX REPLACE "[^0-9A-Za-z.-]" "-" branchName "${branchName}")
	string(APPEND BUILD_STRING "-${branchName}")
endif()
if(BUILD_COMMIT)
	string(SUBSTRING "${BUILD_COMMIT}" 0 8 shortCommit)
	string(APPEND BUILD_STRING "+g${shortCommit}")
	if(BUILD_DIRTY)
		string(APPEND BUILD_STRING "-dirty")
	endif()
endif()

# Git allows " in branch names; keep the C++ string literal valid.
string(REPLACE "\"" "\\\"" BUILD_BRANCH "${BUILD_BRANCH}")

configure_file("${TEMPLATE}" "${OUTPUT}" @ONLY)
