#include "CDClientDatabase.h"
#include "CDComponentsRegistryTable.h"
#include "Profiler.h"

#include <cctype>
#include <string_view>
#include <utility>
#include <vector>

// Static Variables
static CppSQLite3DB* conn = new CppSQLite3DB();

// Status Variables
bool CDClientDatabase::isConnected = false;

namespace {
	// Frame timing (Profiler.h): each CDClient statement the main thread runs, timed from its first step to its end,
	// counted under the scope that ran it as "CDClient <table>" (the first table after FROM)
	std::vector<std::pair<sqlite3_stmt*, int64_t>> g_Running;

	const char* TableScope(const char* sql) {
		if (!sql) return "CDClient";
		const std::string_view text(sql);
		for (size_t i = 0; i + 5 < text.size(); i++) {
			const bool from = (text[i] == 'F' || text[i] == 'f') && (text[i + 1] == 'R' || text[i + 1] == 'r') && (text[i + 2] == 'O' || text[i + 2] == 'o') &&
				(text[i + 3] == 'M' || text[i + 3] == 'm') && std::isspace(static_cast<unsigned char>(text[i + 4])) && (i == 0 || std::isspace(static_cast<unsigned char>(text[i - 1])));
			if (!from) continue;
			size_t start = i + 5;
			while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) start++;
			size_t end = start;
			while (end < text.size() && (std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_')) end++;
			if (end > start) return Profiler::Intern("CDClient " + std::string(text.substr(start, end - start)));
			break;
		}
		return "CDClient";
	}

	int Trace(unsigned type, void*, void* p, void* x) {
		if (!Profiler::IsMainThread()) return 0;
		auto* statement = static_cast<sqlite3_stmt*>(p);
		const auto now = Profiler::NowNs();
		if (type == SQLITE_TRACE_STMT) {
			for (auto& [running, start] : g_Running) {
				if (running == statement) { start = now; return 0; }
			}
			if (g_Running.size() < 64) g_Running.emplace_back(statement, now);
			return 0;
		}
		if (type != SQLITE_TRACE_PROFILE) return 0;
		// SQLite's own time as a fallback (coarse on some platforms)
		int64_t duration = x ? static_cast<int64_t>(*static_cast<sqlite3_int64*>(x)) : 0;
		for (size_t i = 0; i < g_Running.size(); i++) {
			if (g_Running[i].first != statement) continue;
			duration = now - g_Running[i].second;
			g_Running.erase(g_Running.begin() + static_cast<std::ptrdiff_t>(i));
			break;
		}
		Profiler::Local().Record(TableScope(sqlite3_sql(statement)), 0, duration, Profiler::Phase::CDCLIENT, now);
		return 0;
	}
}

//! Opens a connection with the CDClient
void CDClientDatabase::Connect(const std::string& filename) {
	conn->open(filename.c_str());
	isConnected = true;
	sqlite3_trace_v2(conn->handle(), SQLITE_TRACE_STMT | SQLITE_TRACE_PROFILE, Trace, nullptr);
}

void CDClientDatabase::Reconnect(const std::string& filename) {
	auto* next = new CppSQLite3DB();
	try {
		next->open(filename.c_str());
	} catch (...) {
		delete next;
		throw;
	}
	sqlite3_trace_v2(next->handle(), SQLITE_TRACE_STMT | SQLITE_TRACE_PROFILE, Trace, nullptr);
	g_Running.clear();
	auto* old = std::exchange(conn, next);
	isConnected = true;
	// close_v2 waits for statements still open on the old file (a query a caller holds) before it really closes. The
	// wrapper is left behind on purpose: its destructor would close the handle a second time
	sqlite3_close_v2(old->handle());
}

//! Queries the CDClient
CppSQLite3Query CDClientDatabase::ExecuteQuery(const std::string& query) {
	return conn->execQuery(query.c_str());
}

//! Updates the CDClient file with Data Manipulation Language (DML) commands.
int CDClientDatabase::ExecuteDML(const std::string& query) {
	return conn->execDML(query.c_str());
}

//! Makes prepared statements
CppSQLite3Statement CDClientDatabase::CreatePreppedStmt(const std::string& query) {
	return conn->compileStatement(query.c_str());
}
