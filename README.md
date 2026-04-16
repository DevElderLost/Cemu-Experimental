# Cemu Android Port - Odin 3 Optimizations

Fork of [SSimco/Cemu](https://github.com/SSimco/Cemu) (Android port) with stability fixes and performance/thermal optimizations targeting the **Ayn Odin 3** (Snapdragon 8 Elite / Adreno 830), though all changes are designed to work safely on any device.

For general information about Cemu, see the [official website](https://cemu.info) and [main repository](https://github.com/cemu-project/Cemu).

## Game Status (Odin 3)

| Game | Status | Thermal |
|------|--------|---------|
| Twilight Princess HD (US) | Playable | ~45-52C |
| Wind Waker HD (US) | Playable | ~57-60C |

## Changes from Upstream

### Android Packaging

**Application ID suffix** (`src/android/app/build.gradle.kts`)
Changed the Android `applicationId` to `info.cemu.cemu.odin` so this build can be installed alongside SSimco's `info.cemu.cemu` APK without package/signature conflicts during testing and release validation.

### Crash Fixes

**Heap zero-initialization** (`coreinit_MEM_ExpHeap.cpp`)
Games like Twilight Princess HD rely on heap allocations being zero-initialized when reusing freed blocks. Added `memset(mem, 0, size)` after allocation in `MEMAllocFromExpHeapEx` to match Wii U hardware behavior, fixing a SIGSEGV crash in TPHD.

**Memory barrier instructions** (`PPCRecompilerImlGen.cpp`, `BackendAArch64.cpp`, etc.)
Added `PPCREC_IML_TYPE_MEMORY_BARRIER` IML instruction type that emits `dmb ish` on AArch64 for SYNC/ISYNC/EIEIO/DCBF PPC instructions. These were silently ignored before, which is safe on x86 (strong memory model) but incorrect on ARM64 (weak memory model). Also added TWI (trap word immediate) as a no-op.

### CPU Core Affinity Pinning (Android)

**Problem:** The Snapdragon 8 Elite has 2 prime cores (4.32GHz) and 6 big cores (3.53GHz). When the OS scheduler places emulation threads on prime cores, they generate significantly more heat with no measurable FPS improvement (the emulation workload doesn't benefit from the extra ~20% clock speed).

**Solution:** Added a CPU affinity system that reads each core's max frequency from `/sys/devices/system/cpu/cpuN/cpufreq/cpuinfo_max_freq` at runtime and classifies cores into prime/big/little tiers using frequency thresholds relative to the fastest core. PPC emulation threads and the GPU thread are pinned to big cores, excluding prime cores.

**Auto-adapts** to any SoC topology: 3-tier (prime/big/little), 2-tier (prime/big), or symmetric (uses all cores). No hardcoded core IDs or device-specific logic.

**Impact:** ~10C reduction in SoC temperature with no FPS loss.

*Files: `cpu_affinity.cpp`, `cpu_affinity.h`, `CafeSystem.cpp`, `LatteThread.cpp`, `coreinit_Thread.cpp`, `CMakeLists.txt`*

### Thermal Load Reduction: Spin-Wait Replacement

**Problem:** Multiple hot loops across the emulator used CPU spin-waits (`_mm_pause()`, tight `yield()` loops) that kept cores at 100% utilization even when waiting for events.

**Solution:** Replaced spin-waits with proper sleep/yield patterns across the codebase:

| Location | Before | After |
|----------|--------|-------|
| PPC scheduler idle | Tight poll loop | 1ms sleep when no runnable threads |
| VSync driver (non-Windows) | Unimplemented stub | Timer-based 60Hz sleep/wake |
| GPU async command wait | `_mm_pause()` | 1ms sleep |
| TCL ring buffer wait | `yield()` | Yield 10x then 100us sleep |
| GPU command buffer idle | 80x `_mm_pause()` | 1ms sleep |
| GPU fence wait | Tight poll | 1ms sleep between polls |
| GPU semaphore wait | Yield 100x, 1ms sleep | Yield 10x, 100us sleep |
| HLE flip wait | `_mm_pause()` + yield | 1ms sleep |

*Files: `coreinit_Thread.cpp`, `VsyncDriver.cpp`, `LatteAsyncCommands.cpp`, `TCL.cpp`, `LatteCommandProcessor.cpp`, `LatteThread.cpp`*

### ARM64 SIMD Texture Decoding

Uses NEON SIMD intrinsics and optimized `memcpy` for texture tile reordering on AArch64, replacing the generic scalar C++ paths.

### ARM64 WFE/SEV Spinlocks

Replaces emulated Wii U OS spinlock `yield()` loops with ARM64 `wfe` (wait for event) / `sev` (signal event) instructions, which put the core into a low-power wait state until signaled instead of busy-spinning.

### Performance Monitoring

Added per-second diagnostic logging for profiling: FPS, SoC temperature, GPU stage timings (draw API, uniforms, index, vertex, MRT, texture decode), render pass break reasons (clear, depth clear, texture load, surface copy, submit, query, FBO change), layout transition counts, memory upload stats, and CPU/GPU-bound classification.

*Files: `LattePerformanceMonitor.cpp`, `LattePerformanceMonitor.h`, plus instrumentation in Vulkan renderer files*

### Cherry-picked Upstream Fixes

The following fixes were cherry-picked from [cemu-project/Cemu](https://github.com/cemu-project/Cemu):

- `coreinit: Add and use MEMAllocFromDefaultHeapEx` - allocation alignment
- `coreinit: Zero-initialize SysAllocators` - stability fix
- `coreinit: Track memory allocation size for MEMFreeToDefaultHeap`

## License

Cemu is licensed under [Mozilla Public License 2.0](/LICENSE.txt). Exempt from this are all files in the dependencies directory for which the licenses of the original code apply as well as some individual files in the src folder, as specified in those file headers respectively.
