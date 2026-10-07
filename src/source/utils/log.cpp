#include "utils/log.h"

#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace sea::log {
	namespace {
		std::mutex g_lock;
		std::FILE* g_file = nullptr;
		ULONGLONG g_startTime = 0;
	}

	void Open(const char* path) {
		std::lock_guard guard(g_lock);

		if (g_file) {
			return;
		}

		g_file = _fsopen(path, "w", _SH_DENYWR);
		g_startTime = GetTickCount64();
	}

	void Write(const char* format, ...) {
		char line[1024];
		va_list arguments;
		va_start(arguments, format);
		std::vsnprintf(line, sizeof(line), format, arguments);
		va_end(arguments);

		std::lock_guard guard(g_lock);

		if (!g_file) {
			return;
		}

		std::fprintf(g_file, "%8llu [%05lu] %s\n", GetTickCount64() - g_startTime, GetCurrentThreadId(), line);
		std::fflush(g_file);
	}
}
