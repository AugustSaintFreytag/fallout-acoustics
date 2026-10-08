#pragma once

#include <Windows.h>
#include <mmsystem.h>  // WAVEFORMATEX. WIN32_LEAN_AND_MEAN removes it from Windows.h
#include <dsound.h>

#include <cstdint>

namespace sea::audio {

	bool IsDSoundObject(std::uint32_t address);

	bool Supports(IUnknown* object, REFIID iid);

	bool IsDSOALLoaded();

	std::uintptr_t DSoundBegin();
	std::uintptr_t DSoundEnd();

}
