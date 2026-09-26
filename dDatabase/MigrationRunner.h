#pragma once

#include <string>
#include <string_view>
#include <vector>

struct Migration {
	std::string data;
	std::string name;
};

namespace MigrationRunner {
	void RunMigrations();
	void RunSQLiteMigrations();

	/**
	 * Split a migration file into statements at the semicolons that end them. Semicolons inside comments
	 * (-- and /* *\/) and inside quoted strings or identifiers ('...', "...", `...`) are not statement ends.
	 * Comments are left out and statements that are only whitespace are skipped.
	 * @param backslashEscapes MySQL treats a backslash in a string as an escape; SQLite does not (Windows paths)
	 */
	inline std::vector<std::string> SplitStatements(std::string_view sql, bool backslashEscapes) {
		std::vector<std::string> statements;
		std::string current;
		const auto flush = [&]() {
			if (current.find_first_not_of(" \t\r\n") != std::string::npos) statements.push_back(current);
			current.clear();
		};
		for (size_t i = 0; i < sql.size(); i++) {
			const char c = sql[i];
			const char next = i + 1 < sql.size() ? sql[i + 1] : '\0';
			if (c == '-' && next == '-') {
				while (i < sql.size() && sql[i] != '\n') i++;
				current += '\n';
			} else if (c == '/' && next == '*') {
				const auto end = sql.find("*/", i + 2);
				i = end == std::string_view::npos ? sql.size() : end + 1;
				current += ' ';
			} else if (c == '\'' || c == '"' || c == '`') {
				// Copy the quoted text as is; a doubled quote (or a backslash, for MySQL) escapes one inside it
				current += c;
				for (i++; i < sql.size(); i++) {
					current += sql[i];
					if (backslashEscapes && sql[i] == '\\' && c != '`' && i + 1 < sql.size()) {
						current += sql[++i];
					} else if (sql[i] == c) {
						if (i + 1 < sql.size() && sql[i + 1] == c) current += sql[++i];
						else break;
					}
				}
			} else if (c == ';') {
				flush();
			} else {
				current += c;
			}
		}
		flush();
		return statements;
	}
};
