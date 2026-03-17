#if BOOST_OS_WINDOWS

#include <Windows.h>

#include "WindowSystem.h"

typedef LONG NTSTATUS;

typedef UINT32 D3DKMT_HANDLE;
typedef UINT D3DDDI_VIDEO_PRESENT_SOURCE_ID;

typedef struct _D3DKMT_OPENADAPTERFROMHDC
{
	HDC                            hDc;
	D3DKMT_HANDLE                  hAdapter;
	LUID                           AdapterLuid;
	D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
}D3DKMT_OPENADAPTERFROMHDC;

typedef struct _D3DKMT_WAITFORVERTICALBLANKEVENT {
	D3DKMT_HANDLE                  hAdapter;
	D3DKMT_HANDLE                  hDevice;
	D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
} D3DKMT_WAITFORVERTICALBLANKEVENT;

class DeviceVsyncHandler
{
public:
	DeviceVsyncHandler(void(*cbVSync)()) : m_vsyncDriverVSyncCb(cbVSync)
	{
		m_shutdownThread = false;
		if (!pfnD3DKMTOpenAdapterFromHdc)
		{
			HMODULE hModuleGDI = LoadLibraryA("gdi32.dll");
			*(void**)&pfnD3DKMTOpenAdapterFromHdc = GetProcAddress(hModuleGDI, "D3DKMTOpenAdapterFromHdc");
			*(void**)&pfnD3DKMTWaitForVerticalBlankEvent = GetProcAddress(hModuleGDI, "D3DKMTWaitForVerticalBlankEvent");
		}
		m_thd = std::thread(&DeviceVsyncHandler::vsyncThread, this);
	}

	~DeviceVsyncHandler()
	{
		m_shutdownThread = true;
		cemu_assert_debug(m_thd.joinable());
		m_thd.join();
	}

	void notifyWindowPosChanged()
	{
		m_checkMonitorChange = true;
	}

private:
	bool HasMonitorChanged()
	{
		HWND hWnd = (HWND)WindowSystem::GetWindowInfo().canvas_main.surface.load();
		if (hWnd == 0)
			return true;
		HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

		MONITORINFOEXW monitorInfo{};
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (GetMonitorInfoW(hMonitor, &monitorInfo) == 0)
			return true;

		if (wcscmp(monitorInfo.szDevice, m_activeMonitorDevice) == 0)
			return false;
		
		return true;
	}

	HRESULT GetAdapterHandleFromHwnd(D3DKMT_HANDLE* phAdapter, UINT* pOutput)
	{
		HWND hWnd = (HWND)WindowSystem::GetWindowInfo().canvas_main.surface.load();
		if (hWnd == 0)
			return E_FAIL;

		wcsncpy(m_activeMonitorDevice, L"", 32); // reset remembered monitor device
		m_checkMonitorChange = false;

		D3DKMT_OPENADAPTERFROMHDC OpenAdapterData;

		*phAdapter = 0;
		*pOutput = 0;

		HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

		MONITORINFOEXW monitorInfo{};
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (GetMonitorInfoW(hMonitor, &monitorInfo) == 0)
			return E_FAIL;


		HDC hdc = CreateDCW(NULL, monitorInfo.szDevice, NULL, NULL);
		if (hdc == NULL) {
			return E_FAIL;
		}

		OpenAdapterData.hDc = hdc;
		if (pfnD3DKMTOpenAdapterFromHdc(&OpenAdapterData) == 0)
		{
			DeleteDC(hdc);
			*phAdapter = OpenAdapterData.hAdapter;
			*pOutput = OpenAdapterData.VidPnSourceId;
			// remember monitor device
			wcsncpy(m_activeMonitorDevice, monitorInfo.szDevice, 32);
			return S_OK;
		}
		DeleteDC(hdc);

		return E_FAIL;
	}

