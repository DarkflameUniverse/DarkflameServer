#include "dChatFilter.h"

#include <system_error>

#include "Logger.h"
#include "Database.h"
#include "Game.h"
#include "eGameMasterLevel.h"

using namespace dChatFilterDCF;

dChatFilter::dChatFilter(const std::string& filepath, bool dontGenerateDCF) {
	m_DontGenerateDCF = dontGenerateDCF;

	LoadAllowList(filepath);
	LoadBlockList();

	// Approved character names count as allowed words
	for (const auto& name : Database::Get()->GetApprovedCharacterNames()) {
		m_Lists.approved.hashes.insert(ChatFilterWords::Hash(ChatFilterWords::AsciiLower(name)));
	}

	ReloadCustomWords();
}

void dChatFilter::LoadAllowList(const std::string& filepath) {
	const std::string dcf = filepath + ".dcf";
	const std::string txt = filepath + ".txt";
	if (!m_DontGenerateDCF) {
		auto cached = ReadFile(dcf);
		if (cached.status == eStatus::OK) {
			m_Lists.approved = std::move(cached.list);
			return;
		}
		if (cached.status != eStatus::MISSING) LOG("%s is %s; building it again from %s", dcf.c_str(), StatusText(cached.status), txt.c_str());
	}

	const auto text = ReadBytes(txt);
	if (!text) {
		LOG("Could not read the chat filter's allowed words (%s)", txt.c_str());
		return;
	}
	m_Lists.approved = AllowListFromText(*text);
	if (!m_DontGenerateDCF && !WriteFile(dcf, m_Lists.approved)) LOG("Could not write %s", dcf.c_str());
}

void dChatFilter::LoadBlockList() {
	std::error_code error;
	const bool hasText = std::filesystem::exists(BLOCK_LIST_TEXT, error);
	if (hasText) {
		const auto text = ReadBytes(BLOCK_LIST_TEXT);
		const auto existing = ReadFile(BLOCK_LIST_FILE);
		// Rebuilt when the .dcf is missing, unreadable or older than the text
		bool stale = existing.status != eStatus::OK;
		if (!stale) {
			std::error_code textError, fileError;
			const auto textTime = std::filesystem::last_write_time(BLOCK_LIST_TEXT, textError);
			const auto fileTime = std::filesystem::last_write_time(BLOCK_LIST_FILE, fileError);
			stale = textError || fileError || fileTime < textTime;
		}
		if (text && (m_DontGenerateDCF || stale)) {
			m_Lists.denied = BlockListFromText(*text);
			if (m_DontGenerateDCF) {
				LOG("Loaded %zu blocked words and phrases from %s", m_Lists.denied.Size(), BLOCK_LIST_TEXT);
				return;
			}
			if (WriteFile(BLOCK_LIST_FILE, m_Lists.denied)) {
				LOG("Built %s from %s (%zu words and phrases)", BLOCK_LIST_FILE, BLOCK_LIST_TEXT, m_Lists.denied.Size());
			} else {
				LOG("Could not write %s", BLOCK_LIST_FILE);
			}
			return;
		}
	}

	auto blocked = ReadFile(BLOCK_LIST_FILE);
	switch (blocked.status) {
	case eStatus::OK:
		m_Lists.denied = std::move(blocked.list);
		break;
	case eStatus::MISSING:
		LOG("No %s: best friends' free chat stops every message. Put the blocked words in %s next to the servers (one word or phrase per line) and start the servers again.",
			BLOCK_LIST_FILE, BLOCK_LIST_TEXT);
		break;
	case eStatus::OLD_FORMAT:
		LOG("%s is in the old format (version 2), whose hashes depend on the compiler and platform, so it can't be read; best friends' free chat stops every message. "
			"Put the plain word list in %s next to the servers (one word or phrase per line) and start the servers again to rebuild it.",
			BLOCK_LIST_FILE, BLOCK_LIST_TEXT);
		break;
	default:
		LOG("%s is %s and can't be read; best friends' free chat stops every message. Rebuild it from %s.", BLOCK_LIST_FILE, StatusText(blocked.status), BLOCK_LIST_TEXT);
		break;
	}
}

void dChatFilter::ReloadCustomWords() {
	m_Lists.customAllowed = {};
	m_Lists.customBlocked = {};
	for (const auto& word : Database::Get()->GetChatFilterWords()) {
		(word.allowed ? m_Lists.customAllowed : m_Lists.customBlocked).AddEntry(ChatFilterWords::NormalizeEntry(word.word));
	}
}

std::set<std::pair<uint8_t, uint8_t>> dChatFilter::IsSentenceOkay(const std::string& message, eGameMasterLevel gmLevel, bool allowList) {
	if (gmLevel > eGameMasterLevel::FORUM_MODERATOR) return { }; //If anything but a forum mod, return true.
	return ChatFilterWords::CheckMessage(message, allowList, m_Lists);
}
