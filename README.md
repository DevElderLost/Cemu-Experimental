# Cemu Android Port - Odin 3 Build

Fork of [SSimco/Cemu](https://github.com/SSimco/Cemu) (Android port) with stability fixes, Android packaging changes, and ARM64-focused runtime improvements targeting the **Ayn Odin 3** (Snapdragon 8 Elite / Adreno 830). Changes are intended to stay device-safe rather than hardcoding Odin-only behavior.

For general information about Cemu, see the [official website](https://cemu.info) and [main repository](https://github.com/cemu-project/Cemu).

## Game Status (Odin 3)

| Game | Status | Notes |
|------|--------|-------|
| Twilight Princess HD (US) | Playable | Original heap crash fixed |
| Wind Waker HD (US) | Playable | More GPU/thermal intensive |

Thermals depend heavily on device clocks, fan profile, and external tools such as ClusterTune.

## Differences from SSimco / Upstream

### Android Packaging

**Two package IDs**

The default build uses `info.cemu.cemu.odin` so it can install alongside the classic Android port package. The release workflow also builds a classic package variant, `info.cemu.cemu`, for frontends such as Daijisho that expect the standard Cemu package name.

Set `ANDROID_APPLICATION_ID=info.cemu.cemu` to build the classic package locally. Leaving it unset builds the Odin package.

**Red launcher icon**

The launcher background was changed from blue to red so this fork is easy to distinguish from SSimco's build on-device.

### Crash Fixes

**Heap zero-initialization** (`coreinit_MEM_ExpHeap.cpp`)
Games like Twilight Princess HD rely on heap allocations being zero-initialized when reusing freed blocks. Added `memset(mem, 0, size)` after allocation in `MEMAllocFromExpHeapEx` to match Wii U hardware behavior, fixing a SIGSEGV crash in TPHD.

**Memory barrier instructions** (`PPCRecompilerImlGen.cpp`, `BackendAArch64.cpp`, etc.)
Added `PPCREC_IML_TYPE_MEMORY_BARRIER` IML instruction type that emits `dmb ish` on AArch64 for SYNC/ISYNC/EIEIO/DCBF PPC instructions. These were silently ignored before, which is safe on x86 (strong memory model) but incorrect on ARM64 (weak memory model). Also added TWI (trap word immediate) as a no-op.

### Android CPU Scheduling

Earlier test builds pinned emulation work away from the largest cores for thermal reasons. That was removed in `0.5.1`; current builds leave core selection to Android and external tuning tools. This avoids fighting tools like ClusterTune and avoids device-specific scheduler behavior.

### Thermal Load Reduction: Spin-Wait Replacement

Several hot loops used CPU spin-waits (`_mm_pause()`, tight `yield()` loops) that kept cores active while waiting for events. This fork replaces the worst Android-visible idle waits with sleep/yield patterns:

| Location | Current behavior |
|----------|------------------|
| PPC scheduler idle | Sleeps when no runnable PPC threads exist |
| VSync driver (non-Windows) | Timer-based 60Hz callback instead of an unimplemented stub |
| GPU async/fence/flip waits | Sleep/yield instead of pure busy-spin |
| TCL ring buffer and GPU semaphore waits | Short yield phase, then brief sleep |

*Files: `coreinit_Thread.cpp`, `VsyncDriver.cpp`, `LatteAsyncCommands.cpp`, `TCL.cpp`, `LatteCommandProcessor.cpp`, `LatteThread.cpp`*

### ARM64 WFE/SEV Spinlocks

Replaces selected spinlock `yield()` loops with ARM64 `wfe` (wait for event) / `sev` (send event), allowing waiting cores to enter a lower-power wait state until signaled.

### Profiling Support

Adds diagnostic counters/logging for Android performance analysis: FPS, thermal-zone reading, GPU stage timings, render-pass break reasons, layout transitions, texture readbacks, and memory upload stats.

*Files: `LattePerformanceMonitor.cpp`, `LattePerformanceMonitor.h`, plus instrumentation in Vulkan renderer files*

### Cherry-picked Upstream Fixes

Selected fixes from [cemu-project/Cemu](https://github.com/cemu-project/Cemu) have been cherry-picked onto the Android port, including:

- `coreinit: Add and use MEMAllocFromDefaultHeapEx`
- `coreinit: Zero-initialize SysAllocators`
- `coreinit: Track memory allocation size for MEMFreeToDefaultHeap`
- `Vulkan: Skip zero-size readback buffer barriers`
- `Input: Fix race condition in button mapping access`
- `RPL: Remove incorrect ref count check`
- `coreinit: Stub MCP_DemoGetRemainder to 99`
- `coreinit: Implement OSDynLoad_IsModuleLoaded`
- `coreinit: Always try to print symbols for PPC stack traces`
- `Latte: Rework interval tree for vertex/uniform cache`
- `Latte: Fix rare corruption in buffer cache`
- `GX2+Latte: Rework GX2CopySurface`
- `PPCRec: Cleanup and smaller fixes`
- `PPCAsm` string/condition-register/reloc parsing fixes
- Vulkan transform-feedback and vertex-attribute cleanup

## License

Cemu is licensed under [Mozilla Public License 2.0](/LICENSE.txt). Exempt from this are all files in the dependencies directory for which the licenses of the original code apply as well as some individual files in the src folder, as specified in those file headers respectively.
