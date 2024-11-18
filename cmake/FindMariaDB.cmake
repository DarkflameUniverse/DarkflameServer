# mariadb connector cpp
# On Windows ClangCL can't compile the connector from source but can link to an msvc compiled one,
# so prefer the prebuilt binaries unless MARIADB_BUILD_SOURCE is specified
if(WIN32 AND NOT MARIADB_BUILD_SOURCE)
	set(MARIADB_MSI_DIR "${PROJECT_BINARY_DIR}/msi")
	set(MARIADB_CONNECTOR_DIR "${PROJECT_BINARY_DIR}/mariadbcpp")
	set(MARIADB_C_CONNECTOR_DIR "${MARIADB_CONNECTOR_DIR}/MariaDB/MariaDB Connector C 64-bit")
	set(MARIADB_CPP_CONNECTOR_DIR "${MARIADB_CONNECTOR_DIR}/MariaDB/MariaDB C++ Connector 64-bit")

	file(MAKE_DIRECTORY "${MARIADB_MSI_DIR}")
	file(MAKE_DIRECTORY "${MARIADB_CONNECTOR_DIR}")

    # These values need to be updated whenever a new minor release replaces an old one
	# Go to https://mariadb.com/downloads/connectors/ to find the up-to-date URL parts
    set(MARIADB_CONNECTOR_C_VERSION "3.2.7")
	set(MARIADB_CONNECTOR_C_BUCKET "2319651")
	set(MARIADB_CONNECTOR_C_MD5 "f8636d733f1d093af9d4f22f3239f885")
    set(MARIADB_CONNECTOR_CPP_VERSION "1.0.2")
	set(MARIADB_CONNECTOR_CPP_BUCKET "2531525")
	set(MARIADB_CONNECTOR_CPP_MD5 "3034bbd6ca00a0125345f9fd1a178401")
	
	set(MARIADB_CONNECTOR_C_MSI "mariadb-connector-c-${MARIADB_CONNECTOR_C_VERSION}-win64.msi")
	set(MARIADB_CONNECTOR_CPP_MSI "mariadb-connector-cpp-${MARIADB_CONNECTOR_CPP_VERSION}-win64.msi")
	
	if(NOT EXISTS "${MARIADB_MSI_DIR}/${MARIADB_CONNECTOR_C_MSI}" )
		message(STATUS "Downloading mariadb connector/c")
		file(DOWNLOAD https://dlm.mariadb.com/${MARIADB_CONNECTOR_C_BUCKET}/Connectors/c/connector-c-${MARIADB_CONNECTOR_C_VERSION}/${MARIADB_CONNECTOR_C_MSI}
			"${MARIADB_MSI_DIR}/${MARIADB_CONNECTOR_C_MSI}"
			EXPECTED_HASH MD5=${MARIADB_CONNECTOR_C_MD5})
	endif()

	if(NOT EXISTS "${MARIADB_MSI_DIR}/${MARIADB_CONNECTOR_CPP_MSI}" )
		message(STATUS "Downloading mariadb connector/c++")
		file(DOWNLOAD https://dlm.mariadb.com/${MARIADB_CONNECTOR_CPP_BUCKET}/Connectors/cpp/connector-cpp-${MARIADB_CONNECTOR_CPP_VERSION}/${MARIADB_CONNECTOR_CPP_MSI}
			"${MARIADB_MSI_DIR}/${MARIADB_CONNECTOR_CPP_MSI}"
			EXPECTED_HASH MD5=${MARIADB_CONNECTOR_CPP_MD5})
	endif()


	file(TO_NATIVE_PATH "${MARIADB_CONNECTOR_DIR}" MSIEXEC_TARGETDIR)
	# extract msi files without installing to users system
	if(NOT EXISTS "${MARIADB_C_CONNECTOR_DIR}")
		file(TO_NATIVE_PATH "${MARIADB_MSI_DIR}/${MARIADB_CONNECTOR_C_MSI}" MSI_DIR)
		execute_process(COMMAND msiexec /a ${MSI_DIR} /qn TARGETDIR=${MSIEXEC_TARGETDIR})
	endif()
	set(MARIADBC_SHARED_LIBRARY_LOCATION "${MARIADB_C_CONNECTOR_DIR}/lib/libmariadb.dll")

	if(NOT EXISTS "${MARIADB_CPP_CONNECTOR_DIR}")
	file(TO_NATIVE_PATH "${MARIADB_MSI_DIR}/${MARIADB_CONNECTOR_CPP_MSI}" MSI_DIR)
		execute_process(COMMAND msiexec /a ${MSI_DIR} /qn TARGETDIR=${MSIEXEC_TARGETDIR})
	endif()

	set(MARIADBCPP_SHARED_LIBRARY_LOCATION "${MARIADB_CPP_CONNECTOR_DIR}/mariadbcpp.dll")
	set(MARIADB_IMPLIB_LOCATION "${MARIADB_CPP_CONNECTOR_DIR}/mariadbcpp.lib")
	set(MARIADB_INCLUDE_DIR "${MARIADB_CPP_CONNECTOR_DIR}/include/mariadb")

	add_custom_target(mariadb_connector_cpp)
	add_custom_command(TARGET mariadb_connector_cpp POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy_if_different
					"${MARIADBCPP_SHARED_LIBRARY_LOCATION}"
					"${MARIADBC_SHARED_LIBRARY_LOCATION}"
					"${PROJECT_BINARY_DIR}")

	# MariaDB uses plugins that the database needs to load, the prebuilt binaries by default will try to find the libraries in system directories,
	# so set this define and the servers will set the MARIADB_PLUGIN_DIR environment variable to the appropriate directory.
	# Plugin directory is determined at dll load time (this will happen before main()) so we need to delay the dll load so that we can set the environment variable
	add_link_options(/DELAYLOAD:${MARIADBCPP_SHARED_LIBRARY_LOCATION})
	add_compile_definitions(MARIADB_PLUGIN_DIR_OVERRIDE="${MARIADB_CPP_CONNECTOR_DIR}/plugin")
