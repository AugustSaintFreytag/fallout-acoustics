#include "utils/threads.h"

#include <Windows.h>

#include <atomic>

namespace sea::threads {
	namespace {
		std::atomic<std::uint32_t> g_mainThreadId{0};
		std::atomic<std::uint32_t> g_audioThreadId{0};
	}

	// Stores the calling thread as the main thread. 
	// Called from `NVSEPlugin_Load` and on each main loop message.
	//
	// Thread: Main
	void RememberMainThread() {
		g_mainThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);
	}

	// Thread: Any
	bool IsMainThread() {
		return GetCurrentThreadId() == g_mainThreadId.load(std::memory_order_relaxed);
	}

	// Stores the calling thread as the audio thread. 
	// Called from `BSWin32GameSound::Play`, which always runs there.
	//
	// Thread: Audio
	void RememberAudioThread() {
		g_audioThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);
	}

	// Checks if the calling thread is the audio thread. 
	// Evaluates to `false` until first sound plays.
	//
	// Thread: Any
	bool IsAudioThread() {
		return GetCurrentThreadId() == g_audioThreadId.load(std::memory_order_relaxed);
	}

	std::uint32_t MainThreadId() {
		return g_mainThreadId.load(std::memory_order_relaxed);
	}

	std::uint32_t AudioThreadId() {
		return g_audioThreadId.load(std::memory_order_relaxed);
	}
}
