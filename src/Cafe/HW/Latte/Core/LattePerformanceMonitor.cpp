#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"
#include "Cafe/HW/Latte/Core/LatteOverlay.h"
#include "Cemu/Logging/CemuLogging.h"
#include "WindowSystem.h"

#if BOOST_OS_LINUX || BOOST_OS_ANDROID
#include <fstream>
#include <string>

// Read SoC temperature from Android thermal zones
// Returns temperature in millidegrees C, or -1 if unavailable
static sint32 readThermalTemp()
{
	// Try common thermal zone names for Snapdragon SoCs
	static const char* thermalPaths[] = {
		"/sys/class/thermal/thermal_zone0/temp",  // CPU/SoC on most devices
		"/sys/class/thermal/thermal_zone1/temp",
		"/sys/class/thermal/thermal_zone2/temp",
		"/sys/class/thermal/thermal_zone3/temp",
		"/sys/class/thermal/thermal_zone10/temp", // GPU on some Snapdragon
	};
	static int cachedIndex = -2; // -2 = not yet searched

	if (cachedIndex == -1)
		return -1; // previously failed to find any

	if (cachedIndex >= 0)
	{
		std::ifstream f(thermalPaths[cachedIndex]);
		sint32 temp = -1;
		if (f >> temp)
			return temp;
		return -1;
	}

	// First call: find a working thermal zone
	for (int i = 0; i < (int)(sizeof(thermalPaths)/sizeof(thermalPaths[0])); i++)
	{
		std::ifstream f(thermalPaths[i]);
		sint32 temp = -1;
		if (f >> temp && temp > 0)
		{
			cachedIndex = i;
			return temp;
		}
	}
	cachedIndex = -1;
	return -1;
}
#else
static sint32 readThermalTemp() { return -1; }
#endif

performanceMonitor_t performanceMonitor{};

