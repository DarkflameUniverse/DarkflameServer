#ifndef __IGUILDMEMBERS__H__
#define __IGUILDMEMBERS__H__

#include <cstdint>
#include <optional>

class IGuildMembers {
public:
	struct GuildMember {
		uint32_t id;
		uint16_t rank;
	};
	virtual void InsertGuildMember(const uint32_t guild_id, const uint32_t member_id, const uint16_t rank = 4) = 0;
	virtual void DeleteGuildMember(const uint32_t member_id) = 0;
	virtual uint32_t GetMembersGuild(const uint32_t member_id) = 0;
	virtual std::vector<GuildMember> GetGuildMembers(const uint32_t guild_id) = 0;
	virtual bool CheckIsInGuild(const uint32_t guild_id, const uint32_t character_id) = 0;
};

#endif  //!__IGUILDMEMBERS__H__
