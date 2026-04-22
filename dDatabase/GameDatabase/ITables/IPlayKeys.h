#ifndef __IPLAYKEYS__H__
#define __IPLAYKEYS__H__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class IPlayKeys {
public:
	struct Info {
		uint32_t id{};
		std::string key_string;
		uint32_t key_uses{};
		uint32_t times_used{};
		bool active{};
		std::string notes;
		uint64_t created_at{};
	};

	// Get the playkey id for the given playkey.
	// Optional of bool may seem pointless, however the optional indicates if the playkey exists
	// and the bool indicates if the playkey is active.
	virtual std::optional<bool> IsPlaykeyActive(const int32_t playkeyId) = 0;

	// Dashboard methods
	virtual std::vector<Info> GetAllPlayKeys() = 0;
	virtual std::optional<Info> GetPlayKeyById(const uint32_t playkeyId) = 0;
	// Find a playkey by its string value (e.g. "ABCD-EFGH-...."). Returns Info if found.
	virtual std::optional<Info> GetPlayKeyByString(const std::string_view key_string) = 0;
	// Consume one usage of the given playkey id. Returns true if consumed, false if no uses left or not active.
	virtual bool ConsumePlayKeyUsage(const uint32_t playkeyId) = 0;
	virtual void CreatePlayKey(const std::string_view key_string, uint32_t uses, const std::string_view notes) = 0;
	virtual void UpdatePlayKey(const uint32_t playkeyId, uint32_t uses, bool active, const std::string_view notes) = 0;
	virtual void DeletePlayKey(const uint32_t playkeyId) = 0;
	virtual uint32_t GetPlayKeyCount() = 0;
};

#endif  //!__IPLAYKEYS__H__