void LattePerformanceMonitor_frameEnd()
{
	// per-frame stats
	performanceMonitor.gpuTime_shaderCreate.frameFinished();
	performanceMonitor.gpuTime_frameTime.frameFinished();
	performanceMonitor.gpuTime_idleTime.frameFinished();
	performanceMonitor.gpuTime_fenceTime.frameFinished();

	performanceMonitor.gpuTime_dcStageTextures.frameFinished();
	performanceMonitor.gpuTime_dcStageVertexMgr.frameFinished();
	performanceMonitor.gpuTime_dcStageShaderAndUniformMgr.frameFinished();
	performanceMonitor.gpuTime_dcStageIndexMgr.frameFinished();
	performanceMonitor.gpuTime_dcStageMRT.frameFinished();
	performanceMonitor.gpuTime_dcStageDrawcallAPI.frameFinished();
	performanceMonitor.gpuTime_waitForAsync.frameFinished();
	performanceMonitor.gpuTime_textureDecode.frameFinished();

	uint32 elapsedTime = GetTickCount() - performanceMonitor.cycle[performanceMonitor.cycleIndex].lastUpdate;
	if (elapsedTime >= 1000)
	{
		bool isFirstUpdate = performanceMonitor.cycle[performanceMonitor.cycleIndex].lastUpdate == 0;
		// sum up raw stats
		uint32 totalElapsedTime = GetTickCount() - performanceMonitor.cycle[(performanceMonitor.cycleIndex + 1) % PERFORMANCE_MONITOR_TRACK_CYCLES].lastUpdate;
		uint32 totalElapsedTimeFPS = GetTickCount() - performanceMonitor.cycle[(performanceMonitor.cycleIndex + PERFORMANCE_MONITOR_TRACK_CYCLES - 1) % PERFORMANCE_MONITOR_TRACK_CYCLES].lastUpdate;
		uint32 elapsedFrames = 0;
		uint32 elapsedFrames2S = 0; // elapsed frames for last two entries (seconds)
		uint64 skippedCycles = 0;
		uint64 vertexDataUploaded = 0;
		uint64 vertexDataCached = 0;
		uint64 uniformBankUploadedData = 0;
		uint64 uniformBankUploadedCount = 0;
		uint64 indexDataUploaded = 0;
		uint64 indexDataCached = 0;
		uint32 frameCounter = 0;
		uint32 drawCallCounter = 0;
		uint32 fastDrawCallCounter = 0;
		uint32 shaderBindCounter = 0;
		uint32 recompilerLeaveCount = 0;
		uint32 threadLeaveCount = 0;
		for (sint32 i = 0; i < PERFORMANCE_MONITOR_TRACK_CYCLES; i++)
		{
			elapsedFrames += performanceMonitor.cycle[i].frameCounter;
			skippedCycles += performanceMonitor.cycle[i].skippedCycles;
			vertexDataUploaded += performanceMonitor.cycle[i].vertexDataUploaded;
			vertexDataCached += performanceMonitor.cycle[i].vertexDataCached;
			uniformBankUploadedData += performanceMonitor.cycle[i].uniformBankUploadedData;
			uniformBankUploadedCount += performanceMonitor.cycle[i].uniformBankUploadedCount;
			indexDataUploaded += performanceMonitor.cycle[i].indexDataUploaded;
			indexDataCached += performanceMonitor.cycle[i].indexDataCached;
			frameCounter += performanceMonitor.cycle[i].frameCounter;
			drawCallCounter += performanceMonitor.cycle[i].drawCallCounter;
			fastDrawCallCounter += performanceMonitor.cycle[i].fastDrawCallCounter;
			shaderBindCounter += performanceMonitor.cycle[i].shaderBindCount;
			recompilerLeaveCount += performanceMonitor.cycle[i].recompilerLeaveCount;
			threadLeaveCount += performanceMonitor.cycle[i].threadLeaveCount;
		}
		elapsedFrames = std::max<uint32>(elapsedFrames, 1);
		elapsedFrames2S = performanceMonitor.cycle[(performanceMonitor.cycleIndex + PERFORMANCE_MONITOR_TRACK_CYCLES - 0) % PERFORMANCE_MONITOR_TRACK_CYCLES].frameCounter;
		elapsedFrames2S += performanceMonitor.cycle[(performanceMonitor.cycleIndex + PERFORMANCE_MONITOR_TRACK_CYCLES - 1) % PERFORMANCE_MONITOR_TRACK_CYCLES].frameCounter;
		elapsedFrames2S = std::max<uint32>(elapsedFrames2S, 1);
		// calculate stats
		uint64 passedCycles = PPCInterpreter_getMainCoreCycleCounter() - performanceMonitor.cycle[(performanceMonitor.cycleIndex + 1) % PERFORMANCE_MONITOR_TRACK_CYCLES].lastCycleCount;
		passedCycles -= skippedCycles;
		uint64 vertexDataUploadPerFrame = (vertexDataUploaded / (uint64)elapsedFrames);
		vertexDataUploadPerFrame /= 1024ULL;
		uint64 vertexDataCachedPerFrame = (vertexDataCached / (uint64)elapsedFrames);
		vertexDataCachedPerFrame /= 1024ULL;
		uint64 uniformBankDataUploadedPerFrame = (uniformBankUploadedData / (uint64)elapsedFrames);
		uniformBankDataUploadedPerFrame /= 1024ULL;
		uint32 uniformBankCountUploadedPerFrame = (uint32)(uniformBankUploadedCount / (uint64)elapsedFrames);
		uint64 indexDataUploadPerFrame = (indexDataUploaded / (uint64)elapsedFrames);

		double fps = (double)elapsedFrames2S * 1000.0 / (double)totalElapsedTimeFPS;
		uint32 shaderBindsPerFrame = shaderBindCounter / elapsedFrames;
		passedCycles = passedCycles * 1000ULL / totalElapsedTime;
		uint32 rlps = (uint32)((uint64)recompilerLeaveCount * 1000ULL / (uint64)totalElapsedTime);
		uint32 tlps = (uint32)((uint64)threadLeaveCount * 1000ULL / (uint64)totalElapsedTime);
		// set stats
		performanceMonitor.stats.indexDataUploadPerFrame = indexDataUploadPerFrame;
		// next counter cycle
		sint32 nextCycleIndex = (performanceMonitor.cycleIndex + 1) % PERFORMANCE_MONITOR_TRACK_CYCLES;
		performanceMonitor.cycle[nextCycleIndex].drawCallCounter = 0;
		performanceMonitor.cycle[nextCycleIndex].fastDrawCallCounter = 0;
		performanceMonitor.cycle[nextCycleIndex].frameCounter = 0;
		performanceMonitor.cycle[nextCycleIndex].shaderBindCount = 0;
		performanceMonitor.cycle[nextCycleIndex].lastCycleCount = PPCInterpreter_getMainCoreCycleCounter();
		performanceMonitor.cycle[nextCycleIndex].skippedCycles = 0;
		performanceMonitor.cycle[nextCycleIndex].vertexDataUploaded = 0;
		performanceMonitor.cycle[nextCycleIndex].vertexDataCached = 0;
		performanceMonitor.cycle[nextCycleIndex].uniformBankUploadedData = 0;
		performanceMonitor.cycle[nextCycleIndex].uniformBankUploadedCount = 0;
		performanceMonitor.cycle[nextCycleIndex].indexDataUploaded = 0;
		performanceMonitor.cycle[nextCycleIndex].indexDataCached = 0;
		performanceMonitor.cycle[nextCycleIndex].recompilerLeaveCount = 0;
		performanceMonitor.cycle[nextCycleIndex].threadLeaveCount = 0;
		performanceMonitor.cycleIndex = nextCycleIndex;

		// next update in 1 second
		performanceMonitor.cycle[performanceMonitor.cycleIndex].lastUpdate = GetTickCount();

		if (isFirstUpdate)
		{
			LatteOverlay_updateStats(0.0, 0, 0);
			WindowSystem::UpdateWindowTitles(false, false, 0.0);
		}
		else
		{
			LatteOverlay_updateStats(fps, drawCallCounter / elapsedFrames, fastDrawCallCounter / elapsedFrames);
			WindowSystem::UpdateWindowTitles(false, false, fps);

			// Comprehensive performance diagnostics (per-second snapshot)
			uint64 frameTimeUs = PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_frameTime.getPreviousFrameValue());
			uint64 idleTimeUs = PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_idleTime.getPreviousFrameValue());
			uint64 fenceTimeUs = PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_fenceTime.getPreviousFrameValue());
			uint64 asyncWaitUs = PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_waitForAsync.getPreviousFrameValue());
			uint64 texStageUs = PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_dcStageTextures.getPreviousFrameValue());
			uint64 drawApiUs = PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_dcStageDrawcallAPI.getPreviousFrameValue());
			uint64 texDecodeUs = PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_textureDecode.getPreviousFrameValue());
			uint64 uniformUs = PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_dcStageShaderAndUniformMgr.getPreviousFrameValue());
			uint64 indexUs = PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_dcStageIndexMgr.getPreviousFrameValue());
			uint64 vertexUs = PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_dcStageVertexMgr.getPreviousFrameValue());
			uint64 mrtUs = PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_dcStageMRT.getPreviousFrameValue());
			uint64 shaderCreateUs = PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_shaderCreate.getPreviousFrameValue());

			// Bottleneck classification
			uint64 gpuCmdTimeUs = texStageUs + drawApiUs + uniformUs + indexUs + vertexUs + mrtUs;
			uint64 gpuWaitTimeUs = idleTimeUs + fenceTimeUs + asyncWaitUs;
			double idlePct = (frameTimeUs > 0) ? (100.0 * idleTimeUs / frameTimeUs) : 0.0;
			double gpuCmdPct = (frameTimeUs > 0) ? (100.0 * gpuCmdTimeUs / frameTimeUs) : 0.0;

			// Thermal reading
			sint32 thermalRaw = readThermalTemp();
			double thermalC = (thermalRaw > 0) ? (thermalRaw / 1000.0) : -1.0;

			// Memory upload stats
			uint64 vtxUploadKB = vertexDataUploadPerFrame;
			uint64 vtxCachedKB = vertexDataCachedPerFrame;
			uint64 uniformUploadKB = uniformBankDataUploadedPerFrame;
			uint32 uniformUploadCount = uniformBankCountUploadedPerFrame;

			cemuLog_log(LogType::Force,
				"[PERF] FPS:{:.1f} Temp:{:.1f}C FrameT:{}us Idle:{}us({:.0f}%) Fence:{}us AsyncWait:{}us ShaderCreate:{}us",
				fps, thermalC, frameTimeUs, idleTimeUs, idlePct, fenceTimeUs, asyncWaitUs, shaderCreateUs);
			cemuLog_log(LogType::Force,
				"[PERF-GPU] DrawAPI:{}us Uniform:{}us Index:{}us Vertex:{}us MRT:{}us TexStage:{}us TexDecode:{}us GpuCmd:{:.0f}%",
				drawApiUs, uniformUs, indexUs, vertexUs, mrtUs, texStageUs, texDecodeUs, gpuCmdPct);
			cemuLog_log(LogType::Force,
				"[PERF-VK] DC:{} Barriers:{} RP:{} LT:{} StoreLT:{} SmallRP:{} RedundRP:{} SelfDep:{} Readbacks:{}",
				drawCallCounter / elapsedFrames,
				performanceMonitor.vk.numDrawBarriersPerFrame.get(),
				performanceMonitor.vk.numBeginRenderpassPerFrame.get(),
				performanceMonitor.vk.numLayoutTransitionsPerFrame.get(),
				performanceMonitor.vk.numStoreTransitionsPerFrame.get(),
				performanceMonitor.vk.numSmallRenderPassPerFrame.get(),
				performanceMonitor.vk.numRedundantRPBreaksPerFrame.get(),
				performanceMonitor.vk.numSelfDepRenderPassPerFrame.get(),
				performanceMonitor.vk.numTextureReadbacksPerFrame.get());
			cemuLog_log(LogType::Force,
				"[PERF-MEM] VtxUpload:{}KB VtxCached:{}KB UniformUpload:{}KB(x{}) IndexUpload:{}",
				vtxUploadKB, vtxCachedKB, uniformUploadKB, uniformUploadCount, indexDataUploadPerFrame);
			cemuLog_log(LogType::Force,
				"[PERF-RPBRK] Clear:{} DepthClear:{} TexLoad/BufUpload:{} SurfCopy:{} Submit:{} Query:{} FBOChange:{} Other:{}",
				performanceMonitor.vk.numRPBreak_clearSlice.get(),
				performanceMonitor.vk.numRPBreak_clearDepth.get(),
				performanceMonitor.vk.numRPBreak_texLoad.get(),
				performanceMonitor.vk.numRPBreak_surfaceCopy.get(),
				performanceMonitor.vk.numRPBreak_submitCB.get(),
				performanceMonitor.vk.numRPBreak_query.get(),
				performanceMonitor.vk.numRPBreak_drawSetRP.get(),
				performanceMonitor.vk.numRPBreak_other.get());
		}
	}
}

