# Runs the MySQL and SQLite GameDatabase implementations side by side on fresh databases (all migrations run the way the
# servers run them) and compares what every method returns. Needs a MariaDB/MySQL server; skipped when
# DLU_TEST_MYSQL_HOST is not set. See DatabaseParityTests.cpp for the variables.
add_executable(dDatabaseTests "DatabaseParityTests.cpp")

target_include_directories(dDatabaseTests PRIVATE
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase/MySQL"
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase/SQLite"
	"${PROJECT_SOURCE_DIR}/dNet"
	"${PROJECT_SOURCE_DIR}/thirdparty/nlohmann"
	"${PROJECT_SOURCE_DIR}/thirdparty/SQLite"
)

target_link_libraries(dDatabaseTests ${COMMON_LIBRARIES} MariaDB::ConnCpp sqlite3 GTest::gtest_main)

gtest_discover_tests(dDatabaseTests)

# The chat history and chat flag tables on SQLite alone (no MariaDB/MySQL needed), so they always run.
add_executable(dDatabaseSqliteTests "ChatHistorySqliteTests.cpp")

target_include_directories(dDatabaseSqliteTests PRIVATE
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase/MySQL"
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase/SQLite"
	"${PROJECT_SOURCE_DIR}/dNet"
	"${PROJECT_SOURCE_DIR}/thirdparty/nlohmann"
	"${PROJECT_SOURCE_DIR}/thirdparty/SQLite"
)

# dCommon uses raknet without linking it: link them as a group so the order of the static libraries does not matter
target_link_libraries(dDatabaseSqliteTests glm::glm "dNet" "magic_enum" "$<LINK_GROUP:RESCAN,dDatabase,dDatabaseGame,dDatabaseCDClient,dCommon,raknet>" MariaDB::ConnCpp sqlite3 GTest::gtest_main ${CMAKE_DL_LIBS})

gtest_discover_tests(dDatabaseSqliteTests)
