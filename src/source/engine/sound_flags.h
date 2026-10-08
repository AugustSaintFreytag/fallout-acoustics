#pragma once

#include <cstddef>
#include <cstdint>

namespace sea::engine {

	enum SoundFlag : std::uint32_t {  // (JG BSGameSound::TypeFlags)
		kSound_2D = 1u << 0,
		kSound_3D = 1u << 1,
		kSound_Voice = 1u << 2,
		kSound_Footsteps = 1u << 3,
		kSound_Loop = 1u << 4,
		kSound_SystemSound = 1u << 5,
		kSound_RandomFrequency = 1u << 6,
		kSound_Battle = 1u << 7,
		kSound_OneShot = 1u << 8,
		kSound_CoverLight = 1u << 9,  // Plugin tag on voices (JG UNKBIT9, not used by the engine)
		kSound_CoverFull = 1u << 10,  // Plugin tag on voices (JG UNKBIT10, not used by the engine)
		kSound_Music = 1u << 11,
		kSound_Region = 1u << 12,
		kSound_MuteSubmerged = 1u << 13,  // JG "MAYBE_UNDERWATER", "Mute when submerged" flag
		kSound_Impact = 1u << 14,
		kSound_DontCache = 1u << 16,
		kSound_2DGunfire = 1u << 17,
		kSound_FirstPerson = 1u << 18,
		kSound_Modulated = 1u << 19,
		kSound_Radio = 1u << 20,
		kSound_IgnoreTimescale = 1u << 21,  // Seen on player foley
		kSound_EnvelopeFast = 1u << 25,
		kSound_EnvelopeSlow = 1u << 26,
		kSound_2DRadius = 1u << 27,
		kSound_AnimationDriven = 1u << 30,  // Footsteps, etc., triggered from KF files
	};

	void DescribeSoundFlags(std::uint32_t flags, char* buffer, std::size_t size);

}