void LattePerformanceMonitor_frameBegin()
{
	performanceMonitor.vk.numDrawBarriersPerFrame.reset();
	performanceMonitor.vk.numBeginRenderpassPerFrame.reset();
	performanceMonitor.vk.numLayoutTransitionsPerFrame.reset();
	performanceMonitor.vk.numSelfDepRenderPassPerFrame.reset();
	performanceMonitor.vk.numOptimalRenderPassPerFrame.reset();
	performanceMonitor.vk.numTextureReadbacksPerFrame.reset();
	performanceMonitor.vk.numStoreTransitionsPerFrame.reset();
	performanceMonitor.vk.numSmallRenderPassPerFrame.reset();
	performanceMonitor.vk.numRedundantRPBreaksPerFrame.reset();
	performanceMonitor.vk.numRPBreak_clearSlice.reset();
	performanceMonitor.vk.numRPBreak_clearDepth.reset();
	performanceMonitor.vk.numRPBreak_texLoad.reset();
	performanceMonitor.vk.numRPBreak_surfaceCopy.reset();
	performanceMonitor.vk.numRPBreak_submitCB.reset();
	performanceMonitor.vk.numRPBreak_query.reset();
	performanceMonitor.vk.numRPBreak_readback.reset();
	performanceMonitor.vk.numRPBreak_drawSetRP.reset();
	performanceMonitor.vk.numRPBreak_other.reset();
}
