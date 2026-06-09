#pragma once

#include "dCommonVars.h"
#include <vector>
#include <map>
#include <string_view>
#include <unordered_map>
#include <chrono>

#define MAX_MEASURMENT_POINTS 1024

enum class MetricVariable : int32_t {
	GameLoop,
	PacketHandling,
	UpdateEntities,
	UpdateSpawners,
	Physics,
	UpdateReplica,
	Ghosting,
	CPUTime,
	Sleep,
	Frame,
};

struct Metric {
	int64_t measurements[MAX_MEASURMENT_POINTS] = {};
	size_t measurementIndex = 0;
	size_t measurementSize = 0;
	int64_t max = -1;
	int64_t min = -1;
	int64_t average = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> activeMeasurement;
};

namespace Metrics {
	void AddMeasurement(MetricVariable variable, int64_t value);
	void AddMeasurement(Metric& metric, int64_t value);
	const Metric& GetMetric(MetricVariable variable);
	void StartMeasurement(MetricVariable variable);
	void EndMeasurement(MetricVariable variable);
	float ToMiliseconds(int64_t nanoseconds);
	const std::string_view MetricVariableToString(MetricVariable variable);
	const std::vector<MetricVariable>& GetAllMetrics();

	size_t GetPeakRSS();
	size_t GetCurrentRSS();
	size_t GetProcessID();
};
