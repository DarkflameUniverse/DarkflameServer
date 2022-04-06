#include "dChatFilter.h"
#include "BinaryIO.h"
#include <fstream>
#include <string>
#include <functional>
#include <algorithm>
#include <sstream>
#include <regex>

#include "dCommonVars.h"
#include "Database.h"
#include "dLogger.h"
#include "Game.h"

using namespace dChatFilterDCF;

dChatFilter::dChatFilter(const std::string& filepath, bool dontGenerateDCF) {
	m_DontGenerateDCF = dontGenerateDCF;

	if (!BinaryIO::DoesFileExist(filepath + ".dcf") || m_DontGenerateDCF) {
		ReadWordlistPlaintext(filepath + ".txt");
		if (!m_DontGenerateDCF) ExportWordlistToDCF(filepath + ".dcf");
	}
	else if (!ReadWordlistDCF(filepath + ".dcf")) {
		ReadWordlistPlaintext(filepath + ".txt");
		ExportWordlistToDCF(filepath + ".dcf");
	}

	//Read player names that are ok as well:
	auto stmt = Database::CreatePreppedStmt("select name from charinfo;");
	auto res = stmt->executeQuery();
	while (res->next()) {
		std::string line = res->getString(1).c_str();
		std::transform(line.begin(), line.end(), line.begin(), ::tolower); //Transform to lowercase
		m_Words.push_back(CalculateHash(line));
	}
	delete res;
	delete stmt;
}

dChatFilter::~dChatFilter() {
	m_Words.clear();
}

void dChatFilter::ReadWordlistPlaintext(const std::string& filepath) {
	std::ifstream file(filepath);
	if (file) {
		std::string line;
		while (std::getline(file, line)) {
			line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
			std::transform(line.begin(), line.end(), line.begin(), ::tolower); //Transform to lowercase
			m_Words.push_back(CalculateHash(line));
		}
	}
}

bool dChatFilter::ReadWordlistDCF(const std::string& filepath) {
	std::ifstream file(filepath, std::ios::binary);
	if (file) {
		fileHeader hdr;
		BinaryIO::BinaryRead(file, hdr);
		if (hdr.header != header) {
			file.close();
			return false;
		}

		if (hdr.formatVersion == formatVersion) {
			size_t wordsToRead = 0;
			BinaryIO::BinaryRead(file, wordsToRead);
			m_Words.reserve(wordsToRead);

			size_t word = 0;
			for (size_t i = 0; i < wordsToRead; ++i) {
				BinaryIO::BinaryRead(file, word);
				m_Words.push_back(word);
			}

			return true;
		}
		else {
			file.close();
			return false;
		}
	}

	return false;
}

void dChatFilter::ExportWordlistToDCF(const std::string& filepath) {
	std::ofstream file(filepath, std::ios::binary | std::ios_base::out);
	if (file) {
		BinaryIO::BinaryWrite(file, uint32_t(dChatFilterDCF::header));
		BinaryIO::BinaryWrite(file, uint32_t(dChatFilterDCF::formatVersion));
		BinaryIO::BinaryWrite(file, size_t(m_Words.size()));

		for (size_t word : m_Words) {
			BinaryIO::BinaryWrite(file, word);
		}

		file.close();
	}
}

bool dChatFilter::IsSentenceOkay(const std::string& message, int gmLevel) {
	//Commenting out chat filter check
	/**
	if (gmLevel > GAME_MASTER_LEVEL_FORUM_MODERATOR) return true; //If anything but a forum mod, return true.
	if (message.empty()) return true;

	std::stringstream sMessage(message);
	std::string segment;
	std::regex reg("(!*|\\?*|\\;*|\\.*|\\,*)");

	while (std::getline(sMessage, segment, ' ')) {
		std::transform(segment.begin(), segment.end(), segment.begin(), ::tolower); //Transform to lowercase
		segment = std::regex_replace(segment, reg, "");

		size_t hash = CalculateHash(segment);

		if (std::find(m_UserUnapprovedWordCache.begin(), m_UserUnapprovedWordCache.end(), hash) != m_UserUnapprovedWordCache.end()) {
			return false;
		}

		if (!IsInWordlist(hash)) {
			m_UserUnapprovedWordCache.push_back(hash);
			return false;
		}
	}
	**/
	return true;
}

size_t dChatFilter::CalculateHash(const std::string& word) {
	std::hash<std::string> hash{};

	size_t value = hash(word);

	return value;
}

bool dChatFilter::IsInWordlist(size_t word) {
	return std::find(m_Words.begin(), m_Words.end(), word) != m_Words.end();
}
