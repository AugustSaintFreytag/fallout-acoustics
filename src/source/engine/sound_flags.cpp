#include "engine/sound_flags.h"

#include <cstdio>

namespace sea::engine {
	namespace {
		struct FlagName {
			std::uint32_t bit;
			const char* name;
		};

		constexpr FlagName kSoundFlagNames[] = {
			{kSound_2D, "2D"},
			{kSound_3D, "3D"},
			{kSound_Voice, "Voice"},
			{kSound_Footsteps, "Foot"},
			{kSound_Loop, "Loop"},
			{kSound_SystemSound, "System"},
			{kSound_RandomFrequency, "RandFreq"},
			{kSound_Battle, "Battle"},
			{kSound_OneShot, "OneShot"},
			{kSound_CoverLight, "CoverLight"},
			{kSound_CoverFull, "CoverFull"},
			{kSound_Music, "Music"},
			{kSound_Region, "Region"},
			{kSound_MuteSubmerged, "MuteSubmerged?"},
			{kSound_Impact, "Impact"},
			{kSound_DontCache, "NoCache"},
			{kSound_2DGunfire, "2DGunfire"},
			{kSound_FirstPerson, "1stPerson"},
			{kSound_Modulated, "Modulated"},
			{kSound_Radio, "Radio"},
			{kSound_IgnoreTimescale, "NoTimescale"},
			{kSound_EnvelopeFast, "EnvFast"},
			{kSound_EnvelopeSlow, "EnvSlow"},
			{kSound_2DRadius, "2DRadius"},
			{kSound_AnimationDriven, "AnimDriven"},
		};

		const char* FlagSeparator(std::size_t usedLength) {
			if (usedLength == 0) {
				return "";
			}

			return "|";
		}
	}

	void DescribeSoundFlags(std::uint32_t flags, char* buffer, std::size_t size) {
		buffer[0] = '\0';

		std::size_t usedLength = 0;
		std::uint32_t knownFlags = 0;

		for (const auto& [bit, name] : kSoundFlagNames) {
			knownFlags |= bit;

			if (!(flags & bit)) {
				continue;
			}

			const int written = std::snprintf(buffer + usedLength, size - usedLength, "%s%s", FlagSeparator(usedLength), name);

			if (written < 0 || usedLength + written >= size) {
				return;
			}

			usedLength += written;
		}

		const std::uint32_t unknownFlags = flags & ~knownFlags;

		if (unknownFlags != 0) {
			std::snprintf(buffer + usedLength, size - usedLength, "%s0x%X", FlagSeparator(usedLength), unknownFlags);
		}
	}
}
