#pragma once

#include "dCommonVars.h"
#include <vector>
#include <map>
#include <unordered_map>
#include <chrono>

#define MAX_MEASURMENT_POINTS 1024

enum class MetricVariable : int32_t
{
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

struct Metric
{
	int64_t measurements[MAX_MEASURMENT_POINTS] = {};
	size_t measurementIndex = 0;
	size_t measurementSize = 0;
	int64_t max = -1;
	int64_t min = -1;
	int64_t average = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> activeMeasurement;
};

class Metrics
{
public:
	~Metrics();

	static void AddMeasurement(MetricVariable variable, int64_t value);
	static void AddMeasurement(Metric* metric, int64_t value);
	static const Metric* GetMetric(MetricVariable variable);
	static void StartMeasurement(MetricVariable variable);
	static void EndMeasurement(MetricVariable variable);
	static float ToMiliseconds(int64_t nanoseconds);
	static std::string MetricVariableToString(MetricVariable variable);
	static const std::vector<MetricVariable>& GetAllMetrics();

	static size_t GetPeakRSS();
	static size_t GetCurrentRSS();
	static size_t GetProcessID();

	static void Clear();

private:
	Metrics();

	static std::unordered_map<MetricVariable, Metric*> m_Metrics;
	static std::vector<MetricVariable> m_Variables;
};
