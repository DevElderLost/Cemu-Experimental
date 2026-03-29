#include "Common/precompiled.h"
#include "Common/cpu_affinity.h"

#if BOOST_PLAT_ANDROID

#include <sched.h>
#include <unistd.h>
#include <cerrno>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

namespace {

struct CoreInfo
{
	int coreIndex;
	long maxFreqKHz;
};

bool s_initialized = false;
std::vector<int> s_performanceCores;
std::vector<int> s_efficiencyCores;
std::vector<int> s_bigCoresOnly; // big cores excluding prime (for thermal optimization)

long ReadCoreMaxFreq(int coreIndex)
{
	std::string path = fmt::format("/sys/devices/system/cpu/cpu{}/cpufreq/cpuinfo_max_freq", coreIndex);
	std::ifstream file(path);
	if (!file.is_open())
		return -1;
	long freq = -1;
	file >> freq;
	return freq;
}

void SetCurrentThreadAffinityToCores(const std::vector<int>& cores)
{
	if (cores.empty())
		return;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	for (int core : cores)
		CPU_SET(core, &cpuset);
	// sched_setaffinity with pid 0 targets the calling thread
	int ret = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
	if (ret != 0)
		cemuLog_log(LogType::Force, "cpu_affinity: sched_setaffinity failed with error {}", errno);
}

} // anonymous namespace

#endif // BOOST_PLAT_ANDROID

namespace CpuAffinity
{

void InitializeCPUAffinityMapping()
{
#if BOOST_PLAT_ANDROID
	if (s_initialized)
		return;
	s_initialized = true;

	int numCores = sysconf(_SC_NPROCESSORS_CONF);
	if (numCores <= 0)
	{
		cemuLog_log(LogType::Force, "cpu_affinity: Failed to detect CPU core count");
		return;
	}

	std::vector<CoreInfo> cores;
	cores.reserve(numCores);
	for (int i = 0; i < numCores; i++)
	{
		long freq = ReadCoreMaxFreq(i);
		if (freq > 0)
			cores.push_back({i, freq});
		else
			cemuLog_log(LogType::Force, "cpu_affinity: Could not read max frequency for core {}", i);
	}

	if (cores.empty())
	{
		cemuLog_log(LogType::Force, "cpu_affinity: No core frequency data available, affinity pinning disabled");
		return;
	}

	// sort by frequency descending
	std::sort(cores.begin(), cores.end(), [](const CoreInfo& a, const CoreInfo& b) {
		return a.maxFreqKHz > b.maxFreqKHz;
	});

	// log detected topology
	for (const auto& c : cores)
		cemuLog_log(LogType::Force, "cpu_affinity: Core {} max freq {} KHz", c.coreIndex, c.maxFreqKHz);

	// Detect 3-tier topology: prime (fastest), big (mid), little (slowest)
	long maxFreq = cores.front().maxFreqKHz;
	long primeThreshold = maxFreq * 9 / 10; // within 10% of fastest = prime
	long bigThreshold = maxFreq * 3 / 4;    // within 25% of fastest = big (includes prime)

	std::vector<int> primeCores, bigCores, littleCores;
	for (const auto& c : cores)
	{
		if (c.maxFreqKHz >= primeThreshold)
			primeCores.push_back(c.coreIndex);
		if (c.maxFreqKHz >= bigThreshold)
			bigCores.push_back(c.coreIndex);
		else
			littleCores.push_back(c.coreIndex);
	}

	// Performance = all big+prime cores
	s_performanceCores = bigCores;
	s_efficiencyCores = littleCores;

	// Big-only = big cores excluding prime (for thermal optimization)
	// On 2-tier SoCs (e.g., Snapdragon 8 Elite: 2x Oryon@4.32GHz + 6x Oryon@3.53GHz),
	// this gives us the 6 lower-clocked cores which are ~80% as fast but much cooler
	for (int c : bigCores)
	{
		if (std::find(primeCores.begin(), primeCores.end(), c) == primeCores.end())
			s_bigCoresOnly.push_back(c);
	}
	// If no mid-tier exists (all cores same freq = symmetric SMP), use all cores
	if (s_bigCoresOnly.empty())
		s_bigCoresOnly = s_performanceCores;

	// ensure we have at least some performance cores (cap at 6 to leave room for OS/other tasks)
	if (s_performanceCores.size() > 6)
		s_performanceCores.resize(6);

	cemuLog_log(LogType::Force, "cpu_affinity: {} prime, {} big-only, {} little cores",
		primeCores.size(), s_bigCoresOnly.size(), s_efficiencyCores.size());

	std::string primeStr, bigStr, littleStr;
	for (int c : primeCores)
		primeStr += fmt::format("{} ", c);
	for (int c : s_bigCoresOnly)
		bigStr += fmt::format("{} ", c);
	for (int c : s_efficiencyCores)
		littleStr += fmt::format("{} ", c);
	cemuLog_log(LogType::Force, "cpu_affinity: Prime cores (excluded): [{}]", primeStr);
	cemuLog_log(LogType::Force, "cpu_affinity: Big cores (PPC+GPU target): [{}]", bigStr);
	if (!littleStr.empty())
		cemuLog_log(LogType::Force, "cpu_affinity: Little cores: [{}]", littleStr);
#endif
}

void PinThreadToPerformanceCores(std::thread& thread)
{
	// Note: on Android, affinity must be set from within the target thread itself
	// Use PinCurrentThreadToPerformanceCores() from inside the thread instead
	(void)thread;
}

void PinCurrentThreadToPerformanceCores()
{
#if BOOST_PLAT_ANDROID
	if (!s_initialized || s_performanceCores.empty())
		return;
	SetCurrentThreadAffinityToCores(s_performanceCores);
#endif
}

void PinThreadToEfficiencyCores(std::thread& thread)
{
	// Note: on Android, affinity must be set from within the target thread itself
	// Use PinCurrentThreadToEfficiencyCores() from inside the thread instead
	(void)thread;
}

void PinCurrentThreadToEfficiencyCores()
{
#if BOOST_PLAT_ANDROID
	if (!s_initialized || s_efficiencyCores.empty())
		return;
	SetCurrentThreadAffinityToCores(s_efficiencyCores);
#endif
}

void PinCurrentThreadToBigCoresOnly()
{
#if BOOST_PLAT_ANDROID
	if (!s_initialized || s_bigCoresOnly.empty())
		return;
	SetCurrentThreadAffinityToCores(s_bigCoresOnly);
#endif
}

} // namespace CpuAffinity
