#pragma once
#include <vector>
#include <string>

#include "dCommonVars.h"

namespace dChatFilterDCF {
	static const uint32_t header = ('D' + ('C' << 8) + ('F' << 16) + ('B' << 24));
	static const uint32_t formatVersion = 2;

	struct fileHeader {
		uint32_t header;
		uint32_t formatVersion;
	};
};

class dChatFilter
{
public:
	dChatFilter(const std::string& filepath, bool dontGenerateDCF);
	~dChatFilter();

	void ReadWordlistPlaintext(const std::string& filepath);
	bool ReadWordlistDCF(const std::string& filepath, bool whiteList);
	void ExportWordlistToDCF(const std::string& filepath);
	std::vector<std::string> IsSentenceOkay(const std::string& message, int gmLevel, bool whiteList = true);

private:
	bool m_DontGenerateDCF;
	std::vector<size_t> m_NoNoWords;
	std::vector<size_t> m_YesYesWords;
	std::vector<size_t> m_UserUnapprovedWordCache;

	//Private functions:
	size_t CalculateHash(const std::string& word);
	bool IsInWordlist(size_t word, bool whiteList);
};