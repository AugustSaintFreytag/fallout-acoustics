#pragma once

namespace sea::log {
	void Open(const char* path);

	void Write(const char* format, ...);
}

#define SEA_LOG(...) ::sea::log::Write(__VA_ARGS__)
