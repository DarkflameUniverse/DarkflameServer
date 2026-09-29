#ifndef __ICLIENTSYSINFO__H__
#define __ICLIENTSYSINFO__H__

#include <cstdint>
#include <string>
#include <vector>

/**
 * The system description each client sent with its login request, as reported by the client (see ClientSysInfo.h:
 * compatibility values from old Windows calls, not necessarily the real hardware). The auth server keeps one row per
 * account while the description stays the same, and starts a new row when it changes.
 */
class IClientSysInfo {
public:
	struct SysInfoRow {
		uint64_t id{};
		uint32_t accountId{};
		int64_t firstSeen{};
		int64_t lastSeen{};
		uint32_t logins{ 1 };
		std::string ip;                 // empty while log_login_addresses is off
		uint32_t clientOs{};            // the request's clientOS (1 Windows, 2 Mac)
		std::string memoryStats;        // the raw text of the newest login in this row
		uint64_t memoryTotalKb{};       // physical memory from memoryStats (0: not read)
		std::string videoCard;
		uint32_t numberOfProcessors{};
		uint32_t processorType{};
		uint16_t processorLevel{};
		uint16_t processorRevision{};
		uint32_t osVersionInfoSize{};
		uint32_t majorVersion{};
		uint32_t minorVersion{};
		uint32_t buildNumber{};
		uint32_t platformId{};

		// Whether two logins describe the same client (memory in use and the time are left out: they change each login)
		bool SameAs(const SysInfoRow& other) const {
			return ip == other.ip && clientOs == other.clientOs && memoryTotalKb == other.memoryTotalKb && videoCard == other.videoCard &&
				numberOfProcessors == other.numberOfProcessors && processorType == other.processorType && processorLevel == other.processorLevel &&
				processorRevision == other.processorRevision && osVersionInfoSize == other.osVersionInfoSize && majorVersion == other.majorVersion &&
				minorVersion == other.minorVersion && buildNumber == other.buildNumber && platformId == other.platformId;
		}
	};

	/**
	 * A successful login's description. If it is the same as the account's newest row, that row gets the new time, one
	 * more login and this login's memoryStats text; otherwise a new row is added.
	 */
	virtual void RecordClientSysInfo(const SysInfoRow& info) = 0;

	// An account's rows, newest first
	virtual std::vector<SysInfoRow> GetClientSysInfo(uint32_t accountId, uint32_t limit) = 0;

	// Each account's newest row, for the spread across players
	virtual std::vector<SysInfoRow> GetLatestClientSysInfo(uint32_t limit) = 0;
};

#endif  //!__ICLIENTSYSINFO__H__
