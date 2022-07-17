#include "dChatFilter.h"
#include "BinaryIO.h"
#include <fstream>
#include <string>
#include <functional>
#include <algorithm>
#include <sstream>
#include <regex>

#include "dCommonVars.h"
#include "dLogger.h"
#include "dConfig.h"
#include "Database.h"
#include "Game.h"

using namespace dChatFilterDCF;

dChatFilter::dChatFilter(const std::string& filepath, bool dontGenerateDCF) {
	m_DontGenerateDCF = dontGenerateDCF;

	if (!BinaryIO::DoesFileExist(filepath + ".dcf") || m_DontGenerateDCF) {
		ReadWordlistPlaintext(filepath + ".txt");
		if (!m_DontGenerateDCF) ExportWordlistToDCF(filepath + ".dcf");
	}
	else if (!ReadWordlistDCF(filepath + ".dcf", true)) {
		ReadWordlistPlaintext(filepath + ".txt");
		ExportWordlistToDCF(filepath + ".dcf");
	}

	if (BinaryIO::DoesFileExist("blacklist.dcf")) {
		ReadWordlistDCF("blacklist.dcf", false);
	}

	//Read player names that are ok as well:
	auto stmt = Database::CreatePreppedStmt("select name from charinfo;");
	auto res = stmt->executeQuery();
	while (res->next()) {
		std::string line = res->getString(1).c_str();
		std::transform(line.begin(), line.end(), line.begin(), ::tolower); //Transform to lowercase
		m_YesYesWords.push_back(CalculateHash(line));
	}
	delete res;
	delete stmt;
}

dChatFilter::~dChatFilter() {
	m_YesYesWords.clear();
	m_NoNoWords.clear();
}

void dChatFilter::ReadWordlistPlaintext(const std::string& filepath) {
	std::ifstream file(filepath);
	if (file) {
		std::string line;
		while (std::getline(file, line)) {
			line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
			std::transform(line.begin(), line.end(), line.begin(), ::tolower); //Transform to lowercase
			m_YesYesWords.push_back(CalculateHash(line));
		}
	}
}

bool dChatFilter::ReadWordlistDCF(const std::string& filepath, bool whiteList) {
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
			if (whiteList) m_YesYesWords.reserve(wordsToRead);
			else m_NoNoWords.reserve(wordsToRead);

			size_t word = 0;
			for (size_t i = 0; i < wordsToRead; ++i) {
				BinaryIO::BinaryRead(file, word);
				if (whiteList) m_YesYesWords.push_back(word);
				else m_NoNoWords.push_back(word);
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
		BinaryIO::BinaryWrite(file, size_t(m_YesYesWords.size()));

		for (size_t word : m_YesYesWords) {
			BinaryIO::BinaryWrite(file, word);
		}

		file.close();
	}
}

std::vector<std::string> dChatFilter::IsSentenceOkay(const std::string& message, int gmLevel, bool whiteList) {
	if (gmLevel > GAME_MASTER_LEVEL_FORUM_MODERATOR) return { }; //If anything but a forum mod, return true.
	if (message.empty()) return { };
	if (!whiteList && m_NoNoWords.empty()) return { "" };

	std::stringstream sMessage(message);
	std::string segment;
	std::regex reg("(!*|\\?*|\\;*|\\.*|\\,*)");

	std::vector<std::string> listOfBadSegments = std::vector<std::string>();

	while (std::getline(sMessage, segment, ' ')) {
		std::string originalSegment = segment;

		std::transform(segment.begin(), segment.end(), segment.begin(), ::tolower); //Transform to lowercase
		segment = std::regex_replace(segment, reg, "");

		size_t hash = CalculateHash(segment);

		if (std::find(m_UserUnapprovedWordCache.begin(), m_UserUnapprovedWordCache.end(), hash) != m_UserUnapprovedWordCache.end()) {
			listOfBadSegments.push_back(originalSegment); // found word that isn't ok, just deny this code works for both white and black list
		}

		if (!IsInWordlist(hash, whiteList)) {
			if (whiteList) {
				m_UserUnapprovedWordCache.push_back(hash);
				listOfBadSegments.push_back(originalSegment);
			}
		}
		else {
			if (!whiteList) {
				m_UserUnapprovedWordCache.push_back(hash);
				listOfBadSegments.push_back(originalSegment);
			}
		}
	}

	return listOfBadSegments;
}

size_t dChatFilter::CalculateHash(const std::string& word) {
	std::hash<std::string> hash{};

	size_t value = hash(word);

	return value;
}

bool dChatFilter::IsInWordlist(size_t word, bool whiteList) {
	auto* list = whiteList ? &m_YesYesWords : &m_NoNoWords;

	return std::find(list->begin(), list->end(), word) != list->end();
}