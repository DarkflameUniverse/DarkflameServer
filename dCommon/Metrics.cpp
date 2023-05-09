#include "Metrics.hpp"

#include <chrono>

std::unordered_map<MetricVariable, Metric*> Metrics::m_Metrics = {};
std::vector<MetricVariable> Metrics::m_Variables = {
	MetricVariable::GameLoop,
	MetricVariable::PacketHandling,
	MetricVariable::UpdateEntities,
	MetricVariable::UpdateSpawners,
	MetricVariable::Physics,
	MetricVariable::UpdateReplica,
	MetricVariable::Ghosting,
	MetricVariable::CPUTime,
	MetricVariable::Sleep,
	MetricVariable::Frame,
};

void Metrics::AddMeasurement(MetricVariable variable, int64_t value) {
	const auto& iter = m_Metrics.find(variable);

	Metric* metric;

	if (iter == m_Metrics.end()) {
		metric = new Metric();

		m_Metrics[variable] = metric;
	} else {
		metric = iter->second;
	}

	AddMeasurement(metric, value);
}

void Metrics::AddMeasurement(Metric* metric, int64_t value) {
	const auto index = metric->measurementIndex;

	metric->measurements[index] = value;

	if (metric->max == -1 || value > metric->max) {
		metric->max = value;
	} else if (metric->min == -1 || metric->min > value) {
		metric->min = value;
	}

	if (metric->measurementSize < MAX_MEASURMENT_POINTS) {
		metric->measurementSize++;
	}

	metric->measurementIndex = (index + 1) % MAX_MEASURMENT_POINTS;
}

const Metric* Metrics::GetMetric(MetricVariable variable) {
	const auto& iter = m_Metrics.find(variable);

	if (iter == m_Metrics.end()) {
		return nullptr;
	}

	Metric* metric = iter->second;

	int64_t average = 0;

	for (size_t i = 0; i < metric->measurementSize; i++) {
		average += metric->measurements[i];
	}

	average /= metric->measurementSize;

	metric->average = average;

	return metric;
}

void Metrics::StartMeasurement(MetricVariable variable) {
	const auto& iter = m_Metrics.find(variable);

	Metric* metric;

	if (iter == m_Metrics.end()) {
		metric = new Metric();

		m_Metrics[variable] = metric;
	} else {
		metric = iter->second;
	}

	metric->activeMeasurement = std::chrono::high_resolution_clock::now();
}

void Metrics::EndMeasurement(MetricVariable variable) {
	const auto end = std::chrono::high_resolution_clock::now();

	const auto& iter = m_Metrics.find(variable);

	if (iter == m_Metrics.end()) {
		return;
	}

	Metric* metric = iter->second;

	const auto elapsed = end - metric->activeMeasurement;

	const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();

	AddMeasurement(metric, nanoseconds);
}

float Metrics::ToMiliseconds(int64_t nanoseconds) {
	return (float)nanoseconds / 1e6;
}

std::string Metrics::MetricVariableToString(MetricVariable variable) {
	switch (variable) {
	case MetricVariable::GameLoop:
		return "GameLoop";
	case MetricVariable::PacketHandling:
		return "PacketHandling";
	case MetricVariable::UpdateEntities:
		return "UpdateEntities";
	case MetricVariable::UpdateSpawners:
		return "UpdateSpawners";
	case MetricVariable::Physics:
		return "Physics";
	case MetricVariable::UpdateReplica:
		return "UpdateReplica";
	case MetricVariable::Sleep:
		return "Sleep";
	case MetricVariable::CPUTime:
		return "CPUTime";
	case MetricVariable::Frame:
		return "Frame";
	case MetricVariable::Ghosting:
		return "Ghosting";

	default:
		return "Invalid";
	}
}

const std::vector<MetricVariable>& Metrics::GetAllMetrics() {
	return m_Variables;
}

void Metrics::Clear() {
	for (const auto& pair : m_Metrics) {
		delete pair.second;
	}

	m_Metrics.clear();
}

/* RSS Memory utilities
 *
 * Author:  David Robert Nadeau
 * Site:    http://NadeauSoftware.com/
 * License: Creative Commons Attribution 3.0 Unported License
 *          http://creativecommons.org/licenses/by/3.0/deed.en_US
 */


#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#include <sys/resource.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/mach.h>

#elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
#include <fcntl.h>
#include <procfs.h>

#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
#include <stdio.h>

#endif

#else
#error "Cannot define getPeakRSS( ) or getCurrentRSS( ) for an unknown OS."
#endif

 /**
  * Returns the peak (maximum so far) resident set size (physical
  * memory use) measured in bytes, or zero if the value cannot be
  * determined on this OS.
  */
size_t Metrics::GetPeakRSS() {
#if defined(_WIN32)
	/* Windows -------------------------------------------------- */
	PROCESS_MEMORY_COUNTERS info;
	GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
	return (size_t)info.PeakWorkingSetSize;

#elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
	/* AIX and Solaris ------------------------------------------ */
	struct psinfo psinfo;
	int fd = -1;
	if ((fd = open("/proc/self/psinfo", O_RDONLY)) == -1)
		return (size_t)0L;      /* Can't open? */
	if (read(fd, &psinfo, sizeof(psinfo)) != sizeof(psinfo)) {
		close(fd);
		return (size_t)0L;      /* Can't read? */
	}
	close(fd);
	return (size_t)(psinfo.pr_rssize * 1024L);

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
	/* BSD, Linux, and OSX -------------------------------------- */
	struct rusage rusage;
	getrusage(RUSAGE_SELF, &rusage);
#if defined(__APPLE__) && defined(__MACH__)
	return (size_t)rusage.ru_maxrss;
#else
	return (size_t)(rusage.ru_maxrss * 1024L);
#endif

#else
	/* Unknown OS ----------------------------------------------- */
	return (size_t)0L;          /* Unsupported. */
#endif

}

/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
size_t Metrics::GetCurrentRSS() {
#if defined(_WIN32)
	/* Windows -------------------------------------------------- */
	PROCESS_MEMORY_COUNTERS info;
	GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
	return (size_t)info.WorkingSetSize;

#elif defined(__APPLE__) && defined(__MACH__)
	/* OSX ------------------------------------------------------ */
	struct mach_task_basic_info info;
	mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
		(task_info_t)&info, &infoCount) != KERN_SUCCESS)
		return (size_t)0L;      /* Can't access? */
	return (size_t)info.resident_size;

#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
	/* Linux ---------------------------------------------------- */
	long rss = 0L;
	FILE* fp = NULL;
	if ((fp = fopen("/proc/self/statm", "r")) == NULL)
		return (size_t)0L;      /* Can't open? */
	if (fscanf(fp, "%*s%ld", &rss) != 1) {
		fclose(fp);
		return (size_t)0L;      /* Can't read? */
	}
	fclose(fp);
	return (size_t)rss * (size_t)sysconf(_SC_PAGESIZE);

#else
	/* AIX, BSD, Solaris, and Unknown OS ------------------------ */
	return (size_t)0L;          /* Unsupported. */
#endif

}

size_t Metrics::GetProcessID() {
#if defined(_WIN32)
	return GetCurrentProcessId();
#else
	return getpid();
#endif
}
