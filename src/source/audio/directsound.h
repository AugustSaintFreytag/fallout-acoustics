#pragma once

#include <Windows.h>
#include <mmsystem.h>  // WAVEFORMATEX, removed by WIN32_LEAN_AND_MEAN from Windows.h
#include <dsound.h>

#include <cstdint>

namespace sea::audio {
	// Pre-check function to check if an object is a DirectSound object.
	//
	// Checks if vtable of `address` is in dsound.dll.
	// Returns true for DirectSound and DSOAL objects.
	bool IsDSoundObject(std::uint32_t address);

	bool Supports(IUnknown* object, REFIID iid);

	// True if DSOAL replaces DirectSound (its OpenAL driver `dsoal-aldrv.dll` is loaded).
	bool IsDsoalLoaded();

	std::uintptr_t DSoundBegin();
	std::uintptr_t DSoundEnd();
}
