#include "dConfig.h"

#include <sstream>
#include <algorithm>

#include "BinaryPathFinder.h"
#include "GeneralUtils.h"

dConfig::dConfig(const std::string& filepath) {
	m_ConfigFilePath = filepath;
	LoadConfig();
}

std::filesystem::path GetConfigDir() {
	std::filesystem::path config_dir = BinaryPathFinder::GetBinaryDir();
	if (const char* env_p = std::getenv("DLU_CONFIG_DIR")) {
		config_dir /= env_p;
	}
	return config_dir;
}

const bool dConfig::Exists(const std::string& filepath) {
	std::filesystem::path config_dir = GetConfigDir();
	return std::filesystem::exists(config_dir / filepath);
}

void dConfig::LoadFile(const std::string& file) {
	std::ifstream in(GetConfigDir() / file);
	if (!in.good()) return;

	// Comment lines directly above a key describe it (the dashboard shows them)
	std::string line{}, description{};
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty()) {
			description.clear();
		} else if (line.front() == '#') {
			auto text = line.substr(1);
			if (!text.empty() && text.front() == ' ') text.erase(0, 1);
			description += (description.empty() ? "" : " ") + text;
		} else {
			ProcessLine(line, file, description);
			description.clear();
		}
	}
}

void dConfig::LoadConfig() {
	LoadFile(m_ConfigFilePath);
	LoadFile("sharedconfig.ini");
}

void dConfig::ReloadConfig() {
	this->m_ConfigValues.clear();
	this->m_FileEntries.clear();
	this->m_FileOrder.clear();
	this->m_EnvValues.clear();
	LoadConfig();
	if (m_DatabaseSync) m_DatabaseSync(*this);
	for (const auto& handler : m_ConfigHandlers) handler();
	LogSettings();
}

const std::string& dConfig::GetValue(std::string key) {
	if (const auto it = m_DatabaseOverrides.find(key); it != m_DatabaseOverrides.end()) return it->second;
	if (const auto env = GetEnvironmentValue(key)) return m_EnvValues[key] = *env;
	if (const auto it = m_ConfigValues.find(key); it != m_ConfigValues.end()) return it->second;
	if (const auto it = m_DatabaseFallbacks.find(key); it != m_DatabaseFallbacks.end()) return it->second;
	return this->m_ConfigValues[key];
}

std::optional<std::string> dConfig::GetEnvironmentValue(const std::string& key) {
	std::string upper_key(key);
	std::transform(upper_key.begin(), upper_key.end(), upper_key.begin(), ::toupper);
	if (const char* env_p = std::getenv(upper_key.c_str())) return std::string(env_p);
	return std::nullopt;
}

bool dConfig::IsFileOnlyKey(const std::string& key) {
	// Programs the servers run and folders they read or serve files from: set on the dashboard, these would give the
	// settings permission shell or file access on the machine
	return key.starts_with("mysql_") || key.starts_with("sqlite_") || key == "database_type" ||
		key == "jwt_secret" || key == "totp_key" ||
		key == "backup_mysqldump" || key == "backup_folder" || key == "client_location" || key == "dump_folder";
}

bool dConfig::IsSecretKey(const std::string& key) {
	return key.find("password") != std::string::npos || key.find("secret") != std::string::npos ||
		key.find("token") != std::string::npos || key.ends_with("_key");
}

void dConfig::AddConfigHandler(std::function<void()> handler) {
	m_ConfigHandlers.push_back(handler);
}

void dConfig::LogSettings() const {
	LOG("Configuration settings:");
	for (const auto& [key, value] : m_ConfigValues) {
		const auto override = m_DatabaseOverrides.find(key);
		const auto& shown = override != m_DatabaseOverrides.end() ? override->second : value;
		const auto& valueLog = IsSecretKey(key) ? "<HIDDEN>" : shown;
		LOG("  %s = %s%s", key.c_str(), valueLog.c_str(), override != m_DatabaseOverrides.end() ? " (from the dashboard)" : "");
	}
	for (const auto& [key, value] : m_DatabaseFallbacks) {
		if (m_ConfigValues.contains(key)) continue;
		LOG("  %s = %s (from the dashboard)", key.c_str(), IsSecretKey(key) ? "<HIDDEN>" : value.c_str());
	}
}

std::vector<dConfig::FileEntry> dConfig::GetFileEntries() const {
	std::vector<FileEntry> entries;
	for (const auto& key : m_FileOrder) entries.push_back(m_FileEntries.at(key));
	return entries;
}

void dConfig::SetDatabaseValues(std::map<std::string, std::string> overrides, std::map<std::string, std::string> fallbacks) {
	std::erase_if(overrides, [](const auto& entry) { return IsFileOnlyKey(entry.first); });
	std::erase_if(fallbacks, [](const auto& entry) { return IsFileOnlyKey(entry.first); });
	m_DatabaseOverrides = std::move(overrides);
	m_DatabaseFallbacks = std::move(fallbacks);
}

void dConfig::SetDatabaseSync(std::function<void(dConfig&)> sync) {
	m_DatabaseSync = std::move(sync);
	if (m_DatabaseSync) m_DatabaseSync(*this);
}

void dConfig::ProcessLine(const std::string& line, const std::string& file, const std::string& description) {
	auto splitLoc = line.find('=');
	if (splitLoc == std::string::npos) return;
	// Stray spaces around keys and values (for example "client_location = ../client ") are a
	// common setup mistake that otherwise surfaces as an unrelated error much later, so drop them.
	// This also removes the \r left at the end of lines of a file saved with Windows line endings.
	const auto trim = [](std::string_view str) {
		constexpr std::string_view whitespace = " \t\r\n";
		const auto start = str.find_first_not_of(whitespace);
		if (start == std::string_view::npos) return std::string{};
		const auto end = str.find_last_not_of(whitespace);
		return std::string{ str.substr(start, end - start + 1) };
	};
	const auto key = trim(std::string_view(line).substr(0, splitLoc));
	const auto value = trim(std::string_view(line).substr(splitLoc + 1));
	if (key.empty()) return;

	if (this->m_ConfigValues.find(key) != this->m_ConfigValues.end()) return;

	this->m_ConfigValues.insert(std::make_pair(key, value));
	m_FileEntries[key] = { key, value, file, description };
	m_FileOrder.push_back(key);
}

std::string dConfig::GetValue(const std::string& key, const char* emptyValue) {
	return GetValue(key, std::string(emptyValue));
};