else() # Build from source

	include(ExternalProject)
	if(WIN32)
		set(MARIADB_EXTRA_CMAKE_ARGS
			-DCMAKE_C_FLAGS=/w # disable zlib warnings
			-DCMAKE_CXX_FLAGS=/EHsc
			-DWITH_MSI=OFF)
	elseif(APPLE)
		set(MARIADB_EXTRA_CMAKE_ARGS
			-DWITH_EXTERNAL_ZLIB=ON
			-DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}
			-DCMAKE_C_FLAGS=-w # disable zlib warnings
			-DCMAKE_CXX_FLAGS=-D_GLIBCXX_USE_CXX11_ABI=0)
	else()
		set(MARIADB_EXTRA_CMAKE_ARGS
			-DCMAKE_C_FLAGS=-w # disable zlib warnings
			-DCMAKE_CXX_FLAGS=-D_GLIBCXX_USE_CXX11_ABI=0)
	endif()

	set(MARIADBCPP_INSTALL_DIR ${PROJECT_BINARY_DIR}/prefix)
	set(MARIADBCPP_LIBRARY_DIR ${PROJECT_BINARY_DIR}/mariadbcpp)
	set(MARIADBCPP_PLUGIN_DIR ${MARIADBCPP_LIBRARY_DIR}/plugin)
	set(MARIADBCPP_SOURCE_DIR ${PROJECT_SOURCE_DIR}/thirdparty/mariadb-connector-cpp)
	set(MARIADB_INCLUDE_DIR "${MARIADBCPP_SOURCE_DIR}/include")
	ExternalProject_Add(mariadb_connector_cpp
		PREFIX "${PROJECT_BINARY_DIR}/thirdparty/mariadb-connector-cpp"
		SOURCE_DIR ${MARIADBCPP_SOURCE_DIR}
		INSTALL_DIR ${MARIADBCPP_INSTALL_DIR}
		CMAKE_ARGS  -Wno-dev
					-DWITH_UNIT_TESTS=OFF
					-DMARIADB_LINK_DYNAMIC=OFF
					-DCMAKE_BUILD_RPATH_USE_ORIGIN=${CMAKE_BUILD_RPATH_USE_ORIGIN}
					-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
					-DINSTALL_LIBDIR=${MARIADBCPP_LIBRARY_DIR}
					-DINSTALL_PLUGINDIR=${MARIADBCPP_PLUGIN_DIR}
					${MARIADB_EXTRA_CMAKE_ARGS}
		BUILD_ALWAYS true
	)

	if(WIN32)
		set(MARIADB_SHARED_LIBRARY_NAME mariadbcpp.dll)
		set(MARIADB_PLUGIN_SUFFIX .dll)
		set(MARIADB_IMPLIB_LOCATION "${MARIADBCPP_LIBRARY_DIR}/mariadbcpp.lib")

		# When built from source windows only seems to check same folder as exe instead specified folder, so use
		# environment variable to force it
		add_link_options(/DELAYLOAD:mariadbcpp.dll)
		add_compile_definitions(MARIADB_PLUGIN_DIR_OVERRIDE="${MARIADBCPP_PLUGIN_DIR}")
	else()
		set(MARIADB_SHARED_LIBRARY_NAME libmariadbcpp${CMAKE_SHARED_LIBRARY_SUFFIX})
		set(MARIADB_PLUGIN_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
	endif()

	set(MARIADBCPP_SHARED_LIBRARY_LOCATION "${MARIADBCPP_LIBRARY_DIR}/${MARIADB_SHARED_LIBRARY_NAME}")
	if(WIN32)
		set(MARIADBC_SHARED_LIBRARY_LOCATION "${MARIADBCPP_LIBRARY_DIR}/libmariadb.lib")
	endif()
endif()

# Create mariadb connector library object
add_library(MariaDB::ConnCpp SHARED IMPORTED GLOBAL)
add_dependencies(MariaDB::ConnCpp mariadb_connector_cpp)
set_target_properties(MariaDB::ConnCpp PROPERTIES
	IMPORTED_LOCATION "${MARIADBCPP_SHARED_LIBRARY_LOCATION}")

if(WIN32)
	set_target_properties(MariaDB::ConnCpp PROPERTIES
		IMPORTED_IMPLIB "${MARIADB_IMPLIB_LOCATION}")
elseif(APPLE)
	set_target_properties(MariaDB::ConnCpp PROPERTIES
		IMPORTED_SONAME "libmariadbcpp")
endif()

# Add directories to include lists
target_include_directories(MariaDB::ConnCpp SYSTEM INTERFACE ${MARIADB_INCLUDE_DIR})

set(MariaDB_FOUND TRUE)
