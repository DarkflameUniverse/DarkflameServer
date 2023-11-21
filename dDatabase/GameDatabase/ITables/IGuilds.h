#ifndef __IGUILDS__H__
#define __IGUILDS__H__

#include <cstdint>
#include <optional>
#include <string>

class IGuilds {
public:
	struct Guild {
		uint32_t id;
		uint32_t owner_id;
		std::string name;
		std::string motd;
		int64_t reputation;
	};
	virtual std::optional<Guild> CreateGuild(const std::string_view name, const int32_t owner_id, const uint64_t reputation) = 0;
	virtual std::optional<Guild> GetGuild(const uint32_t guild_id) = 0;
	virtual std::optional<Guild> GetGuildByName(const std::string_view name) = 0;
	virtual bool CheckGuildNameExists(const std::string_view name) = 0;
	virtual void SetMOTD(const uint32_t guild_id, const std::string_view motd) = 0;
	virtual void DeleteGuild(const uint32_t guild_id) = 0;
};

#endif  //!__IGUILDS__H__
