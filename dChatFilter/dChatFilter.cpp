#include "dChatFilter.h"
#include "BinaryIO.h"
#include <fstream>
#include <string>
#include <functional>
#include <algorithm>
#include <sstream>
#include <regex>

#include "dCommonVars.h"
#include "Logger.h"
#include "dConfig.h"
#include "Database.h"
#include "Game.h"
#include "eGameMasterLevel.h"

using namespace dChatFilterDCF;

dChatFilter::dChatFilter(const std::string& filepath, bool dontGenerateDCF) {
	m_DontGenerateDCF = dontGenerateDCF;

	if (!BinaryIO::DoesFileExist(filepath + ".dcf") || m_DontGenerateDCF) {
		ReadWordlistPlaintext(filepath + ".txt", true);
		if (!m_DontGenerateDCF) ExportWordlistToDCF(filepath + ".dcf", true);
	} else if (!ReadWordlistDCF(filepath + ".dcf", true)) {
		ReadWordlistPlaintext(filepath + ".txt", true);
		ExportWordlistToDCF(filepath + ".dcf", true);
	}

	if (BinaryIO::DoesFileExist("blocklist.dcf")) {
		ReadWordlistDCF("blocklist.dcf", false);
	}

	//Read player names that are ok as well:
	auto approvedNames = Database::Get()->GetApprovedCharacterNames();
	for (auto& name : approvedNames) {
		std::transform(name.begin(), name.end(), name.begin(), ::tolower); //Transform to lowercase
		m_ApprovedWords.push_back(CalculateHash(name));
	}
}

dChatFilter::~dChatFilter() {
	m_ApprovedWords.clear();
	m_DeniedWords.clear();
}

void dChatFilter::ReadWordlistPlaintext(const std::string& filepath, bool allowList) {
	std::ifstream file(filepath);
	if (file) {
		std::string line;
		while (std::getline(file, line)) {
			line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
			std::transform(line.begin(), line.end(), line.begin(), ::tolower); //Transform to lowercase
			if (allowList) m_ApprovedWords.push_back(CalculateHash(line));
			else m_DeniedWords.push_back(CalculateHash(line));
		}
	}
}

bool dChatFilter::ReadWordlistDCF(const std::string& filepath, bool allowList) {
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
			if (allowList) m_ApprovedWords.reserve(wordsToRead);
			else m_DeniedWords.reserve(wordsToRead);

			size_t word = 0;
			for (size_t i = 0; i < wordsToRead; ++i) {
				BinaryIO::BinaryRead(file, word);
				if (allowList) m_ApprovedWords.push_back(word);
				else m_DeniedWords.push_back(word);
			}

			return true;
		} else {
			file.close();
			return false;
		}
	}

	return false;
}

void dChatFilter::ExportWordlistToDCF(const std::string& filepath, bool allowList) {
	std::ofstream file(filepath, std::ios::binary | std::ios_base::out);
	if (file) {
		BinaryIO::BinaryWrite(file, uint32_t(dChatFilterDCF::header));
		BinaryIO::BinaryWrite(file, uint32_t(dChatFilterDCF::formatVersion));
		BinaryIO::BinaryWrite(file, size_t(allowList ? m_ApprovedWords.size() : m_DeniedWords.size()));

		for (size_t word : allowList ? m_ApprovedWords : m_DeniedWords) {
			BinaryIO::BinaryWrite(file, word);
		}

		file.close();
	}
}

std::set<std::pair<uint8_t, uint8_t>> dChatFilter::IsSentenceOkay(const std::string& message, eGameMasterLevel gmLevel, bool allowList) {
	if (gmLevel > eGameMasterLevel::FORUM_MODERATOR) return { }; //If anything but a forum mod, return true.
	if (message.empty()) return { };
	if (!allowList && m_DeniedWords.empty()) return { { 0, message.length() } };

	std::stringstream sMessage(message);
	std::string segment;
	std::regex reg("(!*|\\?*|\\;*|\\.*|\\,*)");

	std::set<std::pair<uint8_t, uint8_t>> listOfBadSegments;

	uint32_t position = 0;

	while (std::getline(sMessage, segment, ' ')) {
		std::string originalSegment = segment;

		std::transform(segment.begin(), segment.end(), segment.begin(), ::tolower); //Transform to lowercase
		segment = std::regex_replace(segment, reg, "");

		size_t hash = CalculateHash(segment);

		if (std::find(m_UserUnapprovedWordCache.begin(), m_UserUnapprovedWordCache.end(), hash) != m_UserUnapprovedWordCache.end() && allowList) {
			listOfBadSegments.emplace(position, originalSegment.length());
		}

		if (std::find(m_ApprovedWords.begin(), m_ApprovedWords.end(), hash) == m_ApprovedWords.end() && allowList) {
			m_UserUnapprovedWordCache.push_back(hash);
			listOfBadSegments.emplace(position, originalSegment.length());
		}

		if (std::find(m_DeniedWords.begin(), m_DeniedWords.end(), hash) != m_DeniedWords.end() && !allowList) {
			m_UserUnapprovedWordCache.push_back(hash);
			listOfBadSegments.emplace(position, originalSegment.length());
		}

		position += originalSegment.length() + 1;
	}

	return listOfBadSegments;
}

size_t dChatFilter::CalculateHash(const std::string& word) {
	std::hash<std::string> hash{};

	size_t value = hash(word);

	return value;
}
