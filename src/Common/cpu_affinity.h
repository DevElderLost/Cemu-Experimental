#pragma once

#include <thread>

namespace CpuAffinity
{
	void InitializeCPUAffinityMapping();
	void PinThreadToPerformanceCores(std::thread& thread);
	void PinCurrentThreadToPerformanceCores();
	void PinThreadToEfficiencyCores(std::thread& thread);
	void PinCurrentThreadToEfficiencyCores();
	// Pin to big cores only (exclude prime/fastest cores) for thermal optimization
	void PinCurrentThreadToBigCoresOnly();
}
