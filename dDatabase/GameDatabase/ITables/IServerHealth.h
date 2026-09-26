#ifndef __ISERVERHEALTH__H__
#define __ISERVERHEALTH__H__

#include <cstdint>
#include <vector>

/**
 * A sample of how the server is doing, taken by the dashboard every minute, for the health history.
 */
class IServerHealth {
public:
	struct HealthSample {
		int64_t time{};
		uint32_t players{};
		uint32_t worlds{};
		bool authOnline{};
		bool chatOnline{};
		uint64_t memoryKb{}; // all server processes together (Linux only, else 0)
	};

	virtual void InsertHealthSample(const HealthSample& sample) = 0;

	// Samples between from and to, averaged into buckets of `bucketSeconds`; players and worlds are the bucket's highest
	virtual std::vector<HealthSample> GetHealthSamples(int64_t from, int64_t to, int64_t bucketSeconds) = 0;

	// Also prunes the per-instance samples
	virtual uint32_t PruneHealthSamples(int64_t beforeTime) = 0;

	// Players in one world instance at a sample
	struct InstanceSample {
		int64_t time{};
		uint32_t zoneId{};
		uint32_t instanceId{};
		uint32_t cloneId{};
		uint32_t players{};
	};

	virtual void InsertInstanceSamples(const std::vector<InstanceSample>& samples) = 0;

	// Per bucket of `bucketSeconds` and instance, its highest player count; zoneId 0: every zone
	virtual std::vector<InstanceSample> GetInstanceSamples(int64_t from, int64_t to, int64_t bucketSeconds, uint32_t zoneId) = 0;
};

#endif  //!__ISERVERHEALTH__H__
