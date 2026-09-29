#ifndef GUILDNAMERULES_H
#define GUILDNAMERULES_H

#include <cstddef>
#include <string>

// What a guild name may be (docs/Guilds.md), for the chat server and the dashboard
namespace GuildNameRules {
	constexpr size_t MIN_LENGTH = 3;
	constexpr size_t MAX_LENGTH = 30; // the client sends at most 30 characters

	// MIN_LENGTH to MAX_LENGTH of A-Z a-z 0-9, space, ' - . with no space at either end and no two in a row
	inline bool IsValid(const std::u16string& name) {
		if (name.size() < MIN_LENGTH || name.size() > MAX_LENGTH) return false;
		if (name.front() == u' ' || name.back() == u' ') return false;
		for (size_t i = 0; i < name.size(); i++) {
			const char16_t c = name[i];
			const bool allowed = (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || (c >= u'0' && c <= u'9') ||
				c == u' ' || c == u'\'' || c == u'-' || c == u'.';
			if (!allowed) return false;
			if (c == u' ' && i > 0 && name[i - 1] == u' ') return false;
		}
		return true;
	}
}

#endif // GUILDNAMERULES_H
