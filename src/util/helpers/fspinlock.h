#pragma once

// minimal but efficient non-recursive spinlock implementation

#include <atomic>

class FSpinlock
{
public:
	bool is_locked() const
	{
		return m_lockBool.load(std::memory_order_relaxed);
	}

	// implement BasicLockable and Lockable
	void lock() const
	{
		while (true)
		{
			if (!m_lockBool.exchange(true, std::memory_order_acquire))
				break;
			while (m_lockBool.load(std::memory_order_relaxed))
			{
#if defined(__aarch64__)
				asm volatile("wfe"); // low-power wait until cache line event
#else
				_mm_pause();
#endif
			}
		}
	}

	bool try_lock() const
	{
		return !m_lockBool.exchange(true, std::memory_order_acquire);
	}

	void unlock() const
	{
		m_lockBool.store(false, std::memory_order_release);
#if defined(__aarch64__)
		asm volatile("sev"); // wake cores waiting in wfe
#endif
	}

private:

	mutable std::atomic<bool> m_lockBool = false;
};
