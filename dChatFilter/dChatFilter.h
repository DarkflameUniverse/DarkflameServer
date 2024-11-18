#pragma once
#include <vector>
#include <string>

#include "dCommonVars.h"

enum class eGameMasterLevel : uint8_t;
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

	void ReadWordlistPlaintext(const std::string& filepath, bool allowList);
	bool ReadWordlistDCF(const std::string& filepath, bool allowList);
	void ExportWordlistToDCF(const std::string& filepath, bool allowList);
	std::vector<std::pair<uint8_t, uint8_t>> IsSentenceOkay(const std::string& message, eGameMasterLevel gmLevel, bool allowList = true);

private:
	bool m_DontGenerateDCF;
	std::vector<size_t> m_DeniedWords;
	std::vector<size_t> m_ApprovedWords;
	std::vector<size_t> m_UserUnapprovedWordCache;

	//Private functions:
	size_t CalculateHash(const std::string& word);
};