	void vsyncThread()
	{
		D3DKMT_HANDLE hAdapter;
		UINT hOutput;
		GetAdapterHandleFromHwnd(&hAdapter, &hOutput);

		int failCount = 0;
		while (!m_shutdownThread)
		{
			D3DKMT_WAITFORVERTICALBLANKEVENT arg;
			arg.hDevice = 0;
			arg.hAdapter = hAdapter;
			arg.VidPnSourceId = hOutput;
			NTSTATUS r = pfnD3DKMTWaitForVerticalBlankEvent(&arg);
			if (r != 0)
			{
				//cemuLog_log(LogType::Force, "Wait for VerticalBlank failed");
				Sleep(1000 / 60);
				failCount++;
				if (failCount >= 10)
				{
					while (GetAdapterHandleFromHwnd(&hAdapter, &hOutput) != S_OK)
					{
						Sleep(1000 / 60);
						if (m_shutdownThread)
							return;
					}
					failCount = 0;
				}
			}
			else
				signalVsync();
			if (m_checkMonitorChange)
			{
				m_checkMonitorChange = false;
				if (HasMonitorChanged())
				{
					while (GetAdapterHandleFromHwnd(&hAdapter, &hOutput) != S_OK)
					{
						Sleep(1000 / 60);
						if (m_shutdownThread)
							return;
					}
				}
			}
		}
	}

	void signalVsync()
	{
		if(m_vsyncDriverVSyncCb)
			m_vsyncDriverVSyncCb();
	}

	void setCallback(void(*cbVSync)())
	{
		m_vsyncDriverVSyncCb = cbVSync;
	}

private:
	NTSTATUS(__stdcall* pfnD3DKMTOpenAdapterFromHdc)(D3DKMT_OPENADAPTERFROMHDC* Arg1) = nullptr;
	NTSTATUS(__stdcall* pfnD3DKMTWaitForVerticalBlankEvent)(const D3DKMT_WAITFORVERTICALBLANKEVENT* Arg1) = nullptr;

	std::thread m_thd;
	bool m_shutdownThread;
	bool m_checkMonitorChange{};
	WCHAR m_activeMonitorDevice[32];

	void (*m_vsyncDriverVSyncCb)() = nullptr;
};

DeviceVsyncHandler* s_vsyncDriver = nullptr;

std::mutex s_driverAccess;

void VsyncDriver_startThread(void(*cbVSync)())
{
	std::unique_lock<std::mutex> ul(s_driverAccess);
	if (!s_vsyncDriver)
		s_vsyncDriver = new DeviceVsyncHandler(cbVSync);
}

void VsyncDriver_notifyWindowPosChanged()
{
	std::unique_lock<std::mutex> ul(s_driverAccess);
	if (s_vsyncDriver)
		s_vsyncDriver->notifyWindowPosChanged();
}

#else

#include <thread>
#include <chrono>
#include <atomic>

class TimerVsyncHandler
{
public:
	TimerVsyncHandler(void(*cbVSync)()) : m_vsyncCb(cbVSync)
	{
		m_shutdown.store(false);
		m_thd = std::thread(&TimerVsyncHandler::vsyncThread, this);
	}

	~TimerVsyncHandler()
	{
		m_shutdown.store(true);
		if (m_thd.joinable())
			m_thd.join();
	}

private:
	void vsyncThread()
	{
		using namespace std::chrono;
		// ~60 Hz VSync interval (16.667ms)
		constexpr auto vsyncInterval = microseconds(16667);
		auto nextVsync = steady_clock::now() + vsyncInterval;

		while (!m_shutdown.load(std::memory_order_relaxed))
		{
			std::this_thread::sleep_until(nextVsync);
			if (m_vsyncCb)
				m_vsyncCb();
			nextVsync += vsyncInterval;
			// catch up if we fell behind
			auto now = steady_clock::now();
			if (nextVsync < now)
				nextVsync = now + vsyncInterval;
		}
	}

	std::thread m_thd;
	std::atomic<bool> m_shutdown;
	void (*m_vsyncCb)() = nullptr;
};

TimerVsyncHandler* s_vsyncDriver = nullptr;
std::mutex s_driverAccess;

void VsyncDriver_startThread(void(*cbVSync)())
{
	std::unique_lock<std::mutex> ul(s_driverAccess);
	if (!s_vsyncDriver)
		s_vsyncDriver = new TimerVsyncHandler(cbVSync);
}

void VsyncDriver_notifyWindowPosChanged()
{
	// no-op on non-Windows platforms
}

#endif
